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
    virtual std::size_t run() = 0;

    void data(unsigned char* data) { m_data = data; }
    void bufferTransmitter(BufferTransmitter* bufferTransmitter)
        { m_bufferTransmitter.reset(bufferTransmitter); }
    void numPoints(std::size_t numPoints) { m_numPoints = numPoints; }
    void numBytes(std::size_t numBytes) { m_numBytes = numBytes; }
    void errMsg(std::string errMsg) { m_errMsg = errMsg; }
    void callback(v8::Persistent<v8::Function> callback)
        { m_callback = callback; }
    void cancel(bool cancel) { m_cancel = cancel; }

    unsigned char**  dataRef()      { return &m_data; }
    unsigned char* data() const { return m_data; }
    BufferTransmitter* bufferTransmitter() const
        { return m_bufferTransmitter.get(); }
    std::size_t     numPoints() const { return m_numPoints; }
    std::size_t     numBytes()  const { return m_numBytes; }
    std::string     errMsg()    const { return m_errMsg; }
    v8::Persistent<v8::Function> callback() const { return m_callback; }
    bool cancel() const { return m_cancel; }

    ReadCommand(
            std::shared_ptr<PdalSession> pdalSession,
            std::string host,
            std::size_t port,
            v8::Persistent<v8::Function> callback)
        : pdalSession(pdalSession)
        , host(host)
        , port(port)
        , m_data(0)
        , m_bufferTransmitter()
        , m_numPoints(0)
        , m_errMsg()
        , m_callback(callback)
        , m_cancel(false)
    { }

    virtual ~ReadCommand()
    {
        if (m_data)
            delete [] m_data;
    }

    // Inputs
    const std::shared_ptr<PdalSession> pdalSession;
    const std::string host;
    const std::size_t port;

protected:
    // Outputs
    unsigned char* m_data;
    std::shared_ptr<BufferTransmitter> m_bufferTransmitter;
    std::size_t m_numPoints;
    std::size_t m_numBytes;
    std::string m_errMsg;

    v8::Persistent<v8::Function> m_callback;
    bool m_cancel;
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
        , start(start)
        , count(count)
    { }

    std::size_t run()
    {
        return pdalSession->read(&m_data, start, count);
    }

// TODO
//private:
    const std::size_t start;
    const std::size_t count;
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
        , is3d(is3d)
        , radius(radius)
        , x(x)
        , y(y)
        , z(z)
    { }

    std::size_t run()
    {
        return pdalSession->read(&m_data, is3d, radius, x, y, z);
    }

private:
    const bool is3d;
    const double radius;
    const double x;
    const double y;
    const double z;
};

static ReadCommand* createReadCommand(
        const v8::Arguments& args,
        std::shared_ptr<PdalSession> pdalSession)
{
    HandleScope scope;

    ReadCommand* readCommand(0);

    // Validate host and port.
    if (isDefined(args[0]) && args[0]->IsString() &&
        isDefined(args[1]) && isInteger(args[1]))
    {
        const std::string host(*v8::String::Utf8Value(args[0]->ToString()));
        const std::size_t port(args[1]->Uint32Value());

        if (isDefined(args[2]) && isInteger(args[2]) &&
            isDefined(args[3]) && isInteger(args[3]))
        {
            if (args[4]->IsUndefined() || !args[4]->IsFunction())
            {
                scope.Close(Undefined());
                throw std::runtime_error("Invalid callback");
            }

            readCommand = new ReadCommandUnindexed(
                    pdalSession,
                    host,
                    port,
                    args[2]->Uint32Value(),
                    args[3]->Uint32Value(),
                    Persistent<Function>::New(Local<Function>::Cast(args[4])));
        }
    }

    scope.Close(Undefined());
    return readCommand;
}

