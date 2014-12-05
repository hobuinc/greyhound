#include <cmath>

#include <pdal/PointBuffer.hpp>

#include "grey-tree.hpp"

namespace
{
    const std::string greyVersion("1.0");

    const uint64_t nwFlag(0);
    const uint64_t neFlag(1);
    const uint64_t swFlag(2);
    const uint64_t seFlag(3);

    // Must be non-zero to distinguish between various levels of NW-only paths.
    const uint64_t baseId(3);

    const std::string sqlCreateTableMeta(
            "CREATE TABLE meta("
                "id             INT PRIMARY KEY,"
                "version        TEXT,"
                "pointContext   TEXT,"
                "xMin           REAL,"
                "yMin           REAL,"
                "xMax           REAL,"
                "yMax           REAL,"
                "numPoints      INT,"
                "schema         TEXT,"
                "stats          TEXT,"
                "srs            TEXT,"
                "fills          BLOB)");

    const std::string sqlPrepMeta(
            "INSERT INTO meta VALUES "
                "(@version, @pointContext,"
                " @xMin, @yMin, @xMax, @yMax, @numPoints, @schema,"
                " @stats, @srs, @fills, @id)");

    const std::string sqlCreateTableClusters(
            "CREATE TABLE clusters("
                "id         INT PRIMARY KEY,"
                "data       BLOB)");

    const std::string sqlPrepData(
            "INSERT INTO clusters VALUES (@id, @data)");

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

GreyTree::BBox::BBox(double xMin, double yMin, double xMax, double yMax)
    : xMin(xMin)
    , yMin(yMin)
    , xMax(xMax)
    , yMax(yMax)
    , xMid(xMin + (xMax - xMin) / 2.0)
    , yMid(yMin + (yMax - yMin) / 2.0)
{ }

GreyTree::BBox::BBox(pdal::BOX3D bbox)
    : xMin(bbox.minx)
    , yMin(bbox.miny)
    , xMax(bbox.maxx)
    , yMax(bbox.maxy)
    , xMid(xMin + (xMax - xMin) / 2.0)
    , yMid(yMin + (yMax - yMin) / 2.0)
{ }

GreyTree::GreyTree(const pdal::QuadIndex& quadIndex, const GreyMeta meta)
    : m_meta(meta)
    , m_quadIndex(quadIndex)
{ }

void GreyTree::write(std::string filename, std::size_t baseLevel) const
{
    // Get clustered data.
    std::map<uint64_t, std::vector<std::size_t>> clusters;
    clusters[baseId] = m_quadIndex.getPoints(baseLevel);
    const BBox bbox(m_meta.xMin, m_meta.yMin, m_meta.xMax, m_meta.yMax);
    build(clusters, bbox, baseLevel, baseId);

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
    dbWriteData(db, clusters);

    // Write metadata.
    sqlExec(db, sqlCreateTableMeta);
    dbWriteMeta(db);

    // Close DB handle.
    sqlite3_close_v2(db);
}

void GreyTree::dbWriteMeta(sqlite3* db) const
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

    if (sqlite3_bind_text(stmt, 1, greyVersion.c_str(), -1, SQLITE_STATIC) ||
        sqlite3_bind_text(stmt, 2, pcPtr, -1, SQLITE_STATIC) ||
        sqlite3_bind_double(stmt, 3, m_meta.xMin) ||
        sqlite3_bind_double(stmt, 4, m_meta.yMin) ||
        sqlite3_bind_double(stmt, 5, m_meta.xMax) ||
        sqlite3_bind_double(stmt, 6, m_meta.yMax) ||
        sqlite3_bind_int64 (stmt, 7, m_meta.numPoints) ||
        sqlite3_bind_text(stmt, 8, m_meta.schema.c_str(), -1, SQLITE_STATIC) ||
        sqlite3_bind_text(stmt, 9, m_meta.stats.c_str(), -1, SQLITE_STATIC) ||
        sqlite3_bind_text(stmt,10, m_meta.srs.c_str(), -1, SQLITE_STATIC) ||
        sqlite3_bind_blob(stmt,11, fills.data(), fills.size(), SQLITE_STATIC))
    {
        throw std::runtime_error("Error binding values");
    }

    sqlite3_step(stmt);
    sqlite3_clear_bindings(stmt);
    sqlite3_reset(stmt);

    // Done inserting.
    sqlExec(db, "END TRANSACTION");
    sqlite3_finalize(stmt);
}

void GreyTree::dbWriteData(
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
        const std::vector<std::size_t>& indices(entry.second);

        std::vector<uint8_t> data(entry.second.size() * pointSize);
        uint8_t* pos(data.data());

        // Read the entries at these indices into our buffer of real point data.
        for (const auto index : indices)
        {
            for (const auto& dimId : pointBuffer.dims())
            {
                pointBuffer.getRawField(dimId, index, pos);
                pos += pointBuffer.dimSize(dimId);
            }
        }

        if (sqlite3_bind_int(stmt, 1, clusterId) ||
            sqlite3_bind_blob(stmt, 2, data.data(), data.size(), SQLITE_STATIC))
        {
            throw std::runtime_error("Error binding values");
        }

        sqlite3_step(stmt);
        sqlite3_clear_bindings(stmt);
        sqlite3_reset(stmt);
    }

    // Done inserting.
    sqlExec(db, "END TRANSACTION");
    sqlite3_finalize(stmt);
}

void GreyTree::build(
        std::map<uint64_t, std::vector<std::size_t>>& results,
        const BBox& bbox,
        std::size_t level,
        uint64_t id) const
{
    const BBox nwBounds(bbox.xMin, bbox.yMid, bbox.xMid, bbox.yMax);
    const BBox neBounds(bbox.xMid, bbox.yMid, bbox.xMax, bbox.yMax);
    const BBox swBounds(bbox.xMin, bbox.yMin, bbox.xMid, bbox.yMid);
    const BBox seBounds(bbox.xMid, bbox.yMin, bbox.xMax, bbox.yMid);

    id <<= 2;
    const uint64_t nwId(id | nwFlag);
    const uint64_t neId(id | neFlag);
    const uint64_t swId(id | swFlag);
    const uint64_t seId(id | seFlag);

    results[nwId] = getPoints(nwBounds, level);
    results[neId] = getPoints(neBounds, level);
    results[swId] = getPoints(swBounds, level);
    results[seId] = getPoints(seBounds, level);

    if (++level <= m_meta.fills.size())
    {
        build(results, nwBounds, level, neId);
        build(results, neBounds, level, neId);
        build(results, swBounds, level, neId);
        build(results, seBounds, level, neId);
    }
}

std::vector<std::size_t> GreyTree::getPoints(
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

