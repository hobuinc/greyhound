#pragma once

#include <vector>
#include <string>

#include <pdal/PipelineManager.hpp>

#include "grey-common.hpp"
#include "pdal-index.hpp"
#include "once.hpp"

class Schema;
class DimInfo;
class PdalIndex;
class RasterMeta;

class LiveDataSource
{
public:
    LiveDataSource(
            const std::string& pipelineId,
            const std::string& filename);

    void ensureIndex(PdalIndex::IndexType indexType);

    std::size_t getNumPoints() const;
    std::string getSchema() const;
    std::string getStats() const;
    std::string getSrs() const;
    std::vector<std::size_t> getFills() const;

    void serialize(bool compress, const SerialPaths& serialPaths);

    // Read un-indexed data with an offset and a count.
    std::size_t queryUnindexed(std::size_t start, std::size_t count);

    // Read quad-tree indexed data with a bounding box query and min/max tree
    // depths to search.
    std::vector<std::size_t> query(
            double xMin,
            double yMin,
            double xMax,
            double yMax,
            std::size_t depthBegin,
            std::size_t depthEnd);

    // Read quad-tree indexed data with min/max tree depths to search.
    std::vector<std::size_t> query(
            std::size_t depthBegin,
            std::size_t depthEnd);

    // Read quad-tree indexed data with depth level for rasterization.
    std::vector<std::size_t> query(
            std::size_t rasterize,
            RasterMeta& rasterMeta);

    // Read a bounded set of points into a raster of pre-determined resolution.
    std::vector<std::size_t> query(const RasterMeta& rasterMeta);

    // Perform KD-indexed query of point + radius.
    std::vector<std::size_t> query(
            bool is3d,
            double radius,
            double x,
            double y,
            double z);

    const std::shared_ptr<pdal::PointBuffer> pointBuffer() const
    {
        return m_pointBuffer;
    }

    const pdal::PointContext& pointContext() const
    {
        return m_pointContext;
    }

private:
    const std::string m_pipelineId;

    pdal::PipelineManager m_pipelineManager;
    std::shared_ptr<pdal::PointBuffer> m_pointBuffer;
    pdal::PointContext m_pointContext;

    Once m_serializeOnce;

    std::shared_ptr<PdalIndex> m_pdalIndex;
};

