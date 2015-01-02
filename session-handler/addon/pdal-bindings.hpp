#pragma once

#include <memory>
#include <mutex>
#include <map>

#include "pdal-session.hpp"
#include "buffer-pool.hpp"

class PdalSession;
class ReadCommand;

enum Action
{
    Execute,
    Validate,
    Awaken
};

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
    static v8::Handle<v8::Value> getSchema(const v8::Arguments& args);
    static v8::Handle<v8::Value> getStats(const v8::Arguments& args);
    static v8::Handle<v8::Value> getSrs(const v8::Arguments& args);
    static v8::Handle<v8::Value> getFills(const v8::Arguments& args);
    static v8::Handle<v8::Value> read(const v8::Arguments& args);
    static v8::Handle<v8::Value> serialize(const v8::Arguments& args);

    std::shared_ptr<PdalSession> m_pdalSession;

    ItcBufferPool& m_itcBufferPool;

    struct CreateData
    {
        CreateData(
                std::shared_ptr<PdalSession> pdalSession,
                std::string pipelineId,
                std::string pipeline,
                std::vector<std::string> serialPaths,
                bool execute,
                v8::Persistent<v8::Function> callback)
            : pdalSession(pdalSession)
            , pipelineId(pipelineId)
            , pipeline(pipeline)
            , serialPaths(serialPaths)
            , execute(execute)
            , errMsg()
            , callback(callback)
        { }

        ~CreateData()
        {
            callback.Dispose();
        }

        // Inputs
        const std::shared_ptr<PdalSession> pdalSession;
        const std::string pipelineId;
        const std::string pipeline;
        const std::vector<std::string> serialPaths;
        const bool execute;

        // Outputs
        std::string errMsg;

        v8::Persistent<v8::Function> callback;
    };

    struct SerializeData
    {
        SerializeData(
                std::shared_ptr<PdalSession> pdalSession,
                std::vector<std::string> paths,
                v8::Persistent<v8::Function> callback)
            : pdalSession(pdalSession)
            , paths(paths)
            , callback(callback)
        { }

        ~SerializeData()
        {
            callback.Dispose();
        }

        // Inputs
        const std::shared_ptr<PdalSession> pdalSession;
        const std::vector<std::string> paths;

        // Outputs
        std::string errMsg;

        v8::Persistent<v8::Function> callback;
    };
};

