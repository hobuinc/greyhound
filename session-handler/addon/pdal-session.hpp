#pragma once

#include <mutex>

#include <pdal/PointContext.hpp>

#include "once.hpp"
#include "live-data-source.hpp"
#include "grey-tree.hpp"

class Schema;

class PdalSession
{
public:
    PdalSession();

    void initialize(
            const std::string& pipelineId,
            const std::string& pipeline,
            bool execute);

    // Queries.
    std::size_t getNumPoints() const;
    std::string getSchema() const;
    std::string getStats() const;
    std::string getSrs() const;
    std::vector<std::size_t> getFills() const;

    // Serialization methods.
    void serialize();   // Write to disk.
    bool awaken();      // Wake from serialized quad-tree.

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
        if (m_serialDataSource) return m_serialDataSource->pointContext();
        else if (m_liveDataSource) return m_liveDataSource->pointContext();
        else throw std::runtime_error("Not initialized!");
    }

private:
    std::string m_pipelineId;
    std::string m_pipeline;
    std::shared_ptr<LiveDataSource> m_liveDataSource;
    std::shared_ptr<GreyReader> m_serialDataSource;

    // Disallow copy/assignment.
    PdalSession(const PdalSession&);
    PdalSession& operator=(const PdalSession&);
};

