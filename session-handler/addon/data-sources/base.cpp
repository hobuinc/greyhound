#include "read-queries/base.hpp"
#include "types/bbox.hpp"
#include "types/schema.hpp"
#include "base.hpp"

std::vector<std::size_t> DataSource::getFills() const
{
    return std::vector<std::size_t>();
}

std::shared_ptr<ReadQuery> DataSource::queryUnindexed(
        const Schema& schema,
        bool compress,
        std::size_t start,
        std::size_t count)
{
    return 0;
}

std::shared_ptr<ReadQuery> DataSource::query(
        const Schema& schema,
        bool compress,
        const BBox& bbox,
        std::size_t depthBegin,
        std::size_t depthEnd)
{
    return 0;
}

std::shared_ptr<ReadQuery> DataSource::query(
        const Schema& schema,
        bool compress,
        std::size_t depthBegin,
        std::size_t depthEnd)
{
    return 0;
}

std::shared_ptr<ReadQuery> DataSource::query(
        const Schema& schema,
        bool compress,
        std::size_t rasterize,
        RasterMeta& rasterMeta)
{
    return 0;
}

std::shared_ptr<ReadQuery> DataSource::query(
        const Schema& schema,
        bool compress,
        const RasterMeta& rasterMeta)
{
    return 0;
}

std::shared_ptr<ReadQuery> DataSource::query(
        const Schema& schema,
        bool compress,
        bool is3d,
        double radius,
        double x,
        double y,
        double z)
{
    return 0;
}

