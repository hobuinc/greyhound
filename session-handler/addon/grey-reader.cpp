#include <pdal/PointBuffer.hpp>
#include <pdal/XMLSchema.hpp>
#include <pdal/Dimension.hpp>
#include <pdal/Charbuf.hpp>
#include <pdal/Utils.hpp>
#include <pdal/PDALUtils.hpp>
#include <pdal/Compression.hpp>

#include "compression-stream.hpp"
#include "grey-reader.hpp"
#include "read-command.hpp"

namespace
{
    const std::string sqlQueryMeta(
            "SELECT "
                "version, base, pointContext, xMin, yMin, xMax, yMax, "
                "numPoints, schema, compressed, stats, srs, fills "
            "FROM meta");

    // Returns the number of points in the raster.
    std::size_t initRaster(
            std::size_t rasterize,
            RasterMeta& rasterMeta,
            const BBox& bbox)
    {
        const std::size_t exp(std::pow(2, rasterize));
        const double xWidth(bbox.xMax - bbox.xMin);
        const double yWidth(bbox.yMax - bbox.yMin);

        rasterMeta.xStep =  xWidth / exp;
        rasterMeta.yStep =  yWidth / exp;
        rasterMeta.xBegin = bbox.xMin + (rasterMeta.xStep / 2);
        rasterMeta.yBegin = bbox.yMin + (rasterMeta.yStep / 2);
        rasterMeta.xEnd =   bbox.xMax + (rasterMeta.xStep / 2);
        rasterMeta.yEnd =   bbox.yMax + (rasterMeta.yStep / 2);

        return exp * exp;
    }

}

GreyReader::GreyReader(
        const std::string pipelineId,
        const SerialPaths& serialPaths)
    : m_db(0)
    , m_meta()
    , m_idIndex()
    , m_pointContext()
{
    bool opened(false);

    // TODO Check s3.

    for (const auto& path : serialPaths.diskPaths)
    {
        if (sqlite3_open_v2(
                    (path + "/" + pipelineId + ".grey").c_str(),
                    &m_db,
                    SQLITE_OPEN_READONLY,
                    0) == SQLITE_OK)
        {
            opened = true;
            break;
        }
        else
        {
            if (m_db) sqlite3_close_v2(m_db);
        }
    }

    if (!opened)
    {
        throw std::runtime_error("Could not open serial DB " + pipelineId);
    }

    readMeta();

    m_idIndex.reset(new IdIndex(m_meta));

    pdal::XMLSchema reader(m_meta.pointContextXml);
    pdal::MetadataNode metaNode = reader.getMetadata();

    m_pointContext.registerDim(pdal::Dimension::Id::X);
    m_pointContext.registerDim(pdal::Dimension::Id::Y);
    m_pointContext.registerDim(pdal::Dimension::Id::Z);

    const pdal::XMLDimList dims(reader.xmlDims());

    for (auto dim(dims.begin()); dim != dims.end(); ++dim)
    {
        m_pointContext.registerOrAssignDim(dim->m_name, dim->m_dimType.m_type);
    }
}

GreyReader::~GreyReader()
{
    if (m_db)
    {
        sqlite3_close_v2(m_db);
    }
}

bool GreyReader::exists(
        const std::string pipelineId,
        const SerialPaths& serialPaths)
{
    bool exists(false);

    if (serialPaths.s3Info.exists)
    {
        const S3Info& s3Info(serialPaths.s3Info);
        S3 s3(
                s3Info.awsAccessKeyId,
                s3Info.awsSecretAccessKey,
                s3Info.baseAwsUrl,
                s3Info.bucketName);

        if (s3.get(pipelineId + "/meta/version").code() == 200)
        {
            std::cout << "Found s3 store: " << pipelineId << std::endl;
            return true;
        }
    }

    for (const auto& path : serialPaths.diskPaths)
    {
        std::ifstream stream(path + "/" + pipelineId + ".grey");
        if (stream) exists = true;
        stream.close();
        if (exists) break;
    }

    return exists;
}

