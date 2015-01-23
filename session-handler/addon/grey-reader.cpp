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

GreyReader::GreyReader()
    : m_initialized(false)
    , m_meta()
    , m_idIndex()
    , m_pointContext()
{ }

void GreyReader::init(GreyMeta meta)
{
    m_meta = meta;

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

    m_initialized = true;
}

GreyReader::~GreyReader()
{ }

GreyQuery GreyReader::query(
        std::size_t depthBegin,
        std::size_t depthEnd)
{
    NodeInfoMap nodeInfoMap;
    m_idIndex->find(nodeInfoMap, depthBegin, depthEnd);

    const std::vector<std::size_t> missingIds(processNodeInfo(nodeInfoMap));
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

    const std::vector<std::size_t> missingIds(processNodeInfo(nodeInfoMap));
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

    const std::vector<std::size_t> missingIds(processNodeInfo(nodeInfoMap));
    queryClusters(nodeInfoMap, missingIds);
    addToCache(nodeInfoMap);

    const std::size_t points(initRaster(rasterize, rasterMeta, m_meta.bbox));

    return GreyQuery(nodeInfoMap, rasterize, rasterMeta, points);
}

std::vector<std::size_t> GreyReader::processNodeInfo(NodeInfoMap& nodeInfoMap)
{
    std::vector<std::size_t> missingIds;
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
                // We don't have anything for this ID yet.  Add it to the list
                // to be queried from the serialized source.
                missingIds.push_back(id);
            }
        }
    }
    catch (...)
    {
        m_cacheMutex.unlock();
        throw std::runtime_error("Error occurred during serial read.");
    }

    m_cacheMutex.unlock();
    return missingIds;
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

bool GreyReader::exists(std::string pipelineId, const SerialPaths& serialPaths)
{
    return (
            GreyReaderSqlite::exists(pipelineId, serialPaths.diskPaths) ||
            GreyReaderS3::exists(pipelineId, serialPaths.s3Info));
}

GreyReaderSqlite::GreyReaderSqlite(
        const std::string pipelineId,
        const SerialPaths& serialPaths)
    : m_db(0)
{
    bool opened(false);

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

    GreyMeta meta;

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

        meta.version =
            reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));

        meta.base = sqlite3_column_int64(stmt, 1);

        meta.pointContextXml =
            reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));

        meta.bbox.xMin = sqlite3_column_double(stmt, 3);
        meta.bbox.yMin = sqlite3_column_double(stmt, 4);
        meta.bbox.xMax = sqlite3_column_double(stmt, 5);
        meta.bbox.yMax = sqlite3_column_double(stmt, 6);
        meta.numPoints = sqlite3_column_int64 (stmt, 7);
        meta.schema =
            reinterpret_cast<const char*>(sqlite3_column_text(stmt, 8));
        meta.compressed = sqlite3_column_int(stmt, 9);
        meta.stats =
            reinterpret_cast<const char*>(sqlite3_column_text(stmt, 10));
        meta.srs =
            reinterpret_cast<const char*>(sqlite3_column_text(stmt, 11));

        const char* rawFills(
                reinterpret_cast<const char*>(
                    sqlite3_column_blob(stmt, 12)));
        const std::size_t rawBytes(sqlite3_column_bytes(stmt, 12));

        meta.fills.resize(
                reinterpret_cast<const std::size_t*>(rawFills + rawBytes) -
                reinterpret_cast<const std::size_t*>(rawFills));

        std::memcpy(meta.fills.data(), rawFills, rawBytes);
    }

    sqlite3_finalize(stmt);
    init(meta);
}

bool GreyReaderSqlite::exists(
        const std::string pipelineId,
        const std::vector<std::string>& diskPaths)
{
    bool exists(false);

    for (const auto& path : diskPaths)
    {
        std::ifstream stream(path + "/" + pipelineId + ".grey");
        if (stream) exists = true;
        stream.close();
        if (exists) break;
    }

    return exists;
}

GreyReaderSqlite::~GreyReaderSqlite()
{
    if (m_db)
    {
        sqlite3_close_v2(m_db);
    }
}

