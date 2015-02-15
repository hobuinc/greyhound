#include "data-sources/multi.hpp"
#include "http/s3.hpp"
#include "types/serial-paths.hpp"
#include "multi-arbiter.hpp"

MultiArbiter::MultiArbiter(
        const std::string& pipelineId,
        const std::vector<std::string>& paths,
        const Schema& schema,
        const BBox& bbox,
        const bool serialCompress,
        const SerialPaths& serialPaths)
    : m_multiDataSource(
            new MultiDataSource(
                pipelineId,
                paths,
                schema,
                bbox,
                serialPaths.s3Info))
{ }

std::size_t MultiArbiter::getNumPoints() const
{
    return m_multiDataSource->getNumPoints();
}

std::string MultiArbiter::getSchema() const
{
    return m_multiDataSource->getSchema();
}

std::string MultiArbiter::getStats() const
{
    return m_multiDataSource->getStats();
}

std::string MultiArbiter::getSrs() const
{
    return m_multiDataSource->getSrs();
}

std::vector<std::size_t> MultiArbiter::getFills() const
{
    return m_multiDataSource->getFills();
}

const pdal::PointContext& MultiArbiter::pointContext() const
{
    return m_multiDataSource->pointContext();
}

std::shared_ptr<ReadQuery> MultiArbiter::query(
        const Schema& schema,
        bool compress,
        const BBox& bbox,
        std::size_t depthBegin,
        std::size_t depthEnd)
{
    return m_multiDataSource->query(
            schema,
            compress,
            bbox,
            depthBegin,
            depthEnd);
}

std::shared_ptr<ReadQuery> MultiArbiter::query(
        const Schema& schema,
        bool compress,
        std::size_t depthBegin,
        std::size_t depthEnd)
{
    return m_multiDataSource->query(
            schema,
            compress,
            depthBegin,
            depthEnd);
}

