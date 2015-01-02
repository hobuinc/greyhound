#include "read-command.hpp"
#include "pdal-session.hpp"
#include "buffer-pool.hpp"
#include "read-query.hpp"

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
        const Schema schema,
        v8::Persistent<v8::Function> queryCallback,
        v8::Persistent<v8::Function> dataCallback)
    : m_pdalSession(pdalSession)
    , m_itcBufferPool(itcBufferPool)
    , m_itcBuffer()
    , m_async(new uv_async_t)
    , m_readId(readId)
    , m_host(host)
    , m_port(port)
    , m_schema(schemaOrDefault(schema))
    , m_numSent(0)
    , m_queryCallback(queryCallback)
    , m_dataCallback(dataCallback)
    , m_cancel(false)
    , m_errMsg()
{ }

bool ReadCommand::done() const
{
    return m_queryData->done();
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
    m_queryData->read(m_itcBuffer, maxNumBytes, m_schema);
}

void ReadCommandRastered::read(std::size_t maxNumBytes)
{
    m_queryData->read(m_itcBuffer, maxNumBytes, m_schema, true);
}

std::size_t ReadCommand::numPoints() const
{
    return m_queryData->numPoints();
}

std::size_t ReadCommandRastered::numBytes() const
{
    return numPoints() * m_schema.stride(true);
}

ReadCommandUnindexed::ReadCommandUnindexed(
        std::shared_ptr<PdalSession> pdalSession,
        ItcBufferPool& itcBufferPool,
        std::string readId,
        std::string host,
        std::size_t port,
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
        Schema schema,
        double xMin,
        double yMin,
        double xMax,
        double yMax,
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
            schema,
            depthBegin,
            depthEnd,
            queryCallback,
            dataCallback)
    , m_xMin(xMin)
    , m_yMin(yMin)
    , m_xMax(xMax)
    , m_yMax(yMax)
{ }

ReadCommandRastered::ReadCommandRastered(
        std::shared_ptr<PdalSession> pdalSession,
        ItcBufferPool& itcBufferPool,
        const std::string readId,
        const std::string host,
        const std::size_t port,
        const Schema schema,
        v8::Persistent<v8::Function> queryCallback,
        v8::Persistent<v8::Function> dataCallback)
    : ReadCommand(
            pdalSession,
            itcBufferPool,
            readId,
            host,
            port,
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
    m_queryData = m_pdalSession->queryUnindexed(m_start, m_count);
}

void ReadCommandQuadIndex::query()
{
    m_queryData = m_pdalSession->query(m_depthBegin, m_depthEnd);
}

void ReadCommandBoundedQuadIndex::query()
{
    m_queryData = m_pdalSession->query(
            m_xMin,
            m_yMin,
            m_xMax,
            m_yMax,
            m_depthBegin,
            m_depthEnd);
}

void ReadCommandRastered::query()
{
    m_queryData = m_pdalSession->query(m_rasterMeta);
}

void ReadCommandQuadLevel::query()
{
    m_queryData = m_pdalSession->query(m_level, m_rasterMeta);
}

