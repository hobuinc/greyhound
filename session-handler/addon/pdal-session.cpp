#include <thread>

#include <boost/property_tree/json_parser.hpp>

#include <pdal/PipelineReader.hpp>
#include <pdal/PDALUtils.hpp>
#include <pdal/Dimension.hpp>
#include <pdal/Options.hpp>
#include <pdal/Metadata.hpp>

#include "pdal-session.hpp"
#include "read-command.hpp"

namespace
{
    bool rasterOmit(pdal::Dimension::Id::Enum id)
    {
        // These Dimensions are not explicitly placed in the output buffer
        // for rasterized requests.
        return id == pdal::Dimension::Id::X || id == pdal::Dimension::Id::Y;
    }
}

PdalSession::PdalSession()
    : m_pipelineManager()
    , m_pointBuffer()
    , m_initOnce()
    , m_pdalIndex(new PdalIndex())
{ }

void PdalSession::initialize(const std::string& pipeline, const bool execute)
{
    m_initOnce.ensure([this, &pipeline, execute]() {
        std::istringstream ssPipeline(pipeline);
        pdal::PipelineReader pipelineReader(m_pipelineManager);
        pipelineReader.readPipeline(ssPipeline);

        // This segment could take a substantial amount of time.  The
        // PdalBindings wrapper ensures that it will run in a non-blocking
        // manner in the uv_work_queue.
        if (execute)
        {
            m_pipelineManager.addFilter(
                "filters.stats",
                m_pipelineManager.getStage());

            pdal::Options options;
            options.add("do_sample", false);
            m_pipelineManager.getStage()->setOptions(options);

            m_pipelineManager.execute();

            m_pointContext = m_pipelineManager.context();
            const pdal::PointBufferSet& pbSet(m_pipelineManager.buffers());
            m_pointBuffer = *pbSet.begin();

            if (!m_pointBuffer->hasDim(pdal::Dimension::Id::X) ||
                !m_pointBuffer->hasDim(pdal::Dimension::Id::Y) ||
                !m_pointBuffer->hasDim(pdal::Dimension::Id::Z))
            {
                throw std::runtime_error(
                    "Pipeline output should contain X, Y and Z dimensions");
            }
        }
    });
}

std::size_t PdalSession::getNumPoints() const
{
    return m_pointBuffer->size();
}

std::string PdalSession::getSchema() const
{
    std::ostringstream oss;
    boost::property_tree::ptree tree(pdal::utils::toPTree(m_pointContext));
    boost::property_tree::write_json(oss, tree);
    return oss.str();
}

std::string PdalSession::getStats() const
{
    return m_pipelineManager.getMetadata().toJSON();
}

std::string PdalSession::getSrs() const
{
    return m_pointContext.spatialRef().getRawWKT();
}

std::vector<std::size_t> PdalSession::getFills() const
{
    m_pdalIndex->ensureIndex(PdalIndex::QuadIndex, m_pointBuffer);
    const pdal::QuadIndex& quadIndex(m_pdalIndex->quadIndex());
    return quadIndex.getFills();
}

std::size_t PdalSession::readDim(
        unsigned char* buffer,
        const DimInfo& dim,
        const std::size_t index) const
{
    if (dim.type == "floating")
    {
        if (dim.size == 4)
        {
            float val(m_pointBuffer->getFieldAs<float>(dim.id, index));
            std::memcpy(buffer, &val, dim.size);
        }
        else if (dim.size == 8)
        {
            double val(m_pointBuffer->getFieldAs<double>(dim.id, index));
            std::memcpy(buffer, &val, dim.size);
        }
        else
        {
            throw std::runtime_error("Invalid floating size requested");
        }
    }
    else if (dim.type == "signed" || dim.type == "unsigned")
    {
        if (dim.size == 1)
        {
            uint8_t val(m_pointBuffer->getFieldAs<uint8_t>(dim.id, index));
            std::memcpy(buffer, &val, dim.size);
        }
        else if (dim.size == 2)
        {
            uint16_t val(m_pointBuffer->getFieldAs<uint16_t>(dim.id, index));
            std::memcpy(buffer, &val, dim.size);
        }
        else if (dim.size == 4)
        {
            uint32_t val(m_pointBuffer->getFieldAs<uint32_t>(dim.id, index));
            std::memcpy(buffer, &val, dim.size);
        }
        else if (dim.size == 8)
        {
            uint64_t val(m_pointBuffer->getFieldAs<uint64_t>(dim.id, index));
            std::memcpy(buffer, &val, dim.size);
        }
        else
        {
            throw std::runtime_error("Invalid integer size requested");
        }
    }
    else
    {
        throw std::runtime_error("Invalid dimension type requested");
    }

    return dim.size;
}

std::size_t PdalSession::readUnindexed(
        std::vector<unsigned char>& buffer,
        const Schema& schema,
        const std::size_t start,
        const std::size_t count) const
{
    if (start >= getNumPoints())
        throw std::runtime_error("Invalid starting offset in 'read'");

    // If zero points specified, read all points after 'start'.
    const std::size_t pointsToRead(
            count > 0 ?
                std::min<std::size_t>(count, getNumPoints() - start) :
                getNumPoints() - start);

    try
    {
        buffer.resize(pointsToRead * schema.stride());

        unsigned char* pos(buffer.data());

        for (boost::uint32_t i(start); i < start + pointsToRead; ++i)
        {
            for (const auto& dim : schema.dims)
            {
                pos += readDim(pos, dim, i);
            }
        }
    }
    catch (...)
    {
        throw std::runtime_error("Failed to read points from PDAL");
    }

    return pointsToRead;
}

