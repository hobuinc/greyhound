#include <limits>
#include <cmath>
#include <memory>
#include <string>

#include <pdal/PointBuffer.hpp>
#include <pdal/Dimension.hpp>
#include <pdal/Utils.hpp>

#include "node.hpp"

PointInfo::PointInfo(
        const Point& point,
        const pdal::PointBuffer& other,
        const pdal::PointContext& pointContext,
        const pdal::Dimension::Id::Enum originDim,
        std::size_t index,
        const Origin& origin)
    : point(point)
    , origin(origin)
    , pointBuffer(pointContext)
{
    std::vector<char> bytes(8);
    char* pos(bytes.data());
    for (auto dim : pointContext.dimTypes())
    {
        // Not all dimensions may be present in every pipeline of our
        // invokation, which is not an error.
        if (other.hasDim(dim.m_id))
        {
            other.getField(pos, dim.m_id, dim.m_type, index);
            pointBuffer.setField(dim.m_id, dim.m_type, 0, pos);
        }
    }

    // Set our custom origin value.
    pointBuffer.setField(originDim, 0, origin);
}

SleepyTree::SleepyTree(
        const BBox& bbox,
        const Schema& schema,
        std::size_t overflowDepth)
    : m_overflowDepth(overflowDepth)
    , m_pointContext()
    , m_stemPointBuffer()
    , m_tree(new StemNode(bbox.encapsulate(), overflowDepth))
    , m_originDim(static_cast<pdal::Dimension::Id::Enum>(0xFFFF))
{
    // TODO Programmatically via schema.
    m_pointContext.registerDim(pdal::Dimension::Id::X);
    m_pointContext.registerDim(pdal::Dimension::Id::Y);
    m_pointContext.registerDim(pdal::Dimension::Id::Z);
    m_pointContext.registerDim(pdal::Dimension::Id::ScanAngleRank);
    m_pointContext.registerDim(pdal::Dimension::Id::Intensity);
    m_pointContext.registerDim(pdal::Dimension::Id::PointSourceId);
    m_pointContext.registerDim(pdal::Dimension::Id::ReturnNumber);
    m_pointContext.registerDim(pdal::Dimension::Id::NumberOfReturns);
    m_pointContext.registerDim(pdal::Dimension::Id::ScanDirectionFlag);
    m_pointContext.registerDim(pdal::Dimension::Id::EdgeOfFlightLine);
    m_pointContext.registerDim(pdal::Dimension::Id::Classification);
    m_originDim =
        m_pointContext.assignDim("OriginId", pdal::Dimension::Type::Unsigned64);

    m_stemPointBuffer.reset(new pdal::PointBuffer(m_pointContext));
}

SleepyTree::~SleepyTree()
{ }

void SleepyTree::insert(const pdal::PointBuffer& pointBuffer, Origin origin)
{
    Point point;
    std::set<LeafNode*> leafSet;
    LeafNode* leaf;
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
                        m_originDim,
                        i,
                        origin));

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

void SleepyTree::save()
{
    std::map<uint64_t, LeafNode*> leafNodes;
    m_tree->finalize(m_stemPointBuffer, leafNodes);
    std::cout << "Saved.  PB size: " << m_stemPointBuffer->size() << std::endl;
}

void SleepyTree::load()
{

}

BBox SleepyTree::getBounds() const
{
    return m_tree->bbox;
}

MultiResults SleepyTree::getPoints(
        const std::size_t depthBegin,
        const std::size_t depthEnd) const
{
    MultiResults results;
    m_tree->getPoints(results, depthBegin, depthEnd);

    return results;
}

MultiResults SleepyTree::getPoints(
        const BBox& bbox,
        const std::size_t depthBegin,
        const std::size_t depthEnd) const
{
    MultiResults results;
    m_tree->getPoints(results, bbox, depthBegin, depthEnd);

    return results;
}

const pdal::PointContext& SleepyTree::pointContext() const
{
    return m_pointContext;
}

std::shared_ptr<pdal::PointBuffer> SleepyTree::pointBuffer(uint64_t id) const
{
    // TODO Support leaf nodes.
    return m_stemPointBuffer;
}

