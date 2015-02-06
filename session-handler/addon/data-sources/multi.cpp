#include <pdal/Options.hpp>
#include <pdal/PipelineManager.hpp>
#include <pdal/PointBuffer.hpp>
#include <pdal/PointContext.hpp>
#include <pdal/Stage.hpp>
#include <pdal/StageFactory.hpp>

#include "tree/sleepy-tree.hpp"
#include "data-sources/multi.hpp"
#include "read-queries/multi.hpp"

MultiDataSource::MultiDataSource(
        const std::string& pipelineId,
        const std::vector<std::string>& paths,
        const Schema& schema,
        const BBox& bbox)
    : m_pipelineId(pipelineId)
    , m_paths(paths)
    , m_sleepyTree(new SleepyTree(bbox, schema))
{
    for (std::size_t i(0); i < paths.size(); ++i)
    {
        std::unique_ptr<pdal::PipelineManager> pipelineManager(
                new pdal::PipelineManager());
        std::unique_ptr<pdal::StageFactory> stageFactory(
                new pdal::StageFactory());
        std::unique_ptr<pdal::Options> options(
                new pdal::Options());

        const std::string& filename(paths[i]);
        std::cout << "Adding " << filename << std::endl;

        const std::string driver(stageFactory->inferReaderDriver(filename));
        if (driver.size())
        {
            pipelineManager->addReader(driver);

            pdal::Stage* reader(
                    static_cast<pdal::Reader*>(pipelineManager->getStage()));
            options->add(pdal::Option("filename", filename));
            reader->setOptions(*options.get());

            pipelineManager->execute();
            const pdal::PointBufferSet& pbSet(pipelineManager->buffers());

            for (const auto pointBuffer : pbSet)
            {
                m_sleepyTree->insert(*pointBuffer.get(), i);
            }
        }
        else
        {
            std::cout << "No driver found - " << filename << std::endl;
        }
    }

    m_sleepyTree->save();
}

std::size_t MultiDataSource::getNumPoints() const
{
    // TODO
    return 0;
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

