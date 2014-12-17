#include <thread>
#include <fstream>

#include <sqlite3.h>
#include <boost/property_tree/json_parser.hpp>

#include <pdal/PipelineReader.hpp>
#include <pdal/PDALUtils.hpp>
#include <pdal/Utils.hpp>
#include <pdal/Dimension.hpp>
#include <pdal/Options.hpp>
#include <pdal/Metadata.hpp>
#include <pdal/QuadIndex.hpp>
#include <pdal/XMLSchema.hpp>

#include "read-command.hpp"
#include "live-data-source.hpp"
#include "grey-common.hpp"
#include "grey-reader.hpp"
#include "grey-writer.hpp"

namespace
{
    const std::string sqlCreateTable(
            "CREATE TABLE clusters("
                "id INT PRIMARY KEY,"
                "xMin           REAL,"
                "yMin           REAL,"
                "xMax           REAL,"
                "yMax           REAL,"
                "depthBegin     INT,"
                "depthEnd       INT,"
                "data           BLOB)");

    const std::string sqlPrep(
            "INSERT INTO clusters VALUES ("
                "@id,@xMin,@yMin,@xMax,@yMax,@depthBegin,@depthEnd,@data)");

    const std::vector<std::string> sqlCreateIndex(
    {
        "CREATE INDEX xMin ON clusters (xMin)",
        "CREATE INDEX yMin ON clusters (yMin)",
        "CREATE INDEX xMax ON clusters (xMax)",
        "CREATE INDEX yMax ON clusters (yMax)",
        "CREATE INDEX depthBegin ON clusters (depthBegin)",
        "CREATE INDEX depthEnd ON clusters (depthEnd)"
    });

    bool rasterOmit(pdal::Dimension::Id::Enum id)
    {
        // These Dimensions are not explicitly placed in the output buffer
        // for rasterized requests.
        return id == pdal::Dimension::Id::X || id == pdal::Dimension::Id::Y;
    }
}

LiveDataSource::LiveDataSource(
        const std::string& pipelineId,
        const std::string& pipeline,
        const bool execute)
    : m_pipelineId(pipelineId)
    , m_pipelineManager()
    , m_pointBuffer()
    , m_pointContext()
    , m_initOnce()
    , m_pdalIndex(new PdalIndex())
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
                    "Pipeline output should contain X, Y, and Z dimensions");
            }
        }
    });
}

void LiveDataSource::ensureIndex(PdalIndex::IndexType indexType)
{
    m_pdalIndex->ensureIndex(indexType, m_pointBuffer);
}

std::size_t LiveDataSource::getNumPoints() const
{
    return m_pointBuffer->size();
}

std::string LiveDataSource::getSchema() const
{
    std::ostringstream oss;
    boost::property_tree::ptree tree(pdal::utils::toPTree(m_pointContext));
    boost::property_tree::write_json(oss, tree);
    return oss.str();
}

std::string LiveDataSource::getStats() const
{
    return m_pipelineManager.getMetadata().toJSON();
}

std::string LiveDataSource::getSrs() const
{
    return m_pointContext.spatialRef().getRawWKT();
}

std::vector<std::size_t> LiveDataSource::getFills() const
{
    m_pdalIndex->ensureIndex(PdalIndex::QuadIndex, m_pointBuffer);
    const pdal::QuadIndex& quadIndex(m_pdalIndex->quadIndex());
    return quadIndex.getFills();
}

void LiveDataSource::serialize()
{
    m_serializeOnce.ensure([this]() {
        if (GreyReader::exists(m_pipelineId))
        {
            return;
        }

        // Serialize the pdal::PointContext.
        pdal::Dimension::IdList dims(m_pointContext.dims());
        std::vector<pdal::Dimension::Type::Enum> dimTypes;

        for (auto dim(dims.begin()); dim != dims.end(); ++dim)
        {
            dimTypes.push_back(m_pointContext.dimType(*dim));
        }

        pdal::schema::Writer writer(dims, dimTypes);
        pdal::Metadata metadata;
        pdal::MetadataNode metaNode = metadata.getNode();
        writer.setMetadata(metaNode);

        m_pdalIndex->ensureIndex(PdalIndex::QuadIndex, m_pointBuffer);
        const pdal::QuadIndex& quadIndex(m_pdalIndex->quadIndex());

        // Data storage.

        GreyMeta meta;
        meta.pointContextXml = writer.getXML();
        meta.base = 8;
        quadIndex.getBounds(
                meta.bbox.xMin,
                meta.bbox.yMin,
                meta.bbox.xMax,
                meta.bbox.yMax);
        meta.numPoints = getNumPoints();
        meta.schema = getSchema();
        meta.stats = getStats();
        meta.srs = getSrs();
        meta.fills = getFills();

        GreyWriter greyWriter(quadIndex, meta);
        greyWriter.write(m_pipelineId + ".grey"); // TODO Path.
    });
}

std::size_t LiveDataSource::readUnindexed(
        std::vector<unsigned char>& buffer,
        const Schema& schema,
        const std::size_t start,
        const std::size_t count)
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

std::size_t LiveDataSource::read(
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

std::size_t LiveDataSource::read(
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

std::size_t LiveDataSource::read(
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

std::size_t LiveDataSource::read(
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

std::size_t LiveDataSource::read(
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

std::size_t LiveDataSource::readDim(
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

std::size_t LiveDataSource::readIndexList(
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

