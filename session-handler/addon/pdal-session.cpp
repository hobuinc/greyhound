#include <iostream>

#include <entwine/types/bbox.hpp>
#include <entwine/types/schema.hpp>
#include <entwine/tree/multi-batcher.hpp>
#include <entwine/tree/sleepy-tree.hpp>

#include "buffer-pool.hpp"
#include "read-queries/entwine.hpp"
#include "types/serial-paths.hpp"
#include "pdal-session.hpp"

PdalSession::PdalSession()
    : m_initOnce()
    , m_tree()
{ }

PdalSession::~PdalSession()
{ }

void PdalSession::initialize(
        const std::string& pipelineId,
        const std::string& filename,
        const bool serialCompress,
        const SerialPaths& serialPaths)
{
}

void PdalSession::initialize(
        const std::string& pipelineId,
        const std::vector<std::string>& paths,
        const entwine::Schema& schema,
        const entwine::BBox& bbox,
        const bool serialCompress,
        const SerialPaths& serialPaths)
{
    std::cout << "Init multi" << std::endl;
    m_initOnce.ensure([
            this,
            &pipelineId,
            &paths,
            &schema,
            &bbox,
            serialCompress,
            &serialPaths]()
    {
        try
        {
            if (
                    bbox.min().x == 0 && bbox.min().y == 0 &&
                    bbox.max().x == 0 && bbox.min().y == 0)
            {
                /*
                std::cout << "Making tree " << pipelineId << std::endl;
                // Default bbox means we should try to awaken a serialized source.
                m_sleepyTree.reset(new entwine::SleepyTree(pipelineId));
                */
            }
            else
            {
                // TODO Many hard-codes.
                m_tree.reset(
                        new entwine::SleepyTree(
                            "/var/greyhound/serial/" + pipelineId,  // TODO Path.
                            bbox,
                            schema,
                            2,
                            12,
                            12,
                            12,
                            false));
                /*
            const S3Info& s3Info,
            SleepyTree& sleepyTree,
            std::size_t numThreads,
            std::size_t pointBatchSize = 0,
            std::size_t snapshot = 0);
*/
                entwine::MultiBatcher batcher(
                        serialPaths.s3Info,
                        *m_tree.get(),
                        32); // numThreads TODO

                const auto start(std::chrono::high_resolution_clock::now());
                for (const auto& path : paths)
                {
                    batcher.add(path);
                }

                batcher.gather();
                const auto end(std::chrono::high_resolution_clock::now());
                const std::chrono::duration<double> d(end - start);
                const auto time(
                        std::chrono::duration_cast<std::chrono::seconds>
                            (d).count());

                std::cout << "Multi " << pipelineId << " complete - took " <<
                        time <<
                        " seconds" <<
                        std::endl;

                m_tree->save();
            }
        }
        catch (...)
        {
            m_tree.reset();
            throw std::runtime_error(
                    "Caught exception in multi init - " + pipelineId);
        }
    });
}

std::size_t PdalSession::getNumPoints()
{
    check();
    return m_tree->numPoints();
}

std::string PdalSession::getSchemaString()
{
    check();
    return "";
}

std::string PdalSession::getStats()
{
    check();
    return "{ }";
}

std::string PdalSession::getSrs()
{
    check();
    return "";
}

std::vector<std::size_t> PdalSession::getFills()
{
    check();
    return std::vector<std::size_t>();
}

void PdalSession::serialize(const SerialPaths& serialPaths)
{
    check();
}

std::shared_ptr<ReadQuery> PdalSession::queryUnindexed(
        const entwine::Schema& schema,
        bool compress,
        std::size_t start,
        std::size_t count)
{
    // TODO
    check();
    return std::shared_ptr<ReadQuery>();
}

std::shared_ptr<ReadQuery> PdalSession::query(
        const entwine::Schema& schema,
        bool compress,
        const entwine::BBox& bbox,
        std::size_t depthBegin,
        std::size_t depthEnd)
{
    check();
    std::vector<std::size_t> results(m_tree->query(bbox, depthBegin, depthEnd));

    return std::shared_ptr<ReadQuery>(
            new EntwineReadQuery(
                schema,
                compress,
                false,
                *m_tree.get(),
                results));
}

std::shared_ptr<ReadQuery> PdalSession::query(
        const entwine::Schema& schema,
        bool compress,
        std::size_t depthBegin,
        std::size_t depthEnd)
{
    check();
    std::vector<std::size_t> results(m_tree->query(depthBegin, depthEnd));
    return std::shared_ptr<ReadQuery>(
            new EntwineReadQuery(
                schema,
                compress,
                false,
                *m_tree.get(),
                results));
}

std::shared_ptr<ReadQuery> PdalSession::query(
        const entwine::Schema& schema,
        bool compress,
        std::size_t rasterize,
        RasterMeta& rasterMeta)
{
    // TODO
    check();
    return std::shared_ptr<ReadQuery>();
}

std::shared_ptr<ReadQuery> PdalSession::query(
        const entwine::Schema& schema,
        bool compress,
        const RasterMeta& rasterMeta)
{
    // TODO
    check();
    return std::shared_ptr<ReadQuery>();
}

const entwine::Schema& PdalSession::schema() const
{
    return m_tree->schema();
}

void PdalSession::check()
{
    if (m_initOnce.await())
    {
        throw std::runtime_error("Not initialized!");
    }
}

