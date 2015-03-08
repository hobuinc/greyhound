#include "serial.hpp"

#include <entwine/types/bbox.hpp>
#include <entwine/types/schema.hpp>

#include "read-queries/serial.hpp"
#include "types/raster-meta.hpp"
#include "types/serial-paths.hpp"

SerialDataSource::SerialDataSource(
        const std::string& pipelineId,
        const SerialPaths& serialPaths)
    : m_greyReader(GreyReaderFactory::create(pipelineId, serialPaths))
{ }

SerialDataSource::SerialDataSource(GreyReader* greyReader)
    : m_greyReader(greyReader)
{ }

const pdal::PointContext& SerialDataSource::pointContext() const
{
    return m_greyReader->pointContext();
}

std::size_t SerialDataSource::getNumPoints() const
{
    return m_greyReader->getNumPoints();
}

std::string SerialDataSource::getSchema() const
{
    return m_greyReader->getSchema();
}

std::string SerialDataSource::getStats() const
{
    return m_greyReader->getStats();
}

std::string SerialDataSource::getSrs() const
{
    return m_greyReader->getSrs();
}

std::vector<std::size_t> SerialDataSource::getFills() const
{
    return m_greyReader->getFills();
}

std::shared_ptr<ReadQuery> SerialDataSource::query(
        const entwine::Schema& schema,
        bool compressed,
        const entwine::BBox& bbox,
        std::size_t depthBegin,
        std::size_t depthEnd)
{
    return std::shared_ptr<ReadQuery>(
            new SerialReadQuery(
                schema,
                compressed,
                false,
                m_greyReader->query(bbox, depthBegin, depthEnd)));
}

std::shared_ptr<ReadQuery> SerialDataSource::query(
        const entwine::Schema& schema,
        bool compressed,
        std::size_t depthBegin,
        std::size_t depthEnd)
{
    return std::shared_ptr<ReadQuery>(
            new SerialReadQuery(
                schema,
                compressed,
                false,
                m_greyReader->query(depthBegin, depthEnd)));
}

std::shared_ptr<ReadQuery> SerialDataSource::query(
        const entwine::Schema& schema,
        bool compressed,
        std::size_t rasterize,
        RasterMeta& rasterMeta)
{
    return std::shared_ptr<ReadQuery>(
            new SerialReadQuery(
                schema,
                compressed,
                false,
                m_greyReader->query(rasterize, rasterMeta)));
}

