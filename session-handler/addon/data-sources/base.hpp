#pragma once

#include <memory>
#include <string>
#include <vector>

class BBox;
class RasterMeta;
class ReadQuery;
class Schema;

class DataSource
{
public:
    virtual ~DataSource() { }

    virtual std::size_t getNumPoints() const = 0;
    virtual std::string getSchema() const = 0;
    virtual std::string getStats() const = 0;
    virtual std::string getSrs() const = 0;

    virtual std::vector<std::size_t> getFills() const;

    // Read un-indexed data with an offset and a count.
    virtual std::shared_ptr<ReadQuery> queryUnindexed(
            const Schema& schema,
            bool compress,
            std::size_t start,
            std::size_t count);

    // Read quad-tree indexed data with a bounding box query and min/max tree
    // depths to search.
    virtual std::shared_ptr<ReadQuery> query(
            const Schema& schema,
            bool compress,
            const BBox& bbox,
            std::size_t depthBegin,
            std::size_t depthEnd);

    // Read quad-tree indexed data with min/max tree depths to search.
    virtual std::shared_ptr<ReadQuery> query(
            const Schema& schema,
            bool compress,
            std::size_t depthBegin,
            std::size_t depthEnd);

    // Read quad-tree indexed data with depth level for rasterization.
    virtual std::shared_ptr<ReadQuery> query(
            const Schema& schema,
            bool compress,
            std::size_t rasterize,
            RasterMeta& rasterMeta);

    // Read a bounded set of points into a raster of pre-determined resolution.
    virtual std::shared_ptr<ReadQuery> query(
            const Schema& schema,
            bool compress,
            const RasterMeta& rasterMeta);

    // Perform KD-indexed query of point + radius.
    virtual std::shared_ptr<ReadQuery> query(
            const Schema& schema,
            bool compress,
            bool is3d,
            double radius,
            double x,
            double y,
            double z);
};


