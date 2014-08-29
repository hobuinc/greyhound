#pragma once

#include <memory>

#include "pdal-session.hpp"

class ReadCommand;

class PdalBindings : public node::ObjectWrap
{
public:
    static void init(v8::Handle<v8::Object> exports);

private:
    struct ReadData;
    PdalBindings();
    ~PdalBindings();

    static v8::Persistent<v8::Function> constructor;

    static v8::Handle<v8::Value> construct(const v8::Arguments& args);

    static void doInitialize(const v8::Arguments& args, bool execute = true);
    static v8::Handle<v8::Value> parse(const v8::Arguments& args);
    static v8::Handle<v8::Value> create(const v8::Arguments& args);

    static v8::Handle<v8::Value> destroy(const v8::Arguments& args);
    static v8::Handle<v8::Value> getNumPoints(const v8::Arguments& args);
    static v8::Handle<v8::Value> getDimensions(const v8::Arguments& args);
    static v8::Handle<v8::Value> getSrs(const v8::Arguments& args);
    static v8::Handle<v8::Value> read(const v8::Arguments& args);
    static v8::Handle<v8::Value> cancel(const v8::Arguments& args);

    std::shared_ptr<PdalSession> m_pdalSession;

    // Make this a member to facilitate cancel() functionality.
    std::map<std::string, ReadCommand*> m_readCommands;

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
};

