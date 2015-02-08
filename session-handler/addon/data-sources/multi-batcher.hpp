#pragma once

#include <condition_variable>
#include <mutex>
#include <thread>

#include <pdal/Options.hpp>
#include <pdal/PipelineManager.hpp>
#include <pdal/PointBuffer.hpp>
#include <pdal/Stage.hpp>
#include <pdal/StageFactory.hpp>

#include "tree/sleepy-tree.hpp"

class MultiBatcher
{
public:
    MultiBatcher(
            std::size_t numBatches,
            std::shared_ptr<SleepyTree> sleepyTree);

    void add(const std::string& filename, Origin origin);

    // Must be called before destruction.
    void done();

private:
    std::vector<std::thread> m_batches;
    std::vector<std::size_t> m_available;
    std::shared_ptr<SleepyTree> m_sleepyTree;

    std::mutex m_mutex;
    std::condition_variable m_cv;
};

