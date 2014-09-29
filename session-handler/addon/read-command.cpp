#include "read-command.hpp"
#include "pdal-session.hpp"

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
        std::map<std::string, ReadCommand*>& readCommands,
        const std::string readId,
        const std::string host,
        const std::size_t port,
        const Schema schema,
        v8::Persistent<v8::Function> callback)
    : m_pdalSession(pdalSession)
    , m_readCommands(readCommands)
    , m_readId(readId)
    , m_host(host)
    , m_port(port)
    , m_schema(schemaOrDefault(schema))
    , m_numPoints(0)
    , m_callback(callback)
    , m_cancel(false)
    , m_data()
    , m_bufferTransmitter()
    , m_errMsg()
{ }

ReadCommandUnindexed::ReadCommandUnindexed(
        std::shared_ptr<PdalSession> pdalSession,
        std::map<std::string, ReadCommand*>& readCommands,
        std::string readId,
        std::string host,
        std::size_t port,
        Schema schema,
        std::size_t start,
        std::size_t count,
        v8::Persistent<v8::Function> callback)
    : ReadCommand(
            pdalSession,
            readCommands,
            readId,
            host,
            port,
            schema,
            callback)
    , m_start(start)
    , m_count(count)
{ }

ReadCommandPointRadius::ReadCommandPointRadius(
        std::shared_ptr<PdalSession> pdalSession,
        std::map<std::string, ReadCommand*>& readCommands,
        std::string readId,
        std::string host,
        std::size_t port,
        Schema schema,
        bool is3d,
        double radius,
        double x,
        double y,
        double z,
        v8::Persistent<v8::Function> callback)
    : ReadCommand(
            pdalSession,
            readCommands,
            readId,
            host,
            port,
            schema,
            callback)
    , m_is3d(is3d)
    , m_radius(radius)
    , m_x(x)
    , m_y(y)
    , m_z(z)
{ }

ReadCommandQuadIndex::ReadCommandQuadIndex(
        std::shared_ptr<PdalSession> pdalSession,
        std::map<std::string, ReadCommand*>& readCommands,
        std::string readId,
        std::string host,
        std::size_t port,
        Schema schema,
        std::size_t depthBegin,
        std::size_t depthEnd,
        v8::Persistent<v8::Function> callback)
    : ReadCommand(
            pdalSession,
            readCommands,
            readId,
            host,
            port,
            schema,
            callback)
    , m_depthBegin(depthBegin)
    , m_depthEnd(depthEnd)
{ }

ReadCommandBoundedQuadIndex::ReadCommandBoundedQuadIndex(
        std::shared_ptr<PdalSession> pdalSession,
        std::map<std::string, ReadCommand*>& readCommands,
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
        v8::Persistent<v8::Function> callback)
    : ReadCommandQuadIndex(
            pdalSession,
            readCommands,
            readId,
            host,
            port,
            schema,
            depthBegin,
            depthEnd,
            callback)
    , m_xMin(xMin)
    , m_yMin(yMin)
    , m_xMax(xMax)
    , m_yMax(yMax)
{ }

ReadCommandRastered::ReadCommandRastered(
        std::shared_ptr<PdalSession> pdalSession,
        std::map<std::string, ReadCommand*>& readCommands,
        const std::string readId,
        const std::string host,
        const std::size_t port,
        const Schema schema,
        v8::Persistent<v8::Function> callback)
    : ReadCommand(
            pdalSession,
            readCommands,
            readId,
            host,
            port,
            schema,
            callback)
    , m_rasterMeta()
{ }

ReadCommandRastered::ReadCommandRastered(
        std::shared_ptr<PdalSession> pdalSession,
        std::map<std::string, ReadCommand*>& readCommands,
        const std::string readId,
        const std::string host,
        const std::size_t port,
        const Schema schema,
        const RasterMeta rasterMeta,
        v8::Persistent<v8::Function> callback)
    : ReadCommand(
            pdalSession,
            readCommands,
            readId,
            host,
            port,
            schema,
            callback)
    , m_rasterMeta(rasterMeta)
{ }

