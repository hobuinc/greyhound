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
#include "http/collector.hpp"

namespace
{
    const std::string sqlQueryMeta("SELECT json FROM meta");

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

    const std::size_t maxGetRetries(4);
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
            sqlite3_finalize(stmt);
            throw std::runtime_error("Multiple metadata rows detected");
        }

        const std::string rawMeta(
                reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)));

        Json::Reader jsonReader;
        Json::Value jsonMeta;

        if (jsonReader.parse(rawMeta, jsonMeta))
        {
            init(GreyMeta(jsonMeta));
        }
        else
        {
            sqlite3_finalize(stmt);
            throw std::runtime_error("Could not parse metadata from sqlite");
        }
    }

    sqlite3_finalize(stmt);
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
            const std::vector<uint8_t>* rawMeta(res.data());

            if (jsonReader.parse(
                    reinterpret_cast<const char*>(rawMeta->data()),
                    reinterpret_cast<const char*>(
                        rawMeta->data() + rawMeta->size()),
                    jsonMeta))
            {
                init(GreyMeta(jsonMeta));
                delete res.data();
            }
            else
            {
                delete res.data();
                throw std::runtime_error("Could not parse metadata from S3");
            }
        }
        else
        {
            delete res.data();
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

        HttpResponse metaRes(s3.get(pipelineId + "/meta"));
        HttpResponse zeroRes(s3.get(pipelineId + "/0"));
        delete metaRes.data();
        delete zeroRes.data();

        if (metaRes.code() == 200 && zeroRes.code() == 200)
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
    std::size_t i(0);
    std::shared_ptr<GetCollector> collector(
            new GetCollector(missingIds.size()));

    for (const auto& id : missingIds)
    {
        const std::string file(m_pipelineId + "/" + std::to_string(id));
        m_s3.get(id, file, collector.get());

        if (++i % 128 == 0)
        {
            // Don't get too far ahead of actual responses, or we'll create
            // lots of idle threads waiting for a CurlBatch entry to become
            // available.
            collector->waitFor(i - 64);
        }
    }

    // Blocks until all responses are received, and clears the Collector of its
    // errors.
    auto errs(collector->errs());

    std::size_t retries(0);
    while (!errs.empty() && retries++ < maxGetRetries)
    {
        std::cout << "Retry round " << retries << " - " << errs.size() <<
            " errors" << std::endl;

        for (const auto& err : errs)
        {
            const uint64_t id(err.first);
            const std::string file(err.second);

            m_s3.get(id, file, collector.get());
        }

        errs = collector->errs();
    }

    if (!errs.empty())
    {
        throw std::runtime_error("Error finalizing to S3");
    }

    for (auto& res : collector->responses())
    {
        const uint64_t id(res.first);
        const std::vector<uint8_t>* resData(res.second);

        std::uint64_t uncompressedSize(0);
        std::memcpy(&uncompressedSize, resData->data(), sizeof(uint64_t));

        const uint8_t* rawData(resData->data() + sizeof(uint64_t));
        const std::size_t rawNumBytes(resData->size() - sizeof(uint64_t));

        // Data from the table may be compressed.
        std::vector<uint8_t> tableData(rawNumBytes);
        std::memcpy(tableData.data(), rawData, rawNumBytes);
        delete resData->data();
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

