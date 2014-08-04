#pragma once

#include <memory>

#include "pdal-session.hpp"

class PdalBindings : public node::ObjectWrap
{
public:
    static void init(v8::Handle<v8::Object> exports);

private:
    PdalBindings();
    ~PdalBindings();

    static v8::Persistent<v8::Function> constructor;

    static v8::Handle<v8::Value> construct(const v8::Arguments& args);

    static void doInitialize(
            const v8::Arguments& args,
            bool execute = true);
    static v8::Handle<v8::Value> parse(const v8::Arguments& args);
    static v8::Handle<v8::Value> create(const v8::Arguments& args);

    static v8::Handle<v8::Value> destroy(const v8::Arguments& args);
    static v8::Handle<v8::Value> getNumPoints(const v8::Arguments& args);
    static v8::Handle<v8::Value> getSchema(const v8::Arguments& args);
    static v8::Handle<v8::Value> read(const v8::Arguments& args);

    // Helper function to perform an errback.
    static void errorCallback(
            v8::Persistent<v8::Function> callback,
            std::string errMsg);

    std::shared_ptr<PdalSession> m_pdalSession;

    struct CreateData
    {
        CreateData(
                std::shared_ptr<PdalSession> pdalSession,
                std::string pipeline,
                bool execute,
                v8::Persistent<v8::Function> callback)
            : pdalSession(pdalSession)
            , pipeline(pipeline)
            , execute(execute)
            , errMsg()
            , callback(callback)
        { }

        // Inputs
        const std::shared_ptr<PdalSession> pdalSession;
        const std::string pipeline;
        const bool execute;

        // Outputs
        std::string errMsg;

        v8::Persistent<v8::Function> callback;
    };

    struct ReadData
    {
        ReadData(
                std::shared_ptr<PdalSession> pdalSession,
                std::string host,
                std::size_t port,
                std::size_t start,
                std::size_t count,
                v8::Persistent<v8::Function> callback)
            : pdalSession(pdalSession)
            , host(host)
            , port(port)
            , start(start)
            , count(count)
            , data(0)
            , bufferTransmitter()
            , numPoints(0)
            , errMsg()
            , callback(callback)
        { }

        // Inputs
        const std::shared_ptr<PdalSession> pdalSession;
        const std::string host;
        const std::size_t port;
        const std::size_t start;
        const std::size_t count;

        // Outputs
        unsigned char* data;
        std::shared_ptr<BufferTransmitter> bufferTransmitter;
        std::size_t numPoints;
        std::size_t numBytes;
        std::string errMsg;

        v8::Persistent<v8::Function> callback;
    };
};