ReadCommandQuadLevel::ReadCommandQuadLevel(
        std::shared_ptr<PdalSession> pdalSession,
        std::map<std::string, ReadCommand*>& readCommands,
        std::string readId,
        std::string host,
        std::size_t port,
        Schema schema,
        std::size_t level,
        v8::Persistent<v8::Function> callback)
    : ReadCommandRastered(
            pdalSession,
            readCommands,
            readId,
            host,
            port,
            schema,
            callback)
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
            dims.push_back(
                    DimInfo(id, pointContext.dimType(id)));
        }

        return Schema(dims);
    }
}

void ReadCommand::transmit(
        const std::size_t offset,
        const std::size_t numBytes)
{
    m_bufferTransmitter->transmit(offset, numBytes);
}

void ReadCommandUnindexed::run()
{
    m_numPoints = m_pdalSession->readUnindexed(
            m_data,
            m_schema,
            m_start,
            m_count);

    m_bufferTransmitter.reset(
            new BufferTransmitter(m_host, m_port, m_data.data(), numBytes()));
}

void ReadCommandQuadIndex::run()
{
    m_numPoints = m_pdalSession->read(
            m_data,
            m_schema,
            m_depthBegin,
            m_depthEnd);

    m_bufferTransmitter.reset(
            new BufferTransmitter(m_host, m_port, m_data.data(), numBytes()));
}

void ReadCommandBoundedQuadIndex::run()
{
    m_numPoints = m_pdalSession->read(
            m_data,
            m_schema,
            m_xMin,
            m_yMin,
            m_xMax,
            m_yMax,
            m_depthBegin,
            m_depthEnd);

    m_bufferTransmitter.reset(
            new BufferTransmitter(m_host, m_port, m_data.data(), numBytes()));
}

void ReadCommandRastered::run()
{
    m_numPoints = m_pdalSession->read(
            m_data,
            m_schema,
            m_rasterMeta);

    m_bufferTransmitter.reset(
            new BufferTransmitter(m_host, m_port, m_data.data(), numBytes()));
}

void ReadCommandQuadLevel::run()
{
    m_numPoints = m_pdalSession->read(
            m_data,
            m_schema,
            m_level,
            m_rasterMeta);

    m_bufferTransmitter.reset(
            new BufferTransmitter(m_host, m_port, m_data.data(), numBytes()));
}

void ReadCommandPointRadius::run()
{
    m_numPoints = m_pdalSession->read(
            m_data,
            m_schema,
            m_is3d,
            m_radius,
            m_x,
            m_y,
            m_z);

    m_bufferTransmitter.reset(
            new BufferTransmitter(m_host, m_port, m_data.data(), numBytes()));
}

