#include <thread>
#include <fstream>
#include <streambuf>

#include <sqlite3.h>
#include <boost/property_tree/json_parser.hpp>

#include <pdal/PipelineReader.hpp>
#include <pdal/PDALUtils.hpp>
#include <pdal/Utils.hpp>
#include <pdal/Dimension.hpp>
#include <pdal/Options.hpp>
#include <pdal/Metadata.hpp>
#include <pdal/QuadIndex.hpp>
#include <pdal/StageFactory.hpp>
#include <pdal/XMLSchema.hpp>

#include "read-queries/live.hpp"
#include "read-queries/unindexed.hpp"
#include "grey/reader.hpp"
#include "grey/writer.hpp"
#include "tree/sleepy-tree.hpp"
#include "types/raster-meta.hpp"
#include "types/serial-paths.hpp"
#include "types/bbox.hpp"
#include "live.hpp"

LiveDataSource::LiveDataSource(
        const std::string& pipelineId,
        const std::string& filename)
    : m_pipelineId(pipelineId)
    , m_pipelineManager()
    , m_pointBuffer()
    , m_pointContext()
    , m_pdalIndex(new PdalIndex())
{
    exec(filename);
}

void LiveDataSource::exec(const std::string filename)
{
    pdal::StageFactory stageFactory;
    pdal::Options options;

    const std::string driver(stageFactory.inferReaderDriver(filename));

    if (driver.size())
    {
        m_pipelineManager.addReader(driver);

        pdal::Stage* reader(
                static_cast<pdal::Reader*>(m_pipelineManager.getStage()));
        options.add(pdal::Option("filename", filename));
        reader->setOptions(options);
    }
    else
    {
        std::ifstream fileStream(filename);
        const std::string pipeline(
                (std::istreambuf_iterator<char>(fileStream)),
                std::istreambuf_iterator<char>());

        std::istringstream ssPipeline(pipeline);
        pdal::PipelineReader pipelineReader(m_pipelineManager);
        pipelineReader.readPipeline(ssPipeline);

        m_pipelineManager.addFilter(
                "filters.stats",
                m_pipelineManager.getStage());

        options.add("do_sample", false);
        m_pipelineManager.getStage()->setOptions(options);
    }

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
    pdal::MetadataNode pointContextNode(
            pdal::utils::toMetadata(m_pointContext));
    return pdal::utils::toJSON(pointContextNode);
}

