#include <pdal/PointBuffer.hpp>
#include <pdal/QuadIndex.hpp>
#include <pdal/Compression.hpp>

#include <json/json.h>

#include "compression-stream.hpp"
#include "http/collector.hpp"
#include "writer.hpp"

namespace
{
    const std::string sqlCreateTableMeta(
            "CREATE TABLE meta(id INT PRIMARY KEY, json TEXT)");

    const std::string sqlPrepMeta(
            "INSERT INTO meta VALUES (@id, @json)");

    const std::string sqlCreateTableClusters(
            "CREATE TABLE clusters("
                "id         INT PRIMARY KEY,"
                "fullSize   INT,"
                "data       BLOB)");

    const std::string sqlPrepData(
            "INSERT INTO clusters VALUES (@id, @fullSize, @data)");

    void sqlExec(sqlite3* db, std::string sql)
    {
        char* errMsgPtr;
        if (sqlite3_exec(db, sql.c_str(), 0, 0, &errMsgPtr) != SQLITE_OK)
        {
            std::string errMsg(errMsgPtr);
            sqlite3_free(errMsgPtr);
            throw std::runtime_error("Could not execute SQL: " + errMsg);
        }
    }

    const std::size_t greyBase(8);
    std::size_t calcCutoff(
            const std::vector<std::size_t>& fills,
            const std::size_t base)
    {
        std::size_t cutoff(0);
        std::size_t sum(0);
        std::size_t curr(0);

        for (std::size_t i(1); i < fills.size(); ++i)
        {
            sum += fills[i - 1];
            curr = fills[i];
            if (curr < sum)
            {
                cutoff = curr;
                break;
            }
        }

        if (cutoff && cutoff < base)
        {
            if (base < fills.size())
            {
                cutoff = 0;
            }
            else
            {
                cutoff = base;
            }
        }

        return cutoff;
    }

    const std::size_t maxPutRetries(4);
}

GreyWriter::GreyWriter(
        const pdal::PointBuffer& pointBuffer,
        const pdal::QuadIndex& quadIndex,
        const GreyMeta meta)
    : m_meta(meta)
    , m_pointBuffer(pointBuffer)
    , m_quadIndex(quadIndex)
{
    m_meta.base = greyBase;
    m_meta.cutoff = calcCutoff(m_meta.fills, greyBase);
}

