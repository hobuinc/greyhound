#pragma once

#include <memory>
#include <vector>

#include <pdal/PointBuffer.hpp>
#include <pdal/PointContext.hpp>
#include <pdal/Dimension.hpp>

#include "http/s3.hpp"
#include "types/point.hpp"

class BBox;
class RasterMeta;
class Schema;
class StemNode;

typedef uint64_t Origin;
typedef std::vector<std::pair<uint64_t, std::size_t>> MultiResults;

struct PointInfo
{
    PointInfo(
            const pdal::PointContextRef pointContext,
            const pdal::PointBuffer* pointBuffer,
            std::size_t index,
            pdal::Dimension::Id::Enum originDim,
            Origin origin);

    PointInfo(const Point* point, char* pos, std::size_t len);

    void write(char* pos);

    const Point* point;
    std::vector<char> bytes;
};

class SleepyCache
{
public:
    void insert(uint64_t id, std::shared_ptr<std::vector<char>> data)
    {
        std::lock_guard<std::mutex> lock(mutex);
        cache.insert(std::make_pair(id, data));
    }

    std::shared_ptr<std::vector<char>> data(uint64_t id)
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (cache.count(id))
        {
            return cache.at(id);
        }
        else
        {
            return 0;
        }
    }

private:
    std::mutex mutex;
    std::map<uint64_t, std::shared_ptr<std::vector<char>>> cache;
};

class SleepyTree
{
public:
    SleepyTree(
            const std::string& pipelineId,
            const BBox& bbox,
            const Schema& schema,
            const S3Info& s3Info,
            std::size_t overflowDepth = 12);
    ~SleepyTree();

    // Insert the points from a PointBuffer into this index.
    void insert(const pdal::PointBuffer* pointBuffer, Origin origin);

    // Finalize the tree so it may be queried.  No more pipelines may be added.
    void save();

    // Awaken the tree so more pipelines may be added.  After a load(), no
    // queries should be made until save() is subsequently called.
    void load();

    // Get bounds of the quad tree.
    const BBox& getBounds() const;

    // Return all points at depth levels between [depthBegin, depthEnd).
    // A depthEnd value of zero will return all points at levels >= depthBegin.
    MultiResults getPoints(
            std::size_t depthBegin,
            std::size_t depthEnd);

    // Return all points within the bounding box, searching at tree depth
    // levels from [depthBegin, depthEnd).
    // A depthEnd value of zero will return all points within the query range
    // that have a tree level >= depthBegin.
    MultiResults getPoints(
            const BBox& bbox,
            std::size_t depthBegin,
            std::size_t depthEnd);

    const pdal::PointContext& pointContext() const;
    std::shared_ptr<std::vector<char>> data(uint64_t id);

    std::size_t numPoints() const;

private:
    const std::string m_pipelineId;
    const std::size_t m_overflowDepth;

    // Must be this order.
    pdal::PointContext m_pointContext;
    pdal::Dimension::Id::Enum m_originDim;
    //std::unique_ptr<pdal::PointBuffer> m_basePointBuffer;
    std::shared_ptr<std::vector<char>> m_basePoints;
    SleepyCache m_cache;
    std::unique_ptr<StemNode> m_tree;

    S3 m_s3;
    std::size_t m_numPoints;

    SleepyTree(const SleepyTree&);
    SleepyTree& operator=(const SleepyTree&);
};