std::string LiveDataSource::getStats() const
{
    return pdal::utils::toJSON(m_pipelineManager.getMetadata());
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

void LiveDataSource::serialize(
        const bool compressed,
        const SerialPaths& serialPaths)
{
    m_serializeOnce.ensure([this, compressed, &serialPaths]() {
        std::cout << "Serializing live source " << m_pipelineId << std::endl;
        if (GreyReader::exists(m_pipelineId, serialPaths) ||
            (!serialPaths.diskPaths.size() && !serialPaths.s3Info.exists))
        {
            std::cout << "Already serialized" << std::endl;
            return;
        }

        const pdal::DimTypeList dimTypeList(m_pointContext.dimTypes());
        pdal::Metadata metadata;
        pdal::MetadataNode metaNode = metadata.getNode();
        pdal::XMLSchema writer(dimTypeList, metaNode);

        m_pdalIndex->ensureIndex(PdalIndex::QuadIndex, m_pointBuffer);
        const pdal::QuadIndex& quadIndex(m_pdalIndex->quadIndex());

        Point min, max;
        quadIndex.getBounds(min.x, min.y, max.x, max.y);

        // Data storage.
        GreyMeta meta;
        meta.version = greyVersion;
        meta.pointContextXml = writer.xml();
        meta.bbox.min(min);
        meta.bbox.max(max);
        meta.numPoints = getNumPoints();
        meta.schema = getSchema();
        meta.compressed = compressed;
        meta.stats = getStats();
        meta.srs = getSrs();
        meta.fills = getFills();

        GreyWriter greyWriter(*m_pointBuffer.get(), quadIndex, meta);

        bool written(false);

        if (serialPaths.s3Info.exists)
        {
            std::cout << "Writing to s3 at " <<
                    serialPaths.s3Info.baseAwsUrl << "/" <<
                    serialPaths.s3Info.bucketName << "/" <<
                    m_pipelineId << std::endl;

            try
            {
                greyWriter.write(serialPaths.s3Info, m_pipelineId);
                written = true;
                std::cout << "S3 serialization complete" << std::endl;
            }
            catch (...)
            {
                std::cout << "Error caught in S3 serialize" << std::endl;
            }
        }

        if (!written)
        {
            const std::string filename(
                    serialPaths.diskPaths[0] + "/" + m_pipelineId + ".grey");
            std::cout << "Writing sqlite to disk at " << filename << std::endl;

            try
            {
                greyWriter.write(filename);
                written = true;
                std::cout << "Sqlite serialization complete" << std::endl;
            }
            catch (...)
            {
                std::cout << "Error caught in sqlite serialize" << std::endl;
            }
        }

        if (!written)
        {
            std::cout << "Serialization failed: " << m_pipelineId << std::endl;
        }
    });
}

std::shared_ptr<ReadQuery> LiveDataSource::queryUnindexed(
        const Schema& schema,
        bool compressed,
        const std::size_t start,
        std::size_t count)
{
    // If zero points specified, read all points after 'start'.
    count = count > 0 ?
            std::min<std::size_t>(count, getNumPoints() - start) :
            getNumPoints() - start;

    return std::shared_ptr<ReadQuery>(new UnindexedReadQuery(
                schema,
                compressed,
                m_pointBuffer,
                start,
                count));
}

std::shared_ptr<ReadQuery> LiveDataSource::query(
        const Schema& schema,
        bool compressed,
        const BBox& bbox,
        std::size_t depthBegin,
        std::size_t depthEnd)
{
    m_pdalIndex->ensureIndex(PdalIndex::QuadIndex, m_pointBuffer);
    const pdal::QuadIndex& quadIndex(m_pdalIndex->quadIndex());

    const std::vector<std::size_t> results(quadIndex.getPoints(
            bbox.min().x,
            bbox.min().y,
            bbox.max().x,
            bbox.max().y,
            depthBegin,
            depthEnd));

    return std::shared_ptr<ReadQuery>(
            new LiveReadQuery(
                schema,
                compressed,
                false,
                m_pointBuffer,
                results));
}

std::shared_ptr<ReadQuery> LiveDataSource::query(
        const Schema& schema,
        bool compressed,
        const std::size_t depthBegin,
        const std::size_t depthEnd)
{
    m_pdalIndex->ensureIndex(PdalIndex::QuadIndex, m_pointBuffer);
    const pdal::QuadIndex& quadIndex(m_pdalIndex->quadIndex());

    const std::vector<std::size_t> results(quadIndex.getPoints(
            depthBegin,
            depthEnd));

    return std::shared_ptr<ReadQuery>(
            new LiveReadQuery(
                schema,
                compressed,
                false,
                m_pointBuffer,
                results));
}

std::shared_ptr<ReadQuery> LiveDataSource::query(
        const Schema& schema,
        bool compressed,
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

    return std::shared_ptr<ReadQuery>(
            new LiveReadQuery(
                schema,
                compressed,
                true,
                m_pointBuffer,
                results));
}

std::shared_ptr<ReadQuery> LiveDataSource::query(
        const Schema& schema,
        bool compressed,
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

    return std::shared_ptr<ReadQuery>(
            new LiveReadQuery(
                schema,
                compressed,
                true,
                m_pointBuffer,
                results));
}

std::shared_ptr<ReadQuery> LiveDataSource::query(
        const Schema& schema,
        bool compressed,
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

    return std::shared_ptr<ReadQuery>(
            new LiveReadQuery(
                schema,
                compressed,
                false,
                m_pointBuffer,
                results));
}

