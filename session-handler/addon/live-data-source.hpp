#pragma once

#include <vector>
#include <string>

#include <pdal/PipelineManager.hpp>

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
            const std::string& pipeline,
            bool execute);

    void ensureIndex(PdalIndex::IndexType indexType);

    std::size_t getNumPoints() const;
    std::string getSchema() const;
    std::string getStats() const;
    std::string getSrs() const;
    std::vector<std::size_t> getFills() const;

    void serialize(const std::vector<std::string>& serialPaths);

    // Read un-indexed data with an offset and a count.
    std::size_t readUnindexed(
            std::vector<unsigned char>& buffer,
            const Schema& schema,
            std::size_t start,
            std::size_t count);

    // Read quad-tree indexed data with a bounding box query and min/max tree
    // depths to search.
    std::size_t read(
            std::vector<unsigned char>& buffer,
            const Schema& schema,
            double xMin,
            double yMin,
            double xMax,
            double yMax,
            std::size_t depthBegin,
            std::size_t depthEnd);

    // Read quad-tree indexed data with min/max tree depths to search.
    std::size_t read(
            std::vector<unsigned char>& buffer,
            const Schema& schema,
            std::size_t depthBegin,
            std::size_t depthEnd);

    // Read quad-tree indexed data with depth level for rasterization.
    std::size_t read(
            std::vector<unsigned char>& buffer,
            const Schema& schema,
            std::size_t rasterize,
            RasterMeta& rasterMeta);

    // Read a bounded set of points into a raster of pre-determined resolution.
    std::size_t read(
            std::vector<unsigned char>& buffer,
            const Schema& schema,
            const RasterMeta& rasterMeta);

    // Perform KD-indexed query of point + radius.
    std::size_t read(
            std::vector<unsigned char>& buffer,
            const Schema& schema,
            bool is3d,
            double radius,
            double x,
            double y,
            double z);

    const pdal::PointContext& pointContext() const
    {
        return m_pointContext;
    }

private:
    const std::string m_pipelineId;

    pdal::PipelineManager m_pipelineManager;
    pdal::PointBufferPtr m_pointBuffer;
    pdal::PointContext m_pointContext;

    Once m_serializeOnce;

    std::shared_ptr<PdalIndex> m_pdalIndex;

    // Returns number of bytes read into buffer.
    std::size_t readDim(
            unsigned char* buffer,
            const DimInfo& dimReq,
            std::size_t index) const;

    // Read points out from a list that represents indices into m_pointBuffer.
    std::size_t readIndexList(
            std::vector<unsigned char>& buffer,
            const Schema& schema,
            const std::vector<std::size_t>& indexList,
            bool rasterize = false) const;
};

