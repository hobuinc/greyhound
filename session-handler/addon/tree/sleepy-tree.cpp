#include <limits>
#include <cmath>
#include <memory>
#include <string>
#include <thread>

#include <pdal/PointBuffer.hpp>
#include <pdal/Dimension.hpp>
#include <pdal/Utils.hpp>

#include "node.hpp"

namespace
{
    pdal::PointContext initPointContext(const Schema& schema)
    {
        // TODO Get from schema.
        pdal::PointContext pointContext;
        pointContext.registerDim(pdal::Dimension::Id::X);
        pointContext.registerDim(pdal::Dimension::Id::Y);
        pointContext.registerDim(pdal::Dimension::Id::Z);
        pointContext.registerDim(pdal::Dimension::Id::ScanAngleRank);
        pointContext.registerDim(pdal::Dimension::Id::Intensity);
        pointContext.registerDim(pdal::Dimension::Id::PointSourceId);
        pointContext.registerDim(pdal::Dimension::Id::ReturnNumber);
        pointContext.registerDim(pdal::Dimension::Id::NumberOfReturns);
        pointContext.registerDim(pdal::Dimension::Id::ScanDirectionFlag);
        pointContext.registerDim(pdal::Dimension::Id::EdgeOfFlightLine);
        pointContext.registerDim(pdal::Dimension::Id::Classification);
        return pointContext;
    }
}

PointInfo::PointInfo(const Point* point)
    : point(point)
{ }

PointInfoNative::PointInfoNative(
        const pdal::PointBuffer& pointBuffer,
        std::size_t index,
        const pdal::Dimension::Id::Enum originDim,
        const Origin& origin)
    : PointInfo(new Point(
            pointBuffer.getFieldAs<double>(pdal::Dimension::Id::X, index),
            pointBuffer.getFieldAs<double>(pdal::Dimension::Id::Y, index)))
    , pointBuffer(pointBuffer)
    , index(index)
    , originDim(originDim)
    , origin(origin)
{ }

void PointInfoNative::write(
        pdal::PointBuffer* dstPointBuffer,
        std::size_t dstIndex)
{
    std::vector<char> bytes(8);
    char* pos(bytes.data());
    for (const auto& dim : dstPointBuffer->dimTypes())
    {
        // Not all dimensions may be present in every pipeline of our
        // invokation, which is not an error.
        if (pointBuffer.hasDim(dim.m_id))
        {
            //pointBuffer.getField(pos, dim.m_id, dim.m_type, index);
            pointBuffer.getRawField(dim.m_id, index, pos);
            dstPointBuffer->setField(dim.m_id, dim.m_type, dstIndex, pos);
        }
    }

    // Set our custom origin value.
    dstPointBuffer->setField(originDim, dstIndex, origin);
}

PointInfoPulled::PointInfoPulled(
        const Point* point,
        const pdal::PointBuffer* srcPointBuffer,
        std::size_t index)
    : PointInfo(point)
    , bytes(srcPointBuffer->pointSize())
{
    srcPointBuffer->context().rawPtBuf()->getPoint(index, bytes.data());
}

void PointInfoPulled::write(
        pdal::PointBuffer* dstPointBuffer,
        std::size_t dstIndex)
{
    dstPointBuffer->context().rawPtBuf()->setPoint(dstIndex, bytes.data());
}

SleepyTree::SleepyTree(
        const BBox& bbox,
        const Schema& schema,
        std::size_t overflowDepth)
    : m_overflowDepth(overflowDepth)
    , m_pointContext(initPointContext(schema))
    , m_originDim(m_pointContext.assignDim(
                "OriginId",
                pdal::Dimension::Type::Unsigned64))
    , m_basePointBuffer(new pdal::PointBuffer(m_pointContext))
    , m_tree(new StemNode(
                m_basePointBuffer.get(),
                bbox.encapsulate(),
                m_overflowDepth))
    , m_numPoints(0)
{ }

SleepyTree::~SleepyTree()
{ }

void SleepyTree::insert(const pdal::PointBuffer* pointBuffer, Origin origin)
{
    Point point;
    std::set<LeafNode*> leafSet;
    LeafNode* leaf;

    for (std::size_t i = 0; i < pointBuffer->size(); ++i)
    {
        ++m_numPoints;
        point.x = pointBuffer->getFieldAs<double>(pdal::Dimension::Id::X, i);
        point.y = pointBuffer->getFieldAs<double>(pdal::Dimension::Id::Y, i);

        if (m_tree->bbox.contains(point))
        {
            PointInfo* pointInfo(
                    new PointInfoNative(
                        *pointBuffer,
                        i,
                        m_originDim,
                        origin));

            leaf = m_tree->addPoint(
                    m_basePointBuffer.get(),
                    &pointInfo);

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
    //std::map<uint64_t, LeafNode*> leafNodes;
    //m_tree->finalize(m_basePointBuffer, leafNodes);
    std::cout << "Done: " << m_numPoints << " points." << std::endl;
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

pdal::PointBuffer* SleepyTree::pointBuffer(uint64_t id) const
{
    // TODO Support leaf nodes.
    return m_basePointBuffer.get();
}

