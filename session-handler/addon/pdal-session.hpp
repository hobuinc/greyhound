#pragma once

#include <mutex>
#include <vector>

#include <pdal/PointContext.hpp>

#include "once.hpp"
#include "live-data-source.hpp"
#include "grey-reader.hpp"

class Schema;

class PrepData
{
public:
    virtual std::size_t numPoints() const = 0;
    virtual bool done() const = 0;
    virtual bool serial() const { return false; }

    virtual void read(
            std::shared_ptr<ItcBuffer> buffer,
            std::size_t maxNumBytes,
            const Schema& schema,
            bool rasterize = false) = 0;
};

class UnindexedPrepData : public PrepData
{
public:
    UnindexedPrepData(
            std::shared_ptr<pdal::PointBuffer> pointBuffer,
            std::size_t start,
            std::size_t count);

    virtual std::size_t numPoints() const { return m_end - m_begin; }
    virtual bool done() const { return m_index == m_end; }
    virtual void read(
            std::shared_ptr<ItcBuffer> buffer,
            std::size_t maxNumBytes,
            const Schema& schema,
            bool rasterize = false);

private:
    const std::shared_ptr<pdal::PointBuffer> m_pointBuffer;

    const std::size_t m_begin;
    const std::size_t m_end;

    std::size_t m_index;
};

class LivePrepData : public PrepData
{
public:
    LivePrepData(
            std::shared_ptr<pdal::PointBuffer> pointBuffer,
            std::vector<std::size_t> indexList);

    const std::vector<std::size_t>& indexList() { return m_indexList; }
    virtual bool done() const { return m_index == m_indexList.size(); }
    virtual void read(
            std::shared_ptr<ItcBuffer> buffer,
            std::size_t maxNumBytes,
            const Schema& schema,
            bool rasterize = false);

    virtual std::size_t numPoints() const { return m_indexList.size(); }

private:
    const std::shared_ptr<pdal::PointBuffer> m_pointBuffer;

    const std::vector<std::size_t> m_indexList;

    std::size_t m_index;
};

class SerialPrepData : public PrepData
{
public:
    SerialPrepData(GreyQuery query);

    virtual std::size_t numPoints() const { return m_query.numPoints(); }
    virtual bool done() const { return m_index == numPoints(); }
    virtual void read(
            std::shared_ptr<ItcBuffer> buffer,
            std::size_t maxNumBytes,
            const Schema& schema,
            bool rasterize = false);

    virtual bool serial() const { return true; }

private:
    GreyQuery m_query;

    std::size_t m_index;
};

class PdalSession
{
public:
    PdalSession();
    ~PdalSession() { }

    void initialize(
            const std::string& pipelineId,
            const std::string& pipeline,
            const std::vector<std::string>& serialPaths,
            bool execute);

    // Queries.
    std::size_t getNumPoints() const;
    std::string getSchema() const;
    std::string getStats() const;
    std::string getSrs() const;
    std::vector<std::size_t> getFills() const;

    // Write to disk.
    void serialize(const std::vector<std::string>& serialPaths);

    // Wake from serialized quad-tree.
    bool awaken(const std::vector<std::string>& serialPaths);

    // Read un-indexed data with an offset and a count.
    std::shared_ptr<PrepData> prepUnindexed(
            std::size_t start,
            std::size_t count);

    // Read quad-tree indexed data with a bounding box query and min/max tree
    // depths to search.
    std::shared_ptr<PrepData> prep(
            double xMin,
            double yMin,
            double xMax,
            double yMax,
            std::size_t depthBegin,
            std::size_t depthEnd);

    // Read quad-tree indexed data with min/max tree depths to search.
    std::shared_ptr<PrepData> prep(
            std::size_t depthBegin,
            std::size_t depthEnd);

    // Read quad-tree indexed data with depth level for rasterization.
    std::shared_ptr<PrepData> prep(
            std::size_t rasterize,
            RasterMeta& rasterMeta);

    // Read a bounded set of points into a raster of pre-determined resolution.
    std::shared_ptr<PrepData> prep(const RasterMeta& rasterMeta);

    // Perform KD-indexed query of point + radius.
    std::shared_ptr<PrepData> prep(
            bool is3d,
            double radius,
            double x,
            double y,
            double z);

    void read(
            std::shared_ptr<ItcBuffer> itcBuffer,
            std::size_t maxNumBytes,
            const Schema& schema,
            std::shared_ptr<PrepData> prepData);

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

    Once m_initOnce;
    std::mutex m_awakenMutex;

    // Disallow copy/assignment.
    PdalSession(const PdalSession&);
    PdalSession& operator=(const PdalSession&);
};

