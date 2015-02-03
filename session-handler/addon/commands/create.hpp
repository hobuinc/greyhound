#pragma once

#include <string>
#include <vector>

#include "commands/background.hpp"
#include "types/serial-paths.hpp"

class PdalSession;

struct CreateData : public Background
{
    CreateData(
            std::shared_ptr<PdalSession> pdalSession,
            std::string pipelineId,
            const std::vector<std::string>& paths,
            const BBox& bbox,
            bool serialCompress,
            SerialPaths serialPaths,
            v8::Persistent<v8::Function> callback)
        : pdalSession(pdalSession)
        , pipelineId(pipelineId)
        , paths(paths)
        , bbox(bbox)
        , serialCompress(serialCompress)
        , serialPaths(serialPaths)
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
    const std::vector<std::string> paths;
    const BBox bbox;
    const bool serialCompress;
    const SerialPaths serialPaths;

    // Outputs
    std::string errMsg;

    v8::Persistent<v8::Function> callback;
};

