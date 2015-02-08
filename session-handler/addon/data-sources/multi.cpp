#include <thread>

#include <pdal/Options.hpp>
#include <pdal/PipelineManager.hpp>
#include <pdal/PointBuffer.hpp>
#include <pdal/PointContext.hpp>
#include <pdal/Stage.hpp>
#include <pdal/StageFactory.hpp>

#include "tree/sleepy-tree.hpp"
#include "data-sources/multi.hpp"
#include "data-sources/multi-batcher.hpp"
#include "read-queries/multi.hpp"

namespace
{
    const std::size_t numBatches(6);
}

MultiDataSource::MultiDataSource(
        const std::string& pipelineId,
        const std::vector<std::string>& paths,
        const Schema& schema,
        const BBox& bbox)
    : m_pipelineId(pipelineId)
    , m_paths(paths)
    , m_sleepyTree(new SleepyTree(bbox, schema))
{
    MultiBatcher batcher(numBatches, m_sleepyTree);
    for (std::size_t i(0); i < paths.size(); ++i)
    {
        batcher.add(paths[i], i);
    }

    batcher.done();
    m_sleepyTree->save();
}

std::size_t MultiDataSource::getNumPoints() const
{
    return m_sleepyTree->numPoints();
}

std::string MultiDataSource::getSchema() const
{
    // TODO
    return "";
}

std::string MultiDataSource::getStats() const
{
    // TODO
    return "{ }";
}

std::string MultiDataSource::getSrs() const
{
    // TODO
    return "";
}

std::vector<std::size_t> MultiDataSource::getFills() const
{
    // TODO
    return std::vector<std::size_t>();
}

std::shared_ptr<ReadQuery> MultiDataSource::query(
        const Schema& schema,
        bool compressed,
        const BBox& bbox,
        std::size_t depthBegin,
        std::size_t depthEnd)
{
    MultiResults results(m_sleepyTree->getPoints(bbox, depthBegin, depthEnd));
    return std::shared_ptr<ReadQuery>(
            new MultiReadQuery(
                schema,
                compressed,
                false,
                m_sleepyTree,
                results));
}

std::shared_ptr<ReadQuery> MultiDataSource::query(
        const Schema& schema,
        bool compressed,
        std::size_t depthBegin,
        std::size_t depthEnd)
{
    MultiResults results(m_sleepyTree->getPoints(depthBegin, depthEnd));
    return std::shared_ptr<ReadQuery>(
            new MultiReadQuery(
                schema,
                compressed,
                false,
                m_sleepyTree,
                results));
}

const pdal::PointContext& MultiDataSource::pointContext() const
{
    return m_sleepyTree->pointContext();
}