void GreyReaderSqlite::queryClusters(
        NodeInfoMap& nodeInfoMap,
        const std::vector<std::size_t>& missingIds)
{
    std::ostringstream query;

    for (std::size_t i(0); i < missingIds.size(); ++i)
    {
        if (query.str().size()) query << ",";
        query << missingIds[i];
    }

    // If there are any IDs required for this query that we don't already have
    // in our cache, query them now.
    if (query.str().size())
    {
        const std::string sqlDataQuery(
                "SELECT id, fullSize, data FROM clusters WHERE id IN (" +
                query.str() +
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

            std::vector<uint8_t> uncompressedData(
                    meta().compressed ? uncompressedSize : 0);

            if (meta().compressed)
            {
                CompressionStream compressionStream(tableData);

                pdal::LazPerfDecompressor<CompressionStream> decompressor(
                        compressionStream,
                        pointContext().dimTypes());

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

            if (!pointContext().pointSize())
            {
                throw std::runtime_error("Invalid serial PointContext");
            }

            std::shared_ptr<pdal::PointBuffer> pointBuffer(
                    new pdal::PointBuffer(
                        stream,
                        pointContext(),
                        0,
                        data.size() / pointContext().pointSize()));

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

GreyReaderS3::GreyReaderS3(
        const std::string pipelineId,
        const SerialPaths& serialPaths)
    : m_s3(
            serialPaths.s3Info.awsAccessKeyId,
            serialPaths.s3Info.awsSecretAccessKey,
            serialPaths.s3Info.baseAwsUrl,
            serialPaths.s3Info.bucketName)
    , m_pipelineId(pipelineId)
{
    if (serialPaths.s3Info.exists)
    {
        HttpResponse res(m_s3.get(pipelineId + "/meta"));
        if (res.code() == 200)
        {
            Json::Reader jsonReader;
            Json::Value jsonMeta;
            const std::vector<uint8_t>& rawMeta(res.data());

            if (jsonReader.parse(
                    reinterpret_cast<const char*>(rawMeta.data()),
                    reinterpret_cast<const char*>(
                        rawMeta.data() + rawMeta.size()),
                    jsonMeta))
            {
                init(GreyMeta(jsonMeta));
            }
            else
            {
                throw std::runtime_error("Could not parse metadata from S3");
            }
        }
        else
        {
            throw std::runtime_error("Could not get metadata from S3");
        }
    }
    else
    {
        throw std::runtime_error("No S3 credentials supplied");
    }
}

bool GreyReaderS3::exists(const std::string pipelineId, const S3Info& s3Info)
{
    bool exists(false);

    if (s3Info.exists)
    {
        S3 s3(
                s3Info.awsAccessKeyId,
                s3Info.awsSecretAccessKey,
                s3Info.baseAwsUrl,
                s3Info.bucketName);

        if (s3.get(pipelineId + "/meta").code() == 200)
        {
            exists = true;
        }
    }

    return exists;
}

void GreyReaderS3::queryClusters(
        NodeInfoMap& nodeInfoMap,
        const std::vector<std::size_t>& missingIds)
{
    for (const auto& id : missingIds)
    {
        HttpResponse res(m_s3.get(m_pipelineId + "/" + std::to_string(id)));
        if (res.code() != 200) continue;

        std::uint64_t uncompressedSize(0);
        std::memcpy(&uncompressedSize, res.data().data(), sizeof(uint64_t));

        const uint8_t* rawData(res.data().data() + sizeof(uint64_t));
        const std::size_t rawNumBytes(res.data().size() - sizeof(uint64_t));

        // Data from the table may be compressed.
        std::vector<uint8_t> tableData(rawNumBytes);
        std::memcpy(tableData.data(), rawData, rawNumBytes);
        std::vector<uint8_t>& data(tableData);

        std::vector<uint8_t> uncompressedData(
                meta().compressed ? uncompressedSize : 0);
        if (meta().compressed)
        {
            CompressionStream compressionStream(tableData);

            pdal::LazPerfDecompressor<CompressionStream> decompressor(
                    compressionStream,
                    pointContext().dimTypes());

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

        if (!pointContext().pointSize())
        {
            throw std::runtime_error("Invalid serial PointContext");
        }

        std::shared_ptr<pdal::PointBuffer> pointBuffer(
                new pdal::PointBuffer(
                    stream,
                    pointContext(),
                    0,
                    data.size() / pointContext().pointSize()));

        auto it(nodeInfoMap.find(id));
        if (it != nodeInfoMap.end())
        {
            // The populate() method will perform indexing on this cluster
            // if necessary.
            it->second.cluster->populate(pointBuffer, !it->second.complete);
        }
    }
}

GreyReader* GreyReaderFactory::create(
        const std::string pipelineId,
        const SerialPaths& serialPaths)
{
    GreyReader* reader(0);
    try
    {
        if (
                serialPaths.s3Info.exists &&
                GreyReaderS3::exists(pipelineId, serialPaths.s3Info))
        {
            try
            {
                reader = new GreyReaderS3(pipelineId, serialPaths);
                std::cout << "Awoke from S3: " << pipelineId << std::endl;
            }
            catch (...)
            {
                std::cout << "Couldn't awaken S3: " << pipelineId << std::endl;
                reader = 0;
            }
        }

        if (
                !reader &&
                GreyReaderSqlite::exists(pipelineId, serialPaths.diskPaths))
        {
            try
            {
                reader = new GreyReaderSqlite(pipelineId, serialPaths);
                std::cout << "Awoke from sqlite: " << pipelineId << std::endl;
            }
            catch (...)
            {
                std::cout << "Couldn't awaken sq3: " << pipelineId << std::endl;
                reader = 0;
            }
        }
    }
    catch (...)
    {
        std::cout << "Caught exception in create" << std::endl;
        reader = 0;
    }

    return reader;
}

