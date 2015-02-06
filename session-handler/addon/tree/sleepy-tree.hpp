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
    PointInfo(
            const Point& point,
            const pdal::PointBuffer& pointBuffer,
            const pdal::PointContext& pointContext,
            const pdal::Dimension::Id::Enum originDim,
            std::size_t index,
            const Origin& origin);

    const Point point;
    const Origin origin;
    pdal::PointBuffer pointBuffer;
};

class SleepyTree
{
public:
    explicit SleepyTree(
            const BBox& bbox,
            const Schema& schema,
            std::size_t overflowDepth = 10); // TODO
    ~SleepyTree();

    // Insert the points from a PointBuffer into this index.
    void insert(const pdal::PointBuffer& pointBuffer, Origin origin);

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
    std::shared_ptr<pdal::PointBuffer> pointBuffer(uint64_t id) const;

private:
    const std::size_t m_overflowDepth;
    pdal::PointContext m_pointContext;
    std::shared_ptr<pdal::PointBuffer> m_stemPointBuffer;
    std::unique_ptr<StemNode> m_tree;

    pdal::Dimension::Id::Enum m_originDim;

    SleepyTree(const SleepyTree&);
    SleepyTree& operator=(const SleepyTree&);
};

