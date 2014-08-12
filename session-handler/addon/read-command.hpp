#pragma once

#include<memory>

#include <v8.h>

// TODO REMOVE!!!
using namespace v8;

static bool isInteger(const Local<v8::Value>& value)
{
    return value->IsInt32() || value->IsUint32();
}

static bool isDouble(const Local<v8::Value>& value)
{
    return value->IsNumber() && !isInteger(value);
}

static bool isDefined(const Local<v8::Value>& value)
{
    return !value->IsUndefined();
}

static void errorCallback(
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

class ReadCommand
{
public:
    virtual void run() = 0;

    void cancel(bool cancel) { m_cancel = cancel; }
    void errMsg(std::string errMsg) { m_errMsg = errMsg; }

    void transmit(std::size_t offset, std::size_t numBytes)
    {
        m_bufferTransmitter->transmit(offset, numBytes);
    }

    std::size_t     numPoints() const { return m_numPoints; }
    std::size_t     numBytes()  const { return m_numBytes;  }
    std::string     errMsg()    const { return m_errMsg;    }
    bool            cancel()    const { return m_cancel;    }
    v8::Persistent<v8::Function> callback() const { return m_callback; }

    ReadCommand(
            std::shared_ptr<PdalSession> pdalSession,
            std::string host,
            std::size_t port,
            v8::Persistent<v8::Function> callback)
        : m_pdalSession(pdalSession)
        , m_host(host)
        , m_port(port)
        , m_callback(callback)
        , m_cancel(false)
        , m_data(0)
        , m_bufferTransmitter()
        , m_errMsg()
        , m_numPoints(0)
        , m_numBytes(0)
    { }

    virtual ~ReadCommand()
    {
        if (m_data)
            delete [] m_data;
    }

protected:
    const std::shared_ptr<PdalSession> m_pdalSession;
    const std::string m_host;
    const std::size_t m_port;

    v8::Persistent<v8::Function> m_callback;
    bool m_cancel;

    unsigned char* m_data;
    std::shared_ptr<BufferTransmitter> m_bufferTransmitter;
    std::string m_errMsg;

    void setNumPoints(std::size_t numPoints)
    {
        m_numPoints = numPoints;
        m_numBytes  = numPoints * m_pdalSession->getStride();
    }

private:
    std::size_t m_numPoints;
    std::size_t m_numBytes;
};

class ReadCommandUnindexed : public ReadCommand
{
public:
    ReadCommandUnindexed(
            std::shared_ptr<PdalSession> pdalSession,
            std::string host,
            std::size_t port,
            std::size_t start,
            std::size_t count,
            v8::Persistent<v8::Function> callback)
        : ReadCommand(pdalSession, host, port, callback)
        , m_start(start)
        , m_count(count)
    { }

    void run()
    {
        setNumPoints(m_pdalSession->read(&m_data, m_start, m_count));
        m_bufferTransmitter.reset(
                new BufferTransmitter(m_host, m_port, m_data, numBytes()));
    }

private:
    const std::size_t m_start;
    const std::size_t m_count;
};

class ReadCommandPointRadius : public ReadCommand
{
public:
    ReadCommandPointRadius(
            std::shared_ptr<PdalSession> pdalSession,
            std::string host,
            std::size_t port,
            bool is3d,
            double radius,
            double x,
            double y,
            double z,
            v8::Persistent<v8::Function> callback)
        : ReadCommand(pdalSession, host, port, callback)
        , m_is3d(is3d)
        , m_radius(radius)
        , m_x(x)
        , m_y(y)
        , m_z(z)
    { }

    void run()
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

private:
    const bool m_is3d;
    const double m_radius;
    const double m_x;
    const double m_y;
    const double m_z;
};

static ReadCommand* createReadCommand(
        const v8::Arguments& args,
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
            args.Length() == 8 &&
            isDefined(args[2]) && args[2]->IsBoolean() &&
            isDefined(args[3]) && isDouble(args[3]) &&
            isDefined(args[4]) && isDouble(args[4]) &&
            isDefined(args[5]) && isDouble(args[5]))
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

