#pragma once

#include <vector>
#include <memory>

#include <pdal/PointBuffer.hpp>
#include <pdal/PointContext.hpp>
#include <pdal/Dimension.hpp>

#include "types/point.hpp"

class BBox;
class RasterMeta;
class Schema;
class StemNode;

typedef uint64_t Origin;
typedef std::vector<std::pair<uint64_t, std::size_t>> MultiResults;

struct PointInfo
{
    PointInfo(const Point* point);
    virtual ~PointInfo() { }

    virtual void write(
            pdal::PointBuffer* dstPointBuffer,
            std::size_t dstIndex) = 0;

    const Point* point;
};

struct PointInfoNative : PointInfo
{
    PointInfoNative(
            const pdal::PointBuffer& pointBuffer,
            std::size_t index,
            const pdal::Dimension::Id::Enum originDim,
            const Origin& origin);

    virtual void write(
            pdal::PointBuffer* dstPointBuffer,
            std::size_t dstIndex);

    const pdal::PointBuffer& pointBuffer;
    const std::size_t index;
    const pdal::Dimension::Id::Enum originDim;
    const Origin origin;
};

struct PointInfoPulled : PointInfo
{
    PointInfoPulled(
            const Point* point,
            const pdal::PointBuffer* srcPointBuffer,
            std::size_t index);

    virtual void write(
            pdal::PointBuffer* dstPointBuffer,
            std::size_t dstIndex);

    std::vector<char> bytes;
    //std::unique_ptr<pdal::PointBuffer> pointBuffer;
};

class SleepyTree
{
public:
    explicit SleepyTree(
            const BBox& bbox,
            const Schema& schema,
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
    BBox getBounds() const;

    // Return all points at depth levels between [depthBegin, depthEnd).
    // A depthEnd value of zero will return all points at levels >= depthBegin.
    MultiResults getPoints(
            std::size_t depthBegin,
            std::size_t depthEnd) const;

    // Return all points within the bounding box, searching at tree depth
    // levels from [depthBegin, depthEnd).
    // A depthEnd value of zero will return all points within the query range
    // that have a tree level >= depthBegin.
    MultiResults getPoints(
            const BBox& bbox,
            std::size_t depthBegin,
            std::size_t depthEnd) const;

    const pdal::PointContext& pointContext() const;
    pdal::PointBuffer* pointBuffer(uint64_t id) const;

    std::size_t numPoints() const;

private:
    const std::size_t m_overflowDepth;

    // Must be this order.
    pdal::PointContext m_pointContext;
    pdal::Dimension::Id::Enum m_originDim;
    std::unique_ptr<pdal::PointBuffer> m_basePointBuffer;
    std::unique_ptr<StemNode> m_tree;

    std::size_t m_numPoints;

    SleepyTree(const SleepyTree&);
    SleepyTree& operator=(const SleepyTree&);
};

