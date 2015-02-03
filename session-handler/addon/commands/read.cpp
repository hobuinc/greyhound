#include "pdal-session.hpp"
#include "buffer-pool.hpp"
#include "read-query.hpp"

#include "commands/read.hpp"

using namespace v8;

namespace
{
    bool isInteger(const Local<Value>& value)
    {
        return value->IsInt32() || value->IsUint32();
    }

    bool isDefined(const Local<Value>& value)
    {
        return !value->IsUndefined();
    }
}

void errorCallback(
        Persistent<Function> callback,
        std::string errMsg)
{
    HandleScope scope;

    const unsigned argc = 1;
    Local<Value> argv[argc] =
        {
            Local<Value>::New(String::New(errMsg.data(), errMsg.size()))
        };

    callback->Call(Context::GetCurrent()->Global(), argc, argv);

    // Dispose of the persistent handle so the callback may be garbage
    // collected.
    callback.Dispose();

    scope.Close(Undefined());
}

ReadCommand::ReadCommand(
        std::shared_ptr<PdalSession> pdalSession,
        ItcBufferPool& itcBufferPool,
        const std::string readId,
        const std::string host,
        const std::size_t port,
        const bool compress,
        const Schema& schema,
        v8::Persistent<v8::Function> queryCallback,
        v8::Persistent<v8::Function> dataCallback)
    : m_pdalSession(pdalSession)
    , m_itcBufferPool(itcBufferPool)
    , m_itcBuffer()
    , m_async(new uv_async_t)
    , m_readId(readId)
    , m_host(host)
    , m_port(port)
    , m_compress(compress)
    , m_schema(schemaOrDefault(schema))
    , m_numSent(0)
    , m_queryCallback(queryCallback)
    , m_dataCallback(dataCallback)
    , m_cancel(false)
    , m_errMsg()
{ }

ReadCommand::~ReadCommand()
{
    m_queryCallback.Dispose();
    m_dataCallback.Dispose();

    if (m_async)
    {
        uv_close((uv_handle_t*)m_async, NULL);
    }
}

std::size_t ReadCommand::numBytes() const
{
    return numPoints() * m_schema.stride(rasterize());
}

void ReadCommand::cancel(bool cancel)
{
    m_cancel = cancel;
}

std::string& ReadCommand::errMsg()
{
    return m_errMsg;
}

std::shared_ptr<ItcBuffer> ReadCommand::getBuffer()
{
    return m_itcBuffer;
}

ItcBufferPool& ReadCommand::getBufferPool()
{
    return m_itcBufferPool;
}

bool ReadCommand::done() const
{
    return m_readQuery->done();
}

void ReadCommand::run()
{
    query();
}

void ReadCommand::acquire()
{
    m_itcBuffer = m_itcBufferPool.acquire();
}

void ReadCommand::read(std::size_t maxNumBytes)
{
    m_readQuery->read(m_itcBuffer, maxNumBytes);
}

void ReadCommandRastered::read(std::size_t maxNumBytes)
{
    m_readQuery->read(m_itcBuffer, maxNumBytes, true);
}

std::size_t ReadCommand::numPoints() const
{
    return m_readQuery->numPoints();
}

std::string ReadCommand::readId() const
{
    return m_readId;
}

bool ReadCommand::cancel() const
{
    return m_cancel;
}

v8::Persistent<v8::Function> ReadCommand::queryCallback() const
{
    return m_queryCallback;
}

v8::Persistent<v8::Function> ReadCommand::dataCallback() const
{
    return m_dataCallback;
}

ReadCommandUnindexed::ReadCommandUnindexed(
        std::shared_ptr<PdalSession> pdalSession,
        ItcBufferPool& itcBufferPool,
        std::string readId,
        std::string host,
        std::size_t port,
        bool compress,
        Schema schema,
        std::size_t start,
        std::size_t count,
        v8::Persistent<v8::Function> queryCallback,
        v8::Persistent<v8::Function> dataCallback)
    : ReadCommand(
            pdalSession,
            itcBufferPool,
            readId,
            host,
            port,
            compress,
            schema,
            queryCallback,
            dataCallback)
    , m_start(start)
    , m_count(count)
{ }

ReadCommandPointRadius::ReadCommandPointRadius(
        std::shared_ptr<PdalSession> pdalSession,
        ItcBufferPool& itcBufferPool,
        std::string readId,
        std::string host,
        std::size_t port,
        bool compress,
        Schema schema,
        bool is3d,
        double radius,
        double x,
        double y,
        double z,
        v8::Persistent<v8::Function> queryCallback,
        v8::Persistent<v8::Function> dataCallback)
    : ReadCommand(
            pdalSession,
            itcBufferPool,
            readId,
            host,
            port,
            compress,
            schema,
            queryCallback,
            dataCallback)
    , m_is3d(is3d)
    , m_radius(radius)
    , m_x(x)
    , m_y(y)
    , m_z(z)
{ }

