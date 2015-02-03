#include <limits>
#include <cmath>
#include <memory>
#include <string>

#include <pdal/PointBuffer.hpp>
#include <pdal/Dimension.hpp>
#include <pdal/Utils.hpp>

#include "node.hpp"

namespace
{
    pdal::PointContext initPointContext(/*Schema schema*/)
    {
        // TODO Programmatically via schema.
        pdal::PointContext pointContext;
        pointContext.registerDim(pdal::Dimension::Id::X);
        pointContext.registerDim(pdal::Dimension::Id::Y);
        pointContext.registerDim(pdal::Dimension::Id::Z);
        pointContext.registerDim(pdal::Dimension::Id::Intensity);
        pointContext.registerDim(pdal::Dimension::Id::Classification);
        pointContext.assignDim("OriginId", pdal::Dimension::Type::Unsigned64);
        return pointContext;
    }
}

PointInfo::PointInfo(
        const Point& point,
        const pdal::PointBuffer& pointBuffer,
        const pdal::PointContext& pointContext,
        std::size_t index,
        const Origin& origin,
        std::size_t size)
    : point(point)
    , origin(origin)
    , bytes(size)
{
    char* pos(reinterpret_cast<char*>(bytes.data()));
    for (auto dim : pointContext.dims())
    {
        pointBuffer.getField(pos, dim, pointContext.dimType(dim), index);
        pos += pointContext.dimSize(dim);
    }
}

SleepyTree::SleepyTree(
        const BBox& bbox,
        /*Schema schema, */
        std::size_t overflowDepth)
    : m_overflowDepth(overflowDepth)
    , m_pointContext(initPointContext())
    , m_tree(new StemNode(bbox, overflowDepth))
{ }

SleepyTree::~SleepyTree()
{ }

void SleepyTree::insert(const pdal::PointBuffer& pointBuffer, Origin origin)
{
    Point point;
    std::set<LeafNode*> leafSet;
    LeafNode* leaf;
    const std::size_t size(m_pointContext.pointSize());
    for (std::size_t i = 0; i < pointBuffer.size(); ++i)
    {
        point.x = pointBuffer.getFieldAs<double>(pdal::Dimension::Id::X, i);
        point.y = pointBuffer.getFieldAs<double>(pdal::Dimension::Id::Y, i);

        if (m_tree->bbox.contains(point))
        {
            leaf = m_tree->addPoint(
                    new PointInfo(
                        point,
                        pointBuffer,
                        m_pointContext,
                        i,
                        origin,
                        size));

            if (leaf)
            {
                leafSet.insert(leaf);
            }
        }
    }

    for (auto it : leafSet)
    {
        it->save();
    }
}

BBox SleepyTree::getBounds() const
{
    return m_tree->bbox;
}

/*
std::vector<std::size_t> SleepyTree::getPoints(
        const std::size_t minDepth,
        const std::size_t maxDepth) const
{
    std::vector<std::size_t> results;
    m_tree->getPoints(results, minDepth, maxDepth);

    return results;
}

std::vector<std::size_t> SleepyTree::getPoints(
        const std::size_t rasterize,
        RasterMeta& rasterMeta) const
{
    std::vector<std::size_t> results;

    const Point min(m_tree->bbox.min());
    const Point max(m_tree->bbox.max());

    const std::size_t exp(std::pow(2, rasterize));

    rasterMeta.xStep =  m_tree->bbox.width() / exp;
    rasterMeta.yStep =  m_tree->bbox.height() / exp;
    rasterMeta.xBegin = min.x + (xStep / 2);
    rasterMeta.yBegin = min.y + (yStep / 2);
    rasterMeta.xEnd =   max.x + (xStep / 2); // One tick past the end.
    rasterMeta.yEnd =   max.y + (yStep / 2);

    results.resize(exp * exp, std::numeric_limits<std::size_t>::max());

    m_tree->getPoints(results, rasterize, rasterMeta);

    return results;
}

std::vector<std::size_t> SleepyTree::getPoints(
        const RasterMeta& rasterMeta) const;
{
    std::vector<std::size_t> results;

    const std::size_t width (pdal::Utils::sround((xEnd - xBegin) / xStep));
    const std::size_t height(pdal::Utils::sround((yEnd - yBegin) / yStep));
    results.resize(width * height, std::numeric_limits<std::size_t>::max());

    m_tree->getPoints(results, rasterMeta);

    return results;
}

std::vector<std::size_t> SleepyTree::getPoints(
        const BBox& bbox,
        std::size_t minDepth,
        std::size_t maxDepth) const
{
    std::vector<std::size_t> results;
    m_tree->getPoints(results, bbox, minDepth, maxDepth);

    return results;
}
*/

