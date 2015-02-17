#include <chrono>

#include <pdal/Options.hpp>
#include <pdal/PipelineManager.hpp>
#include <pdal/PointBuffer.hpp>
#include <pdal/PointContext.hpp>
#include <pdal/Stage.hpp>
#include <pdal/StageFactory.hpp>

#include "tree/sleepy-tree.hpp"
#include "data-sources/multi.hpp"
#include "data-sources/multi-batcher.hpp"
#include "http/s3.hpp"
#include "read-queries/multi.hpp"

namespace
{
    const std::size_t numBatches(2);
    const std::size_t restorePoint(4);
}

MultiDataSource::MultiDataSource(
        const std::string& pipelineId,
        const std::vector<std::string>& paths,
        const Schema& schema,
        const BBox& bbox,
        const S3Info& s3Info)
{
    if (
            bbox.min().x == 0 && bbox.min().y == 0 &&
            bbox.max().x == 0 && bbox.min().y == 0)
    {
        std::cout << "Making tree " << pipelineId << std::endl;
        // Default bbox means we should try to awaken a serialized source.
        m_sleepyTree.reset(new SleepyTree(pipelineId));
    }
    else
    {
        m_sleepyTree.reset(new SleepyTree(pipelineId, bbox, schema));

        MultiBatcher batcher(s3Info, pipelineId, numBatches, m_sleepyTree);
        const auto start(std::chrono::high_resolution_clock::now());
        for (std::size_t i(0); i < paths.size(); ++i)
        {
            batcher.add(paths[i], i);

            if ((i + 1) % restorePoint == 0)
            {
                std::cout <<
                    "Gathering responses for restore point at " <<
                    i + 1 << " / " << paths.size() <<
                    std::endl;
                batcher.gather();
                std::cout << "Creating restore" << std::endl;

                // TODO Hardcoded path.
                std::ofstream dataStream;
                dataStream.open(
                        "/var/greyhound/serial/" + pipelineId + "/restore-info",
                        std::ofstream::out | std::ofstream::trunc);
                std::string info(std::to_string(i) + "," + paths[i]);
                dataStream.write(info.data(), info.size());
                dataStream.close();

                m_sleepyTree->save(
                        "/var/greyhound/serial/" + pipelineId + "/restore");
                std::cout << "Restore point created" << std::endl;
            }
        }

        batcher.gather();
        const auto end(std::chrono::high_resolution_clock::now());
        const std::chrono::duration<double> d(end - start);
        std::cout << "Multi " << pipelineId << " complete - took " <<
                std::chrono::duration_cast<std::chrono::seconds>(d).count() <<
                " seconds" <<
                std::endl;

        m_sleepyTree->save();
    }
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

