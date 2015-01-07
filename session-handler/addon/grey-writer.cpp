#include <pdal/PointBuffer.hpp>
#include <pdal/QuadIndex.hpp>
#include <pdal/Compression.hpp>

#include "grey-writer.hpp"
#include "compression.hpp"

namespace
{
    const std::string sqlCreateTableMeta(
            "CREATE TABLE meta("
                "id             INT PRIMARY KEY,"
                "version        TEXT,"
                "base           INT,"
                "pointContext   TEXT,"
                "xMin           REAL,"
                "yMin           REAL,"
                "xMax           REAL,"
                "yMax           REAL,"
                "numPoints      INT,"
                "schema         TEXT,"
                "compressed     INT,"
                "stats          TEXT,"
                "srs            TEXT,"
                "fills          BLOB)");

    const std::string sqlPrepMeta(
            "INSERT INTO meta VALUES "
                "(@id, @version, @base, @pointContext,"
                " @xMin, @yMin, @xMax, @yMax, @numPoints, @schema,"
                " @compressed, @stats, @srs, @fills)");

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
}

GreyWriter::GreyWriter(
        const pdal::QuadIndex& quadIndex,
        const GreyMeta meta)
    : m_meta(meta)
    , m_quadIndex(quadIndex)
{ }

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

    const std::vector<std::size_t>& fills(m_meta.fills);
    const char* pcPtr(m_meta.pointContextXml.c_str());

    if (sqlite3_bind_int  (stmt, 1, 0) ||
        sqlite3_bind_text (stmt, 2, greyVersion.c_str(), -1, SQLITE_STATIC) ||
        sqlite3_bind_int64(stmt, 3, m_meta.base) ||
        sqlite3_bind_text (stmt, 4, pcPtr, -1, SQLITE_STATIC) ||
        sqlite3_bind_double(stmt, 5, m_meta.bbox.xMin) ||
        sqlite3_bind_double(stmt, 6, m_meta.bbox.yMin) ||
        sqlite3_bind_double(stmt, 7, m_meta.bbox.xMax) ||
        sqlite3_bind_double(stmt, 8, m_meta.bbox.yMax) ||
        sqlite3_bind_int64 (stmt, 9, m_meta.numPoints) ||
        sqlite3_bind_text(stmt, 10, m_meta.schema.c_str(), -1, SQLITE_STATIC) ||
        sqlite3_bind_int (stmt, 11, m_meta.compressed) ||
        sqlite3_bind_text(stmt, 12, m_meta.stats.c_str(), -1, SQLITE_STATIC) ||
        sqlite3_bind_text(stmt, 13, m_meta.srs.c_str(), -1, SQLITE_STATIC) ||
        sqlite3_bind_blob(stmt, 14,
            fills.data(),
            // Need number of bytes here.
            reinterpret_cast<const char*>(fills.data() + fills.size()) -
                reinterpret_cast<const char*>(fills.data()),
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

    const pdal::PointBuffer& pointBuffer(m_quadIndex.pointBuffer());
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

        GreyhoundStream greyhoundStream;

        if (m_meta.compressed)
        {
            // Perform compression for this cluster.
            pdal::LazPerfCompressor<GreyhoundStream> compressor(
                    greyhoundStream,
                    pointBuffer.dimTypes());

            compressor.compress(
                    reinterpret_cast<char*>(data.data()), data.size());
            compressor.done();

            data = greyhoundStream.data();
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
    return m_quadIndex.getPoints(
            bbox.xMin,
            bbox.yMin,
            bbox.xMax,
            bbox.yMax,
            level,
            level + 1);
}