std::size_t PdalSession::read(
        std::vector<unsigned char>& buffer,
        const Schema& schema,
        const double xMin,
        const double yMin,
        const double xMax,
        const double yMax,
        std::size_t depthBegin,
        std::size_t depthEnd)
{
    m_pdalIndex->ensureIndex(PdalIndex::QuadIndex, m_pointBuffer);
    const pdal::QuadIndex& quadIndex(m_pdalIndex->quadIndex());

    const std::vector<std::size_t> results(quadIndex.getPoints(
            xMin,
            yMin,
            xMax,
            yMax,
            depthBegin,
            depthEnd));

    return readIndexList(buffer, schema, results);
}

std::size_t PdalSession::read(
        std::vector<unsigned char>& buffer,
        const Schema& schema,
        const std::size_t depthBegin,
        const std::size_t depthEnd)
{
    m_pdalIndex->ensureIndex(PdalIndex::QuadIndex, m_pointBuffer);
    const pdal::QuadIndex& quadIndex(m_pdalIndex->quadIndex());

    const std::vector<std::size_t> results(quadIndex.getPoints(
            depthBegin,
            depthEnd));

    return readIndexList(buffer, schema, results);
}

std::size_t PdalSession::read(
        std::vector<unsigned char>& buffer,
        const Schema& schema,
        const std::size_t rasterize,
        RasterMeta& rasterMeta)
{
    m_pdalIndex->ensureIndex(PdalIndex::QuadIndex, m_pointBuffer);
    const pdal::QuadIndex& quadIndex(m_pdalIndex->quadIndex());

    const std::vector<std::size_t> results(quadIndex.getPoints(
            rasterize,
            rasterMeta.xBegin,
            rasterMeta.xEnd,
            rasterMeta.xStep,
            rasterMeta.yBegin,
            rasterMeta.yEnd,
            rasterMeta.yStep));

    return readIndexList(buffer, schema, results, true);
}

std::size_t PdalSession::read(
        std::vector<unsigned char>& buffer,
        const Schema& schema,
        const RasterMeta& rasterMeta)
{
    m_pdalIndex->ensureIndex(PdalIndex::QuadIndex, m_pointBuffer);
    const pdal::QuadIndex& quadIndex(m_pdalIndex->quadIndex());

    const std::vector<std::size_t> results(quadIndex.getPoints(
            rasterMeta.xBegin,
            rasterMeta.xEnd,
            rasterMeta.xStep,
            rasterMeta.yBegin,
            rasterMeta.yEnd,
            rasterMeta.yStep));

    return readIndexList(buffer, schema, results, true);
}

std::size_t PdalSession::read(
        std::vector<unsigned char>& buffer,
        const Schema& schema,
        const bool is3d,
        const double radius,
        const double x,
        const double y,
        const double z)
{
    m_pdalIndex->ensureIndex(
            is3d ? PdalIndex::KdIndex3d : PdalIndex::KdIndex2d,
            m_pointBuffer);

    const pdal::KDIndex& kdIndex(
            is3d ? m_pdalIndex->kdIndex3d() : m_pdalIndex->kdIndex2d());

    // KDIndex::radius() takes r^2.
    const std::vector<std::size_t> results(
            kdIndex.radius(x, y, z, radius * radius));

    return readIndexList(buffer, schema, results);
}

std::size_t PdalSession::readIndexList(
        std::vector<unsigned char>& buffer,
        const Schema& schema,
        const std::vector<std::size_t>& indexList,
        const bool rasterize) const
{
    const std::size_t pointsToRead(indexList.size());

    // Rasterization
    std::size_t stride(schema.stride());

    if (rasterize)
    {
        // Clientward rasterization schemas always contain a byte to specify
        // whether a point at this location in the raster exists.
        ++stride;

        for (auto dim : schema.dims)
        {
            if (rasterOmit(dim.id))
            {
                stride -= dim.size;
            }
        }
    }

    try
    {
        buffer.resize(pointsToRead * stride, 0);

        unsigned char* pos(buffer.data());

        for (std::size_t i : indexList)
        {
            if (i != std::numeric_limits<std::size_t>::max())
            {
                if (rasterize)
                {
                    // Mark this point as a valid point.
                    std::fill(pos, pos + 1, 1);
                    ++pos;
                }

                for (const auto& dim : schema.dims)
                {
                    if (!rasterize || !rasterOmit(dim.id))
                    {
                        pos += readDim(pos, dim, i);
                    }
                }
            }
            else
            {
                if (rasterize)
                {
                    // Mark this point as a hole.
                    std::fill(pos, pos + 1, 0);
                }

                pos += stride;
            }
        }
    }
    catch (...)
    {
        throw std::runtime_error("Failed to read points from PDAL");
    }

    return pointsToRead;
}

void PdalIndex::ensureIndex(
        const IndexType indexType,
        const pdal::PointBufferPtr pointBufferPtr)
{
    const pdal::PointBuffer& pointBuffer(*pointBufferPtr.get());

    switch (indexType)
    {
        case KdIndex2d:
            m_kd2dOnce.ensure([this, &pointBuffer]() {
                m_kdIndex2d.reset(new pdal::KDIndex(pointBuffer));
                m_kdIndex2d->build(false);
            });
            break;
        case KdIndex3d:
            m_kd3dOnce.ensure([this, &pointBuffer]() {
                m_kdIndex3d.reset(new pdal::KDIndex(pointBuffer));
                m_kdIndex3d->build(true);
            });
            break;
        case QuadIndex:
            m_quadOnce.ensure([this, &pointBuffer]() {
                m_quadIndex.reset(new pdal::QuadIndex(pointBuffer));
                m_quadIndex->build();
            });
            break;
        default:
            throw std::runtime_error("Invalid PDAL index type");
    }
}