void GreyWriter::write(S3Info s3Info, std::string dir) const
{
    // Write metadata.
    Json::StyledWriter jsonWriter;

    S3 s3(
            s3Info.awsAccessKeyId,
            s3Info.awsSecretAccessKey,
            s3Info.baseAwsUrl,
            s3Info.bucketName);

    if (s3.put(dir + "/meta", jsonWriter.write(m_meta.toJson())).code() != 200)
    {
        throw std::runtime_error("Error writing meta data to S3 - aborting");
    }

    // Get clustered data.
    std::map<uint64_t, std::vector<std::size_t>> clusters;
    clusters[baseId] = m_quadIndex.getPoints(m_meta.base);
    build(clusters, m_meta.bbox, m_meta.base, baseId);

    const std::size_t pointSize(m_pointBuffer.pointSize());

    std::unique_ptr<PutCollector> collector(new PutCollector(clusters.size()));
    std::size_t i(0);

    // Accumulate inserts.
    for (const auto& entry : clusters)
    {
        const uint64_t clusterId(entry.first);
        const std::vector<std::size_t>& indexList(entry.second);
        const uint64_t uncompressedSize(indexList.size() * pointSize);

        // For now use the first 8 bytes as an unsigned integer representing
        // the uncompressed length.
        std::shared_ptr<std::vector<uint8_t>> data(
                new std::vector<uint8_t>(sizeof(uint64_t) + uncompressedSize));
        std::memcpy(data->data(), &uncompressedSize, sizeof(uint64_t));
        uint8_t* pos(data->data() + sizeof(uint64_t));

        // Read the entries at these indices into our buffer of point data.
        for (const auto index : indexList)
        {
            m_pointBuffer.context().rawPtBuf()->getPoint(index, pos);
            pos += pointSize;
        }

        CompressionStream compressionStream;

        if (m_meta.compressed)
        {
            // Perform compression for this cluster.
            pdal::LazPerfCompressor<CompressionStream> compressor(
                    compressionStream,
                    m_pointBuffer.dimTypes());

            compressor.compress(
                    reinterpret_cast<char*>(data->data() + sizeof(uint64_t)),
                    uncompressedSize);
            compressor.done();

            // Offset by 8 bytes to maintain the uncompressed size field.
            data->resize(sizeof(uint64_t) + compressionStream.data().size());
            std::memcpy(
                    data->data() + sizeof(uint64_t),
                    compressionStream.data().data(),
                    compressionStream.data().size());

            // Our buffer may now be significantly smaller.  Free extra heap
            // space.
            data->shrink_to_fit();
        }

        const std::string file(dir + "/" + std::to_string(clusterId));
        s3.put(clusterId, file, data, collector.get());

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
    while (!errs.empty() && retries++ < maxPutRetries)
    {
        std::cout << "Retry round " << retries << " - " << errs.size() <<
            " errors" << std::endl;

        for (const auto& err : errs)
        {
            const uint64_t id(err.first);
            const std::shared_ptr<std::vector<uint8_t>> data(err.second);

            s3.put(id, dir + "/" + std::to_string(id), data, collector.get());
        }

        errs = collector->errs();
    }

    if (errs.empty())
    {
        // Indicate that this serialization can now be used.
        if (s3.put(dir + "/0", jsonWriter.write("1")).code() != 200)
        {
            throw std::runtime_error("Error finalizing to S3");
        }
    }
}

void GreyWriter::write(std::string filename) const
{
    // Get clustered data.
    std::map<uint64_t, std::vector<std::size_t>> clusters;
    clusters[baseId] = m_quadIndex.getPoints(m_meta.base);
    build(clusters, m_meta.bbox, m_meta.base, baseId);

    // Open database.
    sqlite3* db;
    if (sqlite3_open_v2(
                filename.c_str(),
                &db,
                SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
                0) != SQLITE_OK)
    {
        throw std::runtime_error("Could not open serial database");
    }

    // Write clusters.
    sqlExec(db, sqlCreateTableClusters);
    writeData(db, clusters);

    // Write metadata.
    sqlExec(db, sqlCreateTableMeta);
    writeMeta(db);

    // Close DB handle.
    sqlite3_close_v2(db);
}

void GreyWriter::writeMeta(sqlite3* db) const
{
    sqlite3_stmt* stmt(0);
    sqlite3_prepare_v2(
            db,
            sqlPrepMeta.c_str(),
            sqlPrepMeta.size() + 1,
            &stmt,
            0);

    if (!stmt)
    {
        throw std::runtime_error("Could not prepare meta INSERT stmt");
    }

    // Begin accumulating 'insert' transaction.
    sqlExec(db, "BEGIN TRANSACTION");

    Json::StyledWriter jsonWriter;

    if (sqlite3_bind_int(stmt, 1, 0) ||
        sqlite3_bind_text(
            stmt,
            2,
            jsonWriter.write(m_meta.toJson()).c_str(),
            -1,
            SQLITE_STATIC))
    {
        throw std::runtime_error("Error binding meta values");
    }

    sqlite3_step(stmt);
    sqlite3_clear_bindings(stmt);
    sqlite3_reset(stmt);

    // Done inserting.
    sqlExec(db, "END TRANSACTION");
    sqlite3_finalize(stmt);
}

void GreyWriter::writeData(
        sqlite3* db,
        const std::map<uint64_t, std::vector<std::size_t>>& clusters) const
{
    // Prepare statement for insert.
    sqlite3_stmt* stmt(0);
    sqlite3_prepare_v2(
            db,
            sqlPrepData.c_str(),
            sqlPrepData.size() + 1,
            &stmt,
            0);

    if (!stmt)
    {
        throw std::runtime_error("Could not prepare data INSERT stmt");
    }

    // Begin accumulating 'insert' transaction.
    sqlExec(db, "BEGIN TRANSACTION");

    const pdal::PointBuffer& pointBuffer(m_pointBuffer);
    const std::size_t pointSize(pointBuffer.pointSize());

    // Accumulate inserts.
    for (auto entry : clusters)
    {
        const uint64_t clusterId(entry.first);
        const std::vector<std::size_t>& indexList(entry.second);
        const std::size_t uncompressedSize(indexList.size() * pointSize);

        std::vector<uint8_t> data(uncompressedSize);
        uint8_t* pos(data.data());

        // Read the entries at these indices into our buffer of real point
        // data.
        for (const auto index : indexList)
        {
            for (const auto& dimId : pointBuffer.dims())
            {
                pointBuffer.getRawField(dimId, index, pos);
                pos += pointBuffer.dimSize(dimId);
            }
        }

        CompressionStream compressionStream;

        if (m_meta.compressed)
        {
            // Perform compression for this cluster.
            pdal::LazPerfCompressor<CompressionStream> compressor(
                    compressionStream,
                    pointBuffer.dimTypes());

            compressor.compress(
                    reinterpret_cast<char*>(data.data()), data.size());
            compressor.done();

            data = compressionStream.data();
        }

        if (sqlite3_bind_int  (stmt, 1, clusterId) ||
            sqlite3_bind_int64(stmt, 2, uncompressedSize) ||
            sqlite3_bind_blob(
                stmt,
                3,
                data.data(),
                data.size(),
                SQLITE_STATIC))
        {
            throw std::runtime_error("Error binding data values");
        }

        sqlite3_step(stmt);
        sqlite3_clear_bindings(stmt);
        sqlite3_reset(stmt);
    }

    // Done inserting.
    sqlExec(db, "END TRANSACTION");
    sqlite3_finalize(stmt);
}

void GreyWriter::build(
        std::map<uint64_t, std::vector<std::size_t>>& results,
        const BBox& bbox,
        std::size_t level,
        uint64_t id) const
{
    const BBox nwBounds(bbox.getNw());
    const BBox neBounds(bbox.getNe());
    const BBox swBounds(bbox.getSw());
    const BBox seBounds(bbox.getSe());

    id <<= 2;
    const uint64_t nwId(id | nwFlag);
    const uint64_t neId(id | neFlag);
    const uint64_t swId(id | swFlag);
    const uint64_t seId(id | seFlag);

    const std::vector<std::size_t> nwPoints(getPoints(nwBounds, level));
    const std::vector<std::size_t> nePoints(getPoints(neBounds, level));
    const std::vector<std::size_t> swPoints(getPoints(swBounds, level));
    const std::vector<std::size_t> sePoints(getPoints(seBounds, level));

    if (nwPoints.size()) results[nwId] = nwPoints;
    if (nePoints.size()) results[neId] = nePoints;
    if (swPoints.size()) results[swId] = swPoints;
    if (sePoints.size()) results[seId] = sePoints;

    if (++level <= m_meta.fills.size())
    {
        build(results, nwBounds, level, nwId);
        build(results, neBounds, level, neId);
        build(results, swBounds, level, swId);
        build(results, seBounds, level, seId);
    }
}

std::vector<std::size_t> GreyWriter::getPoints(
        const BBox& bbox,
        std::size_t level) const
{
    const Point min(bbox.min());
    const Point max(bbox.max());

    return m_quadIndex.getPoints(
            min.x,
            min.y,
            max.x,
            max.y,
            level,
            level + 1);
}

