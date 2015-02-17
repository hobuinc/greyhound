#include <limits>
#include <cmath>
#include <memory>
#include <string>
#include <thread>

#include <pdal/Charbuf.hpp>
#include <pdal/Compression.hpp>
#include <pdal/Dimension.hpp>
#include <pdal/PointBuffer.hpp>
#include <pdal/Utils.hpp>

#include "compression-stream.hpp"
#include "http/collector.hpp"
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
        pointContext.registerDim(pdal::Dimension::Id::Classification);
        return pointContext;
    }

    // TODO
    const std::string diskPath("/var/greyhound/serial");
}

PointInfo::PointInfo(
        const pdal::PointContextRef pointContext,
        const pdal::PointBuffer* pointBuffer,
        const std::size_t index,
        const pdal::Dimension::Id::Enum originDim,
        const Origin origin)
    : point(new Point(
            pointBuffer->getFieldAs<double>(pdal::Dimension::Id::X, index),
            pointBuffer->getFieldAs<double>(pdal::Dimension::Id::Y, index)))
    , bytes(pointBuffer->pointSize())
{
    char* pos(bytes.data());
    for (const auto& dim : pointContext.dims())
    {
        // Not all dimensions may be present in every pipeline of our
        // invokation, which is not an error.
        if (pointBuffer->hasDim(dim))
        {
            pointBuffer->getRawField(dim, index, pos);
        }
        else if (dim == originDim)
        {
            std::memcpy(pos, &origin, sizeof(Origin));
        }

        pos += pointContext.dimSize(dim);
    }
}

PointInfo::PointInfo(const Point* point, char* pos, const std::size_t len)
    : point(point)
    , bytes(len)
{
    std::memcpy(bytes.data(), pos, len);
}

void PointInfo::write(char* pos)
{
    std::memcpy(pos, bytes.data(), bytes.size());
}

SleepyTree::SleepyTree(
        const std::string& pipelineId,
        const BBox& bbox,
        const Schema& schema)
    : m_pipelineId(pipelineId)
    , m_bbox(bbox)
    , m_pointContext(initPointContext(schema))
    , m_originDim(m_pointContext.assignDim(
                "OriginId",
                pdal::Dimension::Type::Unsigned64))
    , m_numPoints(0)
    , m_tree(new Sleeper(m_bbox, m_pointContext.pointSize()))
{ }

SleepyTree::~SleepyTree()
{ }

void SleepyTree::insert(const pdal::PointBuffer* pointBuffer, Origin origin)
{
    Point point;

    for (std::size_t i = 0; i < pointBuffer->size(); ++i)
    {
        point.x = pointBuffer->getFieldAs<double>(pdal::Dimension::Id::X, i);
        point.y = pointBuffer->getFieldAs<double>(pdal::Dimension::Id::Y, i);

        if (m_bbox.contains(point))
        {
            PointInfo* pointInfo(
                    new PointInfo(
                        m_pointContext,
                        pointBuffer,
                        i,
                        m_originDim,
                        origin));

            m_tree->addPoint(&pointInfo);
            ++m_numPoints;
        }
    }
}

void SleepyTree::save()
{
    std::string path(diskPath + "/" + m_pipelineId + "/0");
    std::ofstream dataStream;
    dataStream.open(
            path,
            std::ofstream::out | std::ofstream::trunc | std::ofstream::binary);

    const uint64_t uncompressedSize(m_tree->baseData()->size());

    // TODO Duplicate code with Node::compress().
    CompressionStream compressionStream;
    pdal::LazPerfCompressor<CompressionStream> compressor(
            compressionStream,
            m_pointContext.dimTypes());

    compressor.compress(m_tree->baseData()->data(), m_tree->baseData()->size());
    compressor.done();

    std::shared_ptr<std::vector<char>> compressed(
            new std::vector<char>(compressionStream.data().size()));

    std::memcpy(
            compressed->data(),
            compressionStream.data().data(),
            compressed->size());
    const uint64_t compressedSize(compressed->size());

    dataStream.write(
            reinterpret_cast<const char*>(&uncompressedSize),
            sizeof(uint64_t));
    dataStream.write(
            reinterpret_cast<const char*>(&compressedSize),
            sizeof(uint64_t));
    dataStream.write(compressed->data(), compressed->size());
    dataStream.close();
    std::cout << "Done: " << m_numPoints << " points." << std::endl;
}

void SleepyTree::load()
{

}

const BBox& SleepyTree::getBounds() const
{
    return m_bbox;
}

MultiResults SleepyTree::getPoints(
        const std::size_t depthBegin,
        const std::size_t depthEnd)
{
    MultiResults results;
    m_tree->getPoints(
            results,
            depthBegin,
            depthEnd);

    return results;
}

MultiResults SleepyTree::getPoints(
        const BBox& bbox,
        const std::size_t depthBegin,
        const std::size_t depthEnd)
{
    MultiResults results;
    m_tree->getPoints(
            results,
            bbox,
            depthBegin,
            depthEnd);

    return results;
}

const pdal::PointContext& SleepyTree::pointContext() const
{
    return m_pointContext;
}

std::shared_ptr<std::vector<char>> SleepyTree::data(uint64_t id)
{
    return m_tree->baseData();
}

std::size_t SleepyTree::numPoints() const
{
    return m_numPoints;
}

