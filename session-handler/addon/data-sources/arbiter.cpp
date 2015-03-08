#include "arbiter.hpp"

bool Arbiter::serialize()
{
    return false;
}

std::shared_ptr<ReadQuery> Arbiter::queryUnindexed(
        const entwine::Schema& schema,
        bool compress,
        std::size_t start,
        std::size_t count)
{
    throw std::runtime_error("Unsupported query");
}

std::shared_ptr<ReadQuery> Arbiter::query(
        const entwine::Schema& schema,
        bool compress,
        const entwine::BBox& bbox,
        std::size_t depthBegin,
        std::size_t depthEnd)
{
    throw std::runtime_error("Unsupported query");
}

std::shared_ptr<ReadQuery> Arbiter::query(
        const entwine::Schema& schema,
        bool compress,
        std::size_t depthBegin,
        std::size_t depthEnd)
{
    throw std::runtime_error("Unsupported query");
}

std::shared_ptr<ReadQuery> Arbiter::query(
        const entwine::Schema& schema,
        bool compress,
        std::size_t rasterize,
        RasterMeta& rasterMeta)
{
    throw std::runtime_error("Unsupported query");
}

std::shared_ptr<ReadQuery> Arbiter::query(
        const entwine::Schema& schema,
        bool compress,
        const RasterMeta& rasterMeta)
{
    throw std::runtime_error("Unsupported query");
}

std::shared_ptr<ReadQuery> Arbiter::query(
        const entwine::Schema& schema,
        bool compress,
        bool is3d,
        double radius,
        double x,
        double y,
        double z)
{
    throw std::runtime_error("Unsupported query");
}