void GreyReader::readMeta()
{
    sqlite3_stmt* stmt(0);
    sqlite3_prepare_v2(
            m_db,
            sqlQueryMeta.c_str(),
            sqlQueryMeta.size() + 1,
            &stmt,
            0);

    if (!stmt)
    {
        throw std::runtime_error("Could not prepare SELECT stmt - meta");
    }

    int metaRows(0);
    while (sqlite3_step(stmt) == SQLITE_ROW && ++metaRows)
    {
        if (metaRows > 1)
        {
            throw std::runtime_error("Multiple metadata rows detected");
        }

        m_meta.version =
            reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));

        m_meta.base = sqlite3_column_int64(stmt, 1);

        m_meta.pointContextXml =
            reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));

        m_meta.bbox.xMin = sqlite3_column_double(stmt, 3);
        m_meta.bbox.yMin = sqlite3_column_double(stmt, 4);
        m_meta.bbox.xMax = sqlite3_column_double(stmt, 5);
        m_meta.bbox.yMax = sqlite3_column_double(stmt, 6);
        m_meta.numPoints = sqlite3_column_int64 (stmt, 7);
        m_meta.schema =
            reinterpret_cast<const char*>(sqlite3_column_text(stmt, 8));
        m_meta.compressed = sqlite3_column_int(stmt, 9);
        m_meta.stats =
            reinterpret_cast<const char*>(sqlite3_column_text(stmt, 10));
        m_meta.srs =
            reinterpret_cast<const char*>(sqlite3_column_text(stmt, 11));

        const char* rawFills(
                reinterpret_cast<const char*>(
                    sqlite3_column_blob(stmt, 12)));
        const std::size_t rawBytes(sqlite3_column_bytes(stmt, 12));

        m_meta.fills.resize(
                reinterpret_cast<const std::size_t*>(rawFills + rawBytes) -
                reinterpret_cast<const std::size_t*>(rawFills));

        std::memcpy(m_meta.fills.data(), rawFills, rawBytes);
    }

    sqlite3_finalize(stmt);
}

GreyQuery GreyReader::query(
        std::size_t depthBegin,
        std::size_t depthEnd)
{
    NodeInfoMap nodeInfoMap;
    m_idIndex->find(nodeInfoMap, depthBegin, depthEnd);

    const std::string missingIds(processNodeInfo(nodeInfoMap));
    queryClusters(nodeInfoMap, missingIds);
    addToCache(nodeInfoMap);

    return GreyQuery(nodeInfoMap, depthBegin, depthEnd);
}

GreyQuery GreyReader::query(
        double xMin,
        double yMin,
        double xMax,
        double yMax,
        std::size_t depthBegin,
        std::size_t depthEnd)
{
    const BBox bbox(xMin, yMin, xMax, yMax);

    NodeInfoMap nodeInfoMap;
    m_idIndex->find(nodeInfoMap, depthBegin, depthEnd, bbox);

    const std::string missingIds(processNodeInfo(nodeInfoMap));
    queryClusters(nodeInfoMap, missingIds);
    addToCache(nodeInfoMap);

    return GreyQuery(nodeInfoMap, bbox, depthBegin, depthEnd);
}

GreyQuery GreyReader::query(
        const std::size_t rasterize,
        RasterMeta& rasterMeta)
{
    NodeInfoMap nodeInfoMap;
    m_idIndex->find(nodeInfoMap, rasterize, rasterize + 1);

    const std::string missingIds(processNodeInfo(nodeInfoMap));
    queryClusters(nodeInfoMap, missingIds);
    addToCache(nodeInfoMap);

    const std::size_t points(initRaster(rasterize, rasterMeta, m_meta.bbox));

    return GreyQuery(nodeInfoMap, rasterize, rasterMeta, points);
}

