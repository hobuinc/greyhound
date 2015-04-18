#pragma once

#include <string>
#include <vector>

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
            v8::Persistent<v8::Function> callback)
        : session(session)
        , name(name)
        , paths(paths)
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

    v8::Persistent<v8::Function> callback;
};

