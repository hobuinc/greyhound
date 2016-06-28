#pragma once

#include <string>
#include <vector>

#include <entwine/reader/cache.hpp>
#include <entwine/types/outer-scope.hpp>

#include "commands/background.hpp"

class Session;

struct CreateData : public Background
{
    CreateData(
            std::shared_ptr<Session> session,
            std::string name,
            const std::vector<std::string>& paths,
            entwine::OuterScope& outerScope,
            std::shared_ptr<entwine::Cache> cache,
            v8::UniquePersistent<v8::Function> callback)
        : session(session)
        , name(name)
        , paths(paths)
        , outerScope(outerScope)
        , cache(cache)
        , callback(std::move(callback))
    { }

    ~CreateData()
    {
        callback.Reset();
    }

    // Inputs
    const std::shared_ptr<Session> session;
    const std::string name;
    const std::vector<std::string> paths;
    entwine::OuterScope& outerScope;
    std::shared_ptr<entwine::Cache> cache;

    v8::UniquePersistent<v8::Function> callback;
};