void ReadCommandPointRadius::query()
{
    m_queryData = m_pdalSession->query(m_is3d, m_radius, m_x, m_y, m_z);
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
        numArgs < 4 ||
        !args[numArgs - 2]->IsFunction() ||
        !args[numArgs - 1]->IsFunction())
    {
        // If no callback is supplied there's nothing we can do here.
        //scope.Close(Undefined());
        throw std::runtime_error("Invalid callback supplied to 'read'");
    }

    Persistent<Function> queryCallback(
        Persistent<Function>::New(
            Local<Function>::Cast(args[numArgs - 2])));

    Persistent<Function> dataCallback(
        Persistent<Function>::New(
            Local<Function>::Cast(args[numArgs - 1])));

    // Validate host, port, and schemaRequest.
    if (
        args.Length() > 4 &&
        isDefined(args[0]) && args[0]->IsString() &&
        isDefined(args[1]) && isInteger(args[1]) &&
        isDefined(args[2]) && args[2]->IsArray())
    {
        const std::string host(*v8::String::Utf8Value(args[0]->ToString()));
        const std::size_t port(args[1]->Uint32Value());

        std::vector<DimInfo> dims;

        // Unwrap the schema request into native C++ types.
        const Local<Array> schemaArray(Array::Cast(*args[2]));

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
            args.Length() == 7 &&
            isDefined(args[3]) && isInteger(args[3]) &&
            isDefined(args[4]) && isInteger(args[4]))
        {
            const std::size_t start(args[3]->Uint32Value());
            const std::size_t count(args[4]->Uint32Value());

            if (start < pdalSession->getNumPoints())
            {
                readCommand = new ReadCommandUnindexed(
                        pdalSession,
                        itcBufferPool,
                        readId,
                        host,
                        port,
                        schema,
                        start,
                        count,
                        queryCallback,
                        dataCallback);
            }
            else
            {
                errorCallback(queryCallback, "Invalid 'start' in 'read' request");
            }
        }
        // KD-indexed read - point/radius supplied.
        else if (
            args.Length() == 10 &&
            isDefined(args[3]) && args[3]->IsBoolean() &&
            isDefined(args[4]) && args[4]->IsNumber() &&
            isDefined(args[5]) && args[5]->IsNumber() &&
            isDefined(args[6]) && args[6]->IsNumber() &&
            isDefined(args[7]) && args[7]->IsNumber())
        {
            const bool is3d(args[3]->BooleanValue());
            const double radius(args[4]->NumberValue());
            const double x(args[5]->NumberValue());
            const double y(args[6]->NumberValue());
            const double z(args[7]->NumberValue());

            readCommand = new ReadCommandPointRadius(
                    pdalSession,
                    itcBufferPool,
                    readId,
                    host,
                    port,
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
            args.Length() == 8 &&
            (
                (isDefined(args[3]) &&
                    args[3]->IsArray() &&
                    Array::Cast(*args[3])->Length() >= 4) ||
                !isDefined(args[3])
            ) &&
            isDefined(args[4]) && isInteger(args[4]) &&
            isDefined(args[5]) && isInteger(args[5]))
        {
            const std::size_t depthBegin(args[4]->Uint32Value());
            const std::size_t depthEnd(args[5]->Uint32Value());

            if (isDefined(args[3]))
            {
                Local<Array> bbox(Array::Cast(*args[3]));

                if (bbox->Get(Integer::New(0))->IsNumber() &&
                    bbox->Get(Integer::New(1))->IsNumber() &&
                    bbox->Get(Integer::New(2))->IsNumber() &&
                    bbox->Get(Integer::New(3))->IsNumber())
                {
                    const double xMin(
                            bbox->Get(Integer::New(0))->NumberValue());
                    const double yMin(
                            bbox->Get(Integer::New(1))->NumberValue());
                    const double xMax(
                            bbox->Get(Integer::New(2))->NumberValue());
                    const double yMax(
                            bbox->Get(Integer::New(3))->NumberValue());

                    if (xMax >= xMin && yMax >= xMin)
                    {
                        readCommand = new ReadCommandBoundedQuadIndex(
                                pdalSession,
                                itcBufferPool,
                                readId,
                                host,
                                port,
                                schema,
                                xMin,
                                yMin,
                                xMax,
                                yMax,
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
                        schema,
                        depthBegin,
                        depthEnd,
                        queryCallback,
                        dataCallback);
            }

        }
        // Custom bounds rasterized query.
        else if (
            args.Length() == 7 &&
            (isDefined(args[3]) &&
                args[3]->IsArray() &&
                Array::Cast(*args[3])->Length() >= 4) &&
            (isDefined(args[4]) &&
                args[4]->IsArray() &&
                Array::Cast(*args[4])->Length() == 2))
        {
            Local<Array> bbox(Array::Cast(*args[3]));
            Local<Array> dims(Array::Cast(*args[4]));

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
            args.Length() == 6 &&
            isDefined(args[3]) && isInteger(args[3]))
        {
            const std::size_t level(args[3]->Uint32Value());

            readCommand = new ReadCommandQuadLevel(
                    pdalSession,
                    itcBufferPool,
                    readId,
                    host,
                    port,
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

