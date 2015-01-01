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
}

LiveDataSource::LiveDataSource(
        const std::string& pipelineId,
        const std::string& pipeline,
        const bool execute)
    : m_pipelineId(pipelineId)
    , m_pipelineManager()
    , m_pointBuffer()
    , m_pointContext()
    , m_pdalIndex(new PdalIndex())
{
    std::istringstream ssPipeline(pipeline);
    pdal::PipelineReader pipelineReader(m_pipelineManager);
    pipelineReader.readPipeline(ssPipeline);

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

void LiveDataSource::serialize(const std::vector<std::string>& serialPaths)
{
    m_serializeOnce.ensure([this, serialPaths]() {
        std::cout << "Serializing live source " << m_pipelineId << std::endl;
        if (GreyReader::exists(m_pipelineId, serialPaths) ||
            !serialPaths.size())
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
        const std::string filename(
                serialPaths.at(0) + "/" + m_pipelineId + ".grey");
        std::cout << "Writing to disk at " << filename << std::endl;
        greyWriter.write(filename);
    });
}

std::size_t LiveDataSource::queryUnindexed(
        const std::size_t start,
        const std::size_t count)
{
    // If zero points specified, read all points after 'start'.
    return count > 0 ?
            std::min<std::size_t>(count, getNumPoints() - start) :
            getNumPoints() - start;
}

std::vector<std::size_t> LiveDataSource::query(
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

    return results;
}

std::vector<std::size_t> LiveDataSource::query(
        const std::size_t depthBegin,
        const std::size_t depthEnd)
{
    m_pdalIndex->ensureIndex(PdalIndex::QuadIndex, m_pointBuffer);
    const pdal::QuadIndex& quadIndex(m_pdalIndex->quadIndex());

    const std::vector<std::size_t> results(quadIndex.getPoints(
            depthBegin,
            depthEnd));

    return results;
}

std::vector<std::size_t> LiveDataSource::query(
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

    return results;
}

std::vector<std::size_t> LiveDataSource::query(const RasterMeta& rasterMeta)
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

    return results;
}

std::vector<std::size_t> LiveDataSource::query(
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

    return results;
}

