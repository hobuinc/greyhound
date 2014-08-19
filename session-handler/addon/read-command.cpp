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

void ReadCommand::transmit(
        const std::size_t offset,
        const std::size_t numBytes)
{
    m_bufferTransmitter->transmit(offset, numBytes);
}

void ReadCommand::setNumPoints(const std::size_t numPoints)
{
    m_numPoints = numPoints;
    m_numBytes  = numPoints * m_pdalSession->getStride();
}

void ReadCommandUnindexed::run()
{
    setNumPoints(m_pdalSession->read(&m_data, m_start, m_count));

    m_bufferTransmitter.reset(
            new BufferTransmitter(m_host, m_port, m_data, numBytes()));
}

void ReadCommandQuadIndex::run()
{
    if (m_isBBoxQuery)
    {
        setNumPoints(m_pdalSession->read(
                &m_data,
                m_xMin,
                m_yMin,
                m_xMax,
                m_yMax,
                m_depthBegin,
                m_depthEnd));
    }
    else
    {
        setNumPoints(m_pdalSession->read(
                &m_data,
                m_depthBegin,
                m_depthEnd));
    }

    m_bufferTransmitter.reset(
            new BufferTransmitter(m_host, m_port, m_data, numBytes()));
}

void ReadCommandPointRadius::run()
{
    setNumPoints(m_pdalSession->read(
            &m_data,
            m_is3d,
            m_radius,
            m_x,
            m_y,
            m_z));

    m_bufferTransmitter.reset(
            new BufferTransmitter(m_host, m_port, m_data, numBytes()));
}

ReadCommand* ReadCommandFactory::create(
        const Arguments& args,
        std::shared_ptr<PdalSession> pdalSession)
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

    // Validate host and port.
    if (
        args.Length() > 2 &&
        isDefined(args[0]) && args[0]->IsString() &&
        isDefined(args[1]) && isInteger(args[1]))
    {
        const std::string host(*v8::String::Utf8Value(args[0]->ToString()));
        const std::size_t port(args[1]->Uint32Value());

        // Unindexed read - starting offset and count supplied.
        if (
            args.Length() == 5 &&
            isDefined(args[2]) && isInteger(args[2]) &&
            isDefined(args[3]) && isInteger(args[3]))
        {
            const std::size_t start(args[2]->Uint32Value());
            const std::size_t count(args[3]->Uint32Value());

            if (start < pdalSession->getNumPoints())
            {
                readCommand = new ReadCommandUnindexed(
                        pdalSession,
                        host,
                        port,
                        start,
                        count,
                        callback);
            }
            else
            {
                errorCallback(callback, "Invalid 'start' in 'read' request");
            }
        }
        else if (
            args.Length() == 6 &&
            (
                (isDefined(args[2]) &&
                    args[2]->IsArray() &&
                    Array::Cast(*args[2])->Length() >= 4) ||
                !isDefined(args[2])
            ) &&
            isDefined(args[3]) && isInteger(args[3]) &&
            isDefined(args[4]) && isInteger(args[4]))
        {
            const std::size_t depthBegin(args[3]->Uint32Value());
            const std::size_t depthEnd(args[4]->Uint32Value());

            if (isDefined(args[2]))
            {
                Local<Array> bbox(Array::Cast(*args[2]));

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
                        readCommand = new ReadCommandQuadIndex(
                                pdalSession,
                                host,
                                port,
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
                        host,
                        port,
                        depthBegin,
                        depthEnd,
                        callback);
            }

        }
        else if (
            args.Length() == 8 &&
            isDefined(args[2]) && args[2]->IsBoolean() &&
            isDefined(args[3]) && args[3]->IsNumber() &&
            isDefined(args[4]) && args[4]->IsNumber() &&
            isDefined(args[5]) && args[5]->IsNumber() &&
            isDefined(args[6]) && args[6]->IsNumber())
        {
            const bool is3d(args[2]->BooleanValue());
            const double radius(args[3]->NumberValue());
            const double x(args[4]->NumberValue());
            const double y(args[5]->NumberValue());
            const double z(args[6]->NumberValue());

            readCommand = new ReadCommandPointRadius(
                    pdalSession,
                    host,
                    port,
                    is3d,
                    radius,
                    x,
                    y,
                    z,
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