std::string GreyReader::processNodeInfo(NodeInfoMap& nodeInfoMap)
{
    std::ostringstream missingIds;
    m_cacheMutex.lock();
    try
    {
        for (auto& node : nodeInfoMap)
        {
            const uint64_t id(node.first);

            auto cacheEntry(m_cache.find(id));
            if (cacheEntry != m_cache.end())
            {
                std::shared_ptr<GreyCluster> cluster(cacheEntry->second);

                // We already have this ID/cluster cached.  Point to it in our
                // info map, and index it if necessary.
                node.second.cluster = cluster;

                if (!node.second.complete)
                {
                    cluster->index();
                }
            }
            else
            {
                // We don't have anything for this ID yet.  Query it from
                // the serialized data file.
                if (missingIds.str().size()) missingIds << ",";
                missingIds << id;
            }
        }
    }
    catch (...)
    {
        m_cacheMutex.unlock();
        throw std::runtime_error("Error occurred during serial read.");
    }

    m_cacheMutex.unlock();
    return missingIds.str();
}

void GreyReader::queryClusters(
        NodeInfoMap& nodeInfoMap,
        const std::string& missingIds) const
{
    // If there are any IDs required for this query that we don't already have
    // in our cache, query them now.
    if (missingIds.size())
    {
        const std::string sqlDataQuery(
                "SELECT id, fullSize, data FROM clusters WHERE id IN (" +
                missingIds +
                ")");

        sqlite3_stmt* stmtData(0);
        sqlite3_prepare_v2(
                m_db,
                sqlDataQuery.c_str(),
                sqlDataQuery.size() + 1,
                &stmtData,
                0);

        if (!stmtData)
        {
            throw std::runtime_error("Could not prepare SELECT stmt - data");
        }

        while (sqlite3_step(stmtData) == SQLITE_ROW)
        {
            const int id(sqlite3_column_int64(stmtData, 0));
            const std::size_t uncompressedSize(
                    sqlite3_column_int(stmtData, 1));

            const uint8_t* rawData(
                    reinterpret_cast<const uint8_t*>(
                        sqlite3_column_blob(stmtData, 2)));
            const std::size_t rawNumBytes(sqlite3_column_bytes(stmtData, 2));

            // Data from the table may be compressed.
            std::vector<uint8_t> tableData(rawNumBytes);
            std::memcpy(tableData.data(), rawData, rawNumBytes);
            std::vector<uint8_t>& data(tableData);

            if (m_meta.compressed)
            {
                CompressionStream compressionStream(tableData);
                std::vector<uint8_t> uncompressedData(uncompressedSize);

                pdal::LazPerfDecompressor<CompressionStream> decompressor(
                        compressionStream,
                        m_pointContext.dimTypes());

                decompressor.decompress(
                        reinterpret_cast<char*>(uncompressedData.data()),
                        uncompressedSize);

                data = uncompressedData;
            }

            // Construct a pdal::PointBuffer from our binary data.
            pdal::Charbuf charbuf(
                    reinterpret_cast<char*>(data.data()),
                    data.size());
            std::istream stream(&charbuf);

            if (!m_pointContext.pointSize())
            {
                throw std::runtime_error("Invalid serial PointContext");
            }

            std::shared_ptr<pdal::PointBuffer> pointBuffer(
                    new pdal::PointBuffer(
                        stream,
                        m_pointContext,
                        0,
                        data.size() / m_pointContext.pointSize()));

            auto it(nodeInfoMap.find(id));
            if (it != nodeInfoMap.end())
            {
                // The populate() method will perform indexing on this cluster
                // if necessary.
                it->second.cluster->populate(pointBuffer, !it->second.complete);
            }
        }

        sqlite3_finalize(stmtData);
    }
}

void GreyReader::addToCache(const NodeInfoMap& nodeInfoMap)
{
    m_cacheMutex.lock();
    try
    {
        for (auto& entry : nodeInfoMap)
        {
            const std::shared_ptr<GreyCluster> cluster(entry.second.cluster);
            if (cluster->populated())
            {
                m_cache.insert(std::make_pair(entry.first, cluster));
            }
        }
    }
    catch (...)
    {
        m_cacheMutex.unlock();
        throw std::runtime_error("Error occurred during cache insertion.");
    }
    m_cacheMutex.unlock();
}

