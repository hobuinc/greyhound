#pragma once

#include <string>
#include <vector>

#include <entwine/third/arbiter/arbiter.hpp>
#include <entwine/reader/cache.hpp>
#include <entwine/types/bbox.hpp>

#include "commands/background.hpp"

class Session;

struct CreateData : public Background
{
    CreateData(
            std::shared_ptr<Session> session,
            std::string name,
            const std::vector<std::string>& paths,
            std::shared_ptr<arbiter::Arbiter> arbiter,
            std::shared_ptr<entwine::Cache> cache,
            v8::Persistent<v8::Function> callback)
        : session(session)
        , name(name)
        , paths(paths)
        , arbiter(arbiter)
        , cache(cache)
        , callback(callback)
    { }

    ~CreateData()
    {
        callback.Dispose();
    }

    // Inputs
    const std::shared_ptr<Session> session;
    const std::string name;
    const std::vector<std::string> paths;
    std::shared_ptr<arbiter::Arbiter> arbiter;
    std::shared_ptr<entwine::Cache> cache;

    v8::Persistent<v8::Function> callback;
};