ReadCommand* ReadCommandFactory::create(
        std::shared_ptr<PdalSession> pdalSession,
        std::map<std::string, ReadCommand*>& readCommands,
        const std::string readId,
        const Arguments& args)
{
    HandleScope scope;

    ReadCommand* readCommand(0);

    if (args.Length() == 0 || !args[args.Length() - 1]->IsFunction())
    {
        // If no callback is supplied there's nothing we can do here.
        scope.Close(Undefined());
        throw std::runtime_error("Invalid callback supplied to 'read'");
    }

    // Callback is always the last argument.
    Persistent<Function> callback(
        Persistent<Function>::New(
            Local<Function>::Cast(args[args.Length() - 1])));

    // Validate host, port, and schemaRequest.
    if (
        args.Length() > 3 &&
        isDefined(args[0]) && args[0]->IsString() &&
        isDefined(args[1]) && isInteger(args[1]) &&
        isDefined(args[2]) && args[2]->IsObject())
    {
        const std::string host(*v8::String::Utf8Value(args[0]->ToString()));
        const std::size_t port(args[1]->Uint32Value());

        std::vector<DimInfo> dims;

        // Unwrap the schema request into native C++ types.
        const Local<Object> schemaObj(args[2]->ToObject());

        if (schemaObj->Has(String::New("schema")))
        {
            const Local<Array> dimArray(
                    Array::Cast(*(schemaObj->Get(String::New("schema")))));

            for (std::size_t i(0); i < dimArray->Length(); ++i)
            {
                Local<Object> dimObj(dimArray->Get(Integer::New(i))->ToObject());

                const std::string sizeString(*v8::String::Utf8Value(
                        dimObj->Get(String::New("size"))->ToString()));

                const std::size_t size(strtoul(sizeString.c_str(), 0, 0));

                if (size)
                {
                    dims.push_back(
                        DimInfo(
                            *v8::String::Utf8Value(
                                dimObj->Get(String::New("name"))->ToString()),
                            *v8::String::Utf8Value(
                                dimObj->Get(String::New("type"))->ToString()),
                            size));
                }
            }
        }

        Schema schema(dims);

        // Unindexed read - starting offset and count supplied.
        if (
            args.Length() == 6 &&
            isDefined(args[3]) && isInteger(args[3]) &&
            isDefined(args[4]) && isInteger(args[4]))
        {
            const std::size_t start(args[3]->Uint32Value());
            const std::size_t count(args[4]->Uint32Value());

            if (start < pdalSession->getNumPoints())
            {
                readCommand = new ReadCommandUnindexed(
                        pdalSession,
                        readCommands,
                        readId,
                        host,
                        port,
                        schema,
                        start,
                        count,
                        callback);
            }
            else
            {
                errorCallback(callback, "Invalid 'start' in 'read' request");
            }
        }
        // KD-indexed read - point/radius supplied.
        else if (
            args.Length() == 9 &&
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
                    readCommands,
                    readId,
                    host,
                    port,
                    schema,
                    is3d,
                    radius,
                    x,
                    y,
                    z,
                    callback);
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
                                readCommands,
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
                                callback);
                    }
                    else
                    {
                        errorCallback(callback, "Invalid coords in query");
                    }
                }
                else
                {
                    errorCallback(callback, "Invalid coord types in query");
                }
            }
            else
            {

                readCommand = new ReadCommandQuadIndex(
                        pdalSession,
                        readCommands,
                        readId,
                        host,
                        port,
                        schema,
                        depthBegin,
                        depthEnd,
                        callback);
            }

        }
        // Custom bounds rasterized query.
        else if (
            args.Length() == 7 &&
            (isDefined(args[3]) &&
                args[3]->IsArray() &&
                Array::Cast(*args[3])->Length() >= 4) &&
            isDefined(args[4]) && isInteger(args[4]) &&
            isDefined(args[5]) && isInteger(args[5]))
        {
            Local<Array> bbox(Array::Cast(*args[3]));

            if (bbox->Get(Integer::New(0))->IsNumber() &&
                bbox->Get(Integer::New(1))->IsNumber() &&
                bbox->Get(Integer::New(2))->IsNumber() &&
                bbox->Get(Integer::New(3))->IsNumber())
            {
                double xMin(
                        bbox->Get(Integer::New(0))->NumberValue());
                double yMin(
                        bbox->Get(Integer::New(1))->NumberValue());
                double xMax(
                        bbox->Get(Integer::New(2))->NumberValue());
                double yMax(
                        bbox->Get(Integer::New(3))->NumberValue());

                const std::size_t xNum(1 + args[4]->Uint32Value());
                const std::size_t yNum(1 + args[5]->Uint32Value());

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
                            readCommands,
                            readId,
                            host,
                            port,
                            schema,
                            customRasterMeta,
                            callback);
                }
                else
                {
                    errorCallback(callback, "Invalid coords in query");
                }
            }
            else
            {
                errorCallback(callback, "Invalid coord types in query");
            }
        }
        else if (
            args.Length() == 5 &&
            isDefined(args[3]) && isInteger(args[3]))
        {
            const std::size_t level(args[3]->Uint32Value());

            readCommand = new ReadCommandQuadLevel(
                    pdalSession,
                    readCommands,
                    readId,
                    host,
                    port,
                    schema,
                    level,
                    callback);
        }
        else
        {
            errorCallback(callback, "Could not identify 'read' from args");
        }
    }
    else
    {
        errorCallback(callback, "Host, port, and callback must be supplied");
    }

    scope.Close(Undefined());
    return readCommand;
}