ReadCommandQuadIndex::ReadCommandQuadIndex(
        std::shared_ptr<PdalSession> pdalSession,
        ItcBufferPool& itcBufferPool,
        std::string readId,
        std::string host,
        std::size_t port,
        bool compress,
        Schema schema,
        std::size_t depthBegin,
        std::size_t depthEnd,
        v8::Persistent<v8::Function> queryCallback,
        v8::Persistent<v8::Function> dataCallback)
    : ReadCommand(
            pdalSession,
            itcBufferPool,
            readId,
            host,
            port,
            compress,
            schema,
            queryCallback,
            dataCallback)
    , m_depthBegin(depthBegin)
    , m_depthEnd(depthEnd)
{ }

ReadCommandBoundedQuadIndex::ReadCommandBoundedQuadIndex(
        std::shared_ptr<PdalSession> pdalSession,
        ItcBufferPool& itcBufferPool,
        std::string readId,
        std::string host,
        std::size_t port,
        bool compress,
        Schema schema,
        BBox bbox,
        std::size_t depthBegin,
        std::size_t depthEnd,
        v8::Persistent<v8::Function> queryCallback,
        v8::Persistent<v8::Function> dataCallback)
    : ReadCommandQuadIndex(
            pdalSession,
            itcBufferPool,
            readId,
            host,
            port,
            compress,
            schema,
            depthBegin,
            depthEnd,
            queryCallback,
            dataCallback)
    , m_bbox(bbox)
{ }

ReadCommandRastered::ReadCommandRastered(
        std::shared_ptr<PdalSession> pdalSession,
        ItcBufferPool& itcBufferPool,
        const std::string readId,
        const std::string host,
        const std::size_t port,
        bool compress,
        const Schema schema,
        v8::Persistent<v8::Function> queryCallback,
        v8::Persistent<v8::Function> dataCallback)
    : ReadCommand(
            pdalSession,
            itcBufferPool,
            readId,
            host,
            port,
            compress,
            schema,
            queryCallback,
            dataCallback)
    , m_rasterMeta()
{ }

ReadCommandRastered::ReadCommandRastered(
        std::shared_ptr<PdalSession> pdalSession,
        ItcBufferPool& itcBufferPool,
        const std::string readId,
        const std::string host,
        const std::size_t port,
        bool compress,
        const Schema schema,
        const RasterMeta rasterMeta,
        v8::Persistent<v8::Function> queryCallback,
        v8::Persistent<v8::Function> dataCallback)
    : ReadCommand(
            pdalSession,
            itcBufferPool,
            readId,
            host,
            port,
            compress,
            schema,
            queryCallback,
            dataCallback)
    , m_rasterMeta(rasterMeta)
{ }

ReadCommandQuadLevel::ReadCommandQuadLevel(
        std::shared_ptr<PdalSession> pdalSession,
        ItcBufferPool& itcBufferPool,
        std::string readId,
        std::string host,
        std::size_t port,
        bool compress,
        Schema schema,
        std::size_t level,
        v8::Persistent<v8::Function> queryCallback,
        v8::Persistent<v8::Function> dataCallback)
    : ReadCommandRastered(
            pdalSession,
            itcBufferPool,
            readId,
            host,
            port,
            compress,
            schema,
            queryCallback,
            dataCallback)
    , m_level(level)
{ }

Schema ReadCommand::schemaOrDefault(const Schema reqSchema)
{
    // If no schema supplied, stream all dimensions in their native format.
    if (reqSchema.dims.size() > 0)
    {
        return reqSchema;
    }
    else
    {
        std::vector<DimInfo> dims;

        const pdal::PointContext& pointContext(m_pdalSession->pointContext());

        const pdal::Dimension::IdList& idList(pointContext.dims());

        for (const auto& id : idList)
        {
            dims.push_back(DimInfo(id, pointContext.dimType(id)));
        }

        return Schema(dims);
    }
}

void ReadCommandUnindexed::query()
{
    m_readQuery = m_pdalSession->queryUnindexed(
            m_schema,
            m_compress,
            m_start,
            m_count);
}

void ReadCommandQuadIndex::query()
{
    m_readQuery = m_pdalSession->query(
            m_schema,
            m_compress,
            m_depthBegin,
            m_depthEnd);
}

void ReadCommandBoundedQuadIndex::query()
{
    m_readQuery = m_pdalSession->query(
            m_schema,
            m_compress,
            m_bbox,
            m_depthBegin,
            m_depthEnd);
}

void ReadCommandRastered::query()
{
    m_readQuery = m_pdalSession->query(
            m_schema,
            m_compress,
            m_rasterMeta);
}

