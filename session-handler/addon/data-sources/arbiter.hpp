#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace pdal
{
    class PointContext;
}

namespace entwine
{
    class BBox;
    class Schema;
}

class DataSource;
class RasterMeta;
class ReadQuery;

class Arbiter
{
public:
    Arbiter() { }
    virtual ~Arbiter() { }

    virtual std::size_t getNumPoints() const = 0;
    virtual std::string getSchema() const = 0;
    virtual std::string getStats() const = 0;
    virtual std::string getSrs() const = 0;
    virtual std::vector<std::size_t> getFills() const = 0;

    virtual const pdal::PointContext& pointContext() const = 0;

    // TODO Should occur internally and automatically.
    virtual bool serialize();   // Return true if successful.

    // All queries will throw if not overridden, not all must be supported.
    virtual std::shared_ptr<ReadQuery> queryUnindexed(
            const entwine::Schema& schema,
            bool compress,
            std::size_t start,
            std::size_t count);

    virtual std::shared_ptr<ReadQuery> query(
            const entwine::Schema& schema,
            bool compress,
            const entwine::BBox& bbox,
            std::size_t depthBegin,
            std::size_t depthEnd);

    virtual std::shared_ptr<ReadQuery> query(
            const entwine::Schema& schema,
            bool compress,
            std::size_t depthBegin,
            std::size_t depthEnd);

    virtual std::shared_ptr<ReadQuery> query(
            const entwine::Schema& schema,
            bool compress,
            std::size_t rasterize,
            RasterMeta& rasterMeta);

    virtual std::shared_ptr<ReadQuery> query(
            const entwine::Schema& schema,
            bool compress,
            const RasterMeta& rasterMeta);

    virtual std::shared_ptr<ReadQuery> query(
            const entwine::Schema& schema,
            bool compress,
            bool is3d,
            double radius,
            double x,
            double y,
            double z);
};

