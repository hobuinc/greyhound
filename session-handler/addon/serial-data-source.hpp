#pragma once

#include <sstream>
#include <memory>
#include <string>
#include <vector>
#include <map>

#include <sqlite3.h>

#include <pdal/QuadIndex.hpp>
#include <pdal/PointBuffer.hpp>
#include <pdal/PointContext.hpp>

class Schema;
class DimInfo;
class RasterMeta;

class SerialDataSource
{
public:
    SerialDataSource(std::string pipelineId);
    ~SerialDataSource();

    std::size_t getNumPoints() const;
    std::string getSchema() const;
    std::string getStats() const;
    std::string getSrs() const;
    std::vector<std::size_t> getFills() const;

    static bool exists(std::string pipelineId);

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

    const pdal::PointContext& pointContext() const
    {
        return m_pointContext;
    }

private:
    std::vector<int> getRows(std::string sql);

    std::size_t appendIndexList(
            std::vector<unsigned char>& buffer,
            std::size_t pointOffset,
            const pdal::PointBuffer& pointBuffer,
            const Schema& schema,
            const std::vector<std::size_t>& indexList,
            bool rasterize) const;

    std::size_t insertIndexMap(
            std::vector<unsigned char>& buffer,
            const pdal::PointBuffer& pointBuffer,
            const Schema& schema,
            const std::map<std::size_t, std::size_t>& indexList);

    std::size_t readDim(
            unsigned char* buffer,
            const DimInfo& dim,
            const pdal::PointBuffer& pointBuffer,
            const std::size_t index) const;

    const std::string& m_pipelineId;    // TODO Needed?

    std::size_t m_numPoints;
    std::string m_schema;
    std::string m_stats;
    std::string m_srs;
    std::vector<std::size_t> m_fills;

    pdal::PointContext m_pointContext;
    pdal::PointBufferPtr m_pointBuffer;

    sqlite3* m_db;
    // TODO Mutex lock to write to the map/set.
    std::map<int, std::shared_ptr<pdal::QuadIndex>> m_quadCache;
    pdal::PointBufferSet m_pbSet;
};

