#pragma once

#include <vector>
#include <memory>

#include <pdal/PointContext.hpp>

#include "types/point.hpp"

namespace pdal
{
    class PointBuffer;
}

class BBox;
class RasterMeta;
class StemNode;

typedef uint64_t Origin;

struct PointInfo
{
    PointInfo(
            const Point& point,
            const pdal::PointBuffer& pointBuffer,
            const pdal::PointContext& pointContext,
            std::size_t index,
            const Origin& origin,
            std::size_t size);

    const Point point;
    const Origin origin;
    std::vector<uint8_t> bytes;
};

class SleepyTree
{
public:
    explicit SleepyTree(const BBox& bbox, std::size_t overflowDepth = 12);
    ~SleepyTree();

    // Insert the points from a PointBuffer into this index.
    void insert(const pdal::PointBuffer& pointBuffer, Origin origin);

    // Get bounds of the quad tree.
    BBox getBounds() const;

    // Return all points at depth levels strictly less than depthEnd.
    // A depthEnd value of zero returns all points in the tree.
    std::vector<std::size_t> getPoints(
            std::size_t depthEnd = 0) const;

    // Return all points at depth levels between [depthBegin, depthEnd).
    // A depthEnd value of zero will return all points at levels >= depthBegin.
    std::vector<std::size_t> getPoints(
            std::size_t depthBegin,
            std::size_t depthEnd) const;

    // Rasterize a single level of the tree.
    std::vector<std::size_t> getPoints(
            std::size_t rasterize,
            RasterMeta& rasterMeta) const;

    // Get custom raster via bounds and resolution query.
    std::vector<std::size_t> getPoints(const RasterMeta& rasterMeta) const;

    // Return all points within the query bounding box, searching only up to
    // depth levels strictly less than depthEnd.
    // A depthEnd value of zero will return all existing points that fall
    // within the query range regardless of depth.
    std::vector<std::size_t> getPoints(
            const BBox& bbox,
            std::size_t depthEnd = 0) const;

    // Return all points within the bounding box, searching at tree depth
    // levels from [depthBegin, depthEnd).
    // A depthEnd value of zero will return all points within the query range
    // that have a tree level >= depthBegin.
    std::vector<std::size_t> getPoints(
            const BBox& bbox,
            std::size_t depthBegin,
            std::size_t depthEnd) const;

private:
    const std::size_t m_overflowDepth;
    const pdal::PointContext m_pointContext;
    std::unique_ptr<StemNode> m_tree;

    SleepyTree(const SleepyTree&);
    SleepyTree& operator=(const SleepyTree&);
};