void ReadCommandQuadLevel::query()
{
    m_readQuery = m_pdalSession->query(
            m_schema,
            m_compress,
            m_level,
            m_rasterMeta);
}

void ReadCommandPointRadius::query()
{
    m_readQuery = m_pdalSession->query(
            m_schema,
            m_compress,
            m_is3d,
            m_radius,
            m_x,
            m_y,
            m_z);
}

ReadCommand* ReadCommandFactory::create(
        std::shared_ptr<PdalSession> pdalSession,
        ItcBufferPool& itcBufferPool,
        const std::string readId,
        const Arguments& args)
{
    ReadCommand* readCommand(0);

    const std::size_t numArgs(args.Length());

    if (
        numArgs < 5 ||
        !args[numArgs - 2]->IsFunction() ||
        !args[numArgs - 1]->IsFunction())
    {
        // If no callback is supplied there's nothing we can do here.
        throw std::runtime_error("Invalid callback supplied to 'read'");
    }

    Persistent<Function> queryCallback(
        Persistent<Function>::New(
            Local<Function>::Cast(args[numArgs - 2])));

    Persistent<Function> dataCallback(
        Persistent<Function>::New(
            Local<Function>::Cast(args[numArgs - 1])));

    // Validate host, port, compress, and schemaRequest.
    if (
        args.Length() > 5 &&
        isDefined(args[0]) && args[0]->IsString() &&
        isDefined(args[1]) && isInteger(args[1]) &&
        isDefined(args[2]) && args[2]->IsBoolean() &&
        isDefined(args[3]) && args[3]->IsArray())
    {
        const std::string host(*v8::String::Utf8Value(args[0]->ToString()));
        const std::size_t port(args[1]->Uint32Value());
        const bool compress(args[2]->BooleanValue());

        std::vector<DimInfo> dims;

        // Unwrap the schema request into native C++ types.
        const Local<Array> schemaArray(Array::Cast(*args[3]));

        for (std::size_t i(0); i < schemaArray->Length(); ++i)
        {
            Local<Object> dimObj(schemaArray->Get(
                        Integer::New(i))->ToObject());

            const std::string sizeString(*v8::String::Utf8Value(
                    dimObj->Get(String::New("size"))->ToString()));

            const std::size_t size(strtoul(sizeString.c_str(), 0, 0));

            if (size)
            {
                const std::string name(*v8::String::Utf8Value(
                        dimObj->Get(String::New("name"))->ToString()));

                const std::string type(*v8::String::Utf8Value(
                        dimObj->Get(String::New("type"))->ToString()));

                if (pdalSession->pointContext().hasDim(
                            pdal::Dimension::id(name)))
                {
                    dims.push_back(DimInfo(name, type, size));
                }
            }
        }

        Schema schema(dims);

        // Unindexed read - starting offset and count supplied.
        if (
            args.Length() == 8 &&
            isDefined(args[4]) && isInteger(args[4]) &&
            isDefined(args[5]) && isInteger(args[5]))
        {
            const std::size_t start(args[4]->Uint32Value());
            const std::size_t count(args[5]->Uint32Value());

            if (start < pdalSession->getNumPoints())
            {
                readCommand = new ReadCommandUnindexed(
                        pdalSession,
                        itcBufferPool,
                        readId,
                        host,
                        port,
                        compress,
                        schema,
                        start,
                        count,
                        queryCallback,
                        dataCallback);
            }
            else
            {
                errorCallback(
                        queryCallback,
                        "Invalid 'start' in 'read' request");
            }
        }
        // KD-indexed read - point/radius supplied.
        else if (
            args.Length() == 11 &&
            isDefined(args[4]) && args[4]->IsBoolean() &&
            isDefined(args[5]) && args[5]->IsNumber() &&
            isDefined(args[6]) && args[6]->IsNumber() &&
            isDefined(args[7]) && args[7]->IsNumber() &&
            isDefined(args[8]) && args[8]->IsNumber())
        {
            const bool is3d(args[4]->BooleanValue());
            const double radius(args[5]->NumberValue());
            const double x(args[6]->NumberValue());
            const double y(args[7]->NumberValue());
            const double z(args[8]->NumberValue());

            readCommand = new ReadCommandPointRadius(
                    pdalSession,
                    itcBufferPool,
                    readId,
                    host,
                    port,
                    compress,
                    schema,
                    is3d,
                    radius,
                    x,
                    y,
                    z,
                    queryCallback,
                    dataCallback);
        }
        // Quad index query, bounded and unbounded.
        else if (
            args.Length() == 9 &&
            (
                (isDefined(args[4]) &&
                    args[4]->IsArray() &&
                    Array::Cast(*args[4])->Length() >= 4) ||
                !isDefined(args[4])
            ) &&
            isDefined(args[5]) && isInteger(args[5]) &&
            isDefined(args[6]) && isInteger(args[6]))
        {
            const std::size_t depthBegin(args[5]->Uint32Value());
            const std::size_t depthEnd(args[6]->Uint32Value());

            if (isDefined(args[4]))
            {
                Local<Array> bbox(Array::Cast(*args[4]));

                if (bbox->Get(Integer::New(0))->IsNumber() &&
                    bbox->Get(Integer::New(1))->IsNumber() &&
                    bbox->Get(Integer::New(2))->IsNumber() &&
                    bbox->Get(Integer::New(3))->IsNumber())
                {
                    const Point min(
                            bbox->Get(Integer::New(0))->NumberValue(),
                            bbox->Get(Integer::New(1))->NumberValue());
                    const Point max(
                            bbox->Get(Integer::New(2))->NumberValue(),
                            bbox->Get(Integer::New(3))->NumberValue());

                    if (max.x >= min.x && max.y >= min.y)
                    {
                        readCommand = new ReadCommandBoundedQuadIndex(
                                pdalSession,
                                itcBufferPool,
                                readId,
                                host,
                                port,
                                compress,
                                schema,
                                BBox(min, max),
                                depthBegin,
                                depthEnd,
                                queryCallback,
                                dataCallback);
                    }
                    else
                    {
                        errorCallback(queryCallback, "Invalid coords in query");
                    }
                }
                else
                {
                    errorCallback(queryCallback, "Invalid coord types in query");
                }
            }
            else
            {

                readCommand = new ReadCommandQuadIndex(
                        pdalSession,
                        itcBufferPool,
                        readId,
                        host,
                        port,
                        compress,
                        schema,
                        depthBegin,
                        depthEnd,
                        queryCallback,
                        dataCallback);
            }

        }
        // Custom bounds rasterized query.
        else if (
            args.Length() == 8 &&
            (isDefined(args[4]) &&
                args[4]->IsArray() &&
                Array::Cast(*args[4])->Length() >= 4) &&
            (isDefined(args[5]) &&
                args[5]->IsArray() &&
                Array::Cast(*args[5])->Length() == 2))
        {
            Local<Array> bbox(Array::Cast(*args[4]));
            Local<Array> dims(Array::Cast(*args[5]));

            if (bbox->Get(Integer::New(0))->IsNumber() &&
                bbox->Get(Integer::New(1))->IsNumber() &&
                bbox->Get(Integer::New(2))->IsNumber() &&
                bbox->Get(Integer::New(3))->IsNumber() &&
                isInteger(dims->Get(Integer::New(0))) &&
                isInteger(dims->Get(Integer::New(1))))
            {
                double xMin(
                        bbox->Get(Integer::New(0))->NumberValue());
                double yMin(
                        bbox->Get(Integer::New(1))->NumberValue());
                double xMax(
                        bbox->Get(Integer::New(2))->NumberValue());
                double yMax(
                        bbox->Get(Integer::New(3))->NumberValue());

                const std::size_t xNum(
                        dims->Get(Integer::New(0))->Uint32Value());
                const std::size_t yNum(
                        dims->Get(Integer::New(1))->Uint32Value());

                if (xMax >= xMin && yMax >= xMin)
                {
                    const double xStep(
                            (xMax - xMin) / static_cast<double>(xNum));
                    const double yStep(
                            (yMax - yMin) / static_cast<double>(yNum));

                    const RasterMeta customRasterMeta(
                            xMin,
                            xMax,
                            xStep,
                            yMin,
                            yMax,
                            yStep);

                    readCommand = new ReadCommandRastered(
                            pdalSession,
                            itcBufferPool,
                            readId,
                            host,
                            port,
                            compress,
                            schema,
                            customRasterMeta,
                            queryCallback,
                            dataCallback);
                }
                else
                {
                    errorCallback(queryCallback, "Invalid coords in query");
                }
            }
            else
            {
                errorCallback(queryCallback, "Invalid coord types in query");
            }
        }
        else if (
            args.Length() == 7 &&
            isDefined(args[4]) && isInteger(args[4]))
        {
            const std::size_t level(args[4]->Uint32Value());

            readCommand = new ReadCommandQuadLevel(
                    pdalSession,
                    itcBufferPool,
                    readId,
                    host,
                    port,
                    compress,
                    schema,
                    level,
                    queryCallback,
                    dataCallback);
        }
        else
        {
            errorCallback(queryCallback, "Could not identify 'read' from args");
        }
    }
    else
    {
        errorCallback(
                queryCallback,
                "Host, port, and callback must be supplied");
    }

    //scope.Close(Undefined());
    return readCommand;
}

