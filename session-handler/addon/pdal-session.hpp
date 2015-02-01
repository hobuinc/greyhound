#pragma once

#include <mutex>
#include <vector>

#include <pdal/PointContext.hpp>

#include "once.hpp"
#include "live-data-source.hpp"
#include "grey-reader.hpp"

class QueryData;

class PdalSession
{
public:
    PdalSession();
    ~PdalSession() { }

    void initialize(
            const std::string& pipelineId,
            const std::string& filename,
            bool serialCompress,
            const SerialPaths& serialPaths);

    // Queries.
    std::size_t getNumPoints();
    std::string getSchema();
    std::string getStats();
    std::string getSrs();
    std::vector<std::size_t> getFills();

    // Write to disk.
    void serialize(const SerialPaths& serialPaths);

    // Read un-indexed data with an offset and a count.
    std::shared_ptr<QueryData> queryUnindexed(
            const Schema& schema,
            bool compress,
            std::size_t start,
            std::size_t count);

    // Read quad-tree indexed data with a bounding box query and min/max tree
    // depths to search.
    std::shared_ptr<QueryData> query(
            const Schema& schema,
            bool compress,
            double xMin,
            double yMin,
            double xMax,
            double yMax,
            std::size_t depthBegin,
            std::size_t depthEnd);

    // Read quad-tree indexed data with min/max tree depths to search.
    std::shared_ptr<QueryData> query(
            const Schema& schema,
            bool compress,
            std::size_t depthBegin,
            std::size_t depthEnd);

    // Read quad-tree indexed data with depth level for rasterization.
    std::shared_ptr<QueryData> query(
            const Schema& schema,
            bool compress,
            std::size_t rasterize,
            RasterMeta& rasterMeta);

    // Read a bounded set of points into a raster of pre-determined resolution.
    std::shared_ptr<QueryData> query(
            const Schema& schema,
            bool compress,
            const RasterMeta& rasterMeta);

    // Perform KD-indexed query of point + radius.
    std::shared_ptr<QueryData> query(
            const Schema& schema,
            bool compress,
            bool is3d,
            double radius,
            double x,
            double y,
            double z);

    void read(
            std::shared_ptr<ItcBuffer> itcBuffer,
            std::size_t maxNumBytes,
            const Schema& schema,
            std::shared_ptr<QueryData> queryData);

    const pdal::PointContext& pointContext() const
    {
        if (m_serialDataSource) return m_serialDataSource->pointContext();
        else if (m_liveDataSource) return m_liveDataSource->pointContext();
        else throw std::runtime_error("Not initialized!");
    }

private:
    // Wake from serialized quad-tree.
    bool awaken(const SerialPaths& serialPaths);

    std::string m_pipelineId;
    std::string m_filename;
    bool m_serialCompress;
    std::shared_ptr<LiveDataSource> m_liveDataSource;
    std::shared_ptr<GreyReader> m_serialDataSource;

    Once m_initOnce;
    std::mutex m_awakenMutex;

    // Disallow copy/assignment.
    PdalSession(const PdalSession&);
    PdalSession& operator=(const PdalSession&);
};

