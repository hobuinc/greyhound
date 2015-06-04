#pragma once

#include <string>
#include <vector>

#include <entwine/drivers/arbiter.hpp>
#include <entwine/types/bbox.hpp>

#include "commands/background.hpp"
#include "types/paths.hpp"

class Session;

struct CreateData : public Background
{
    CreateData(
            std::shared_ptr<Session> session,
            std::string name,
            Paths paths,
            std::size_t maxQuerySize,
            std::size_t maxCacheSize,
            std::shared_ptr<entwine::Arbiter> arbiter,
            v8::Persistent<v8::Function> callback)
        : session(session)
        , name(name)
        , paths(paths)
        , maxQuerySize(maxQuerySize)
        , maxCacheSize(maxCacheSize)
        , arbiter(arbiter)
        , callback(callback)
    { }

    ~CreateData()
    {
        callback.Dispose();
    }

    // Inputs
    const std::shared_ptr<Session> session;
    const std::string name;
    const Paths paths;
    const std::size_t maxQuerySize;
    const std::size_t maxCacheSize;
    std::shared_ptr<entwine::Arbiter> arbiter;

    v8::Persistent<v8::Function> callback;
};

