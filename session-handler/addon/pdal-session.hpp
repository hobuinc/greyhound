#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "util/once.hpp"

namespace pdal
{
    class PointContext;
}

namespace entwine
{
    class BBox;
    class Schema;
    class SleepyTree;
}

class RasterMeta;
class ReadQuery;
class SerialPaths;

class PdalSession
{
public:
    PdalSession();
    ~PdalSession();

    void initialize(
            const std::string& pipelineId,
            const std::string& filename,
            bool serialCompress,
            const SerialPaths& serialPaths);

    void initialize(
            const std::string& pipelineId,
            const std::vector<std::string>& paths,
            const entwine::Schema& schema,
            const entwine::BBox& bbox,
            bool serialCompress,
            const SerialPaths& serialPaths);

    // Queries.
    std::size_t getNumPoints();
    std::string getSchemaString();
    std::string getStats();
    std::string getSrs();
    std::vector<std::size_t> getFills();

    // Write to disk.
    void serialize(const SerialPaths& serialPaths);

    // Read un-indexed data with an offset and a count.
    std::shared_ptr<ReadQuery> queryUnindexed(
            const entwine::Schema& schema,
            bool compress,
            std::size_t start,
            std::size_t count);

    // Read quad-tree indexed data with a bounding box query and min/max tree
    // depths to search.
    std::shared_ptr<ReadQuery> query(
            const entwine::Schema& schema,
            bool compress,
            const entwine::BBox& bbox,
            std::size_t depthBegin,
            std::size_t depthEnd);

    // Read quad-tree indexed data with min/max tree depths to search.
    std::shared_ptr<ReadQuery> query(
            const entwine::Schema& schema,
            bool compress,
            std::size_t depthBegin,
            std::size_t depthEnd);

    // Read quad-tree indexed data with depth level for rasterization.
    std::shared_ptr<ReadQuery> query(
            const entwine::Schema& schema,
            bool compress,
            std::size_t rasterize,
            RasterMeta& rasterMeta);

    // Read a bounded set of points into a raster of pre-determined resolution.
    std::shared_ptr<ReadQuery> query(
            const entwine::Schema& schema,
            bool compress,
            const RasterMeta& rasterMeta);

    const entwine::Schema& schema() const;

private:
    // Make sure we are successfully initialized.
    void check();

    Once m_initOnce;
    std::unique_ptr<entwine::SleepyTree> m_tree;

    // Disallow copy/assignment.
    PdalSession(const PdalSession&);
    PdalSession& operator=(const PdalSession&);
};

