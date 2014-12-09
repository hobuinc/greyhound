#include <cmath>

#include <pdal/PointBuffer.hpp>
#include <pdal/Charbuf.hpp>
#include <pdal/XMLSchema.hpp>

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
}

namespace
{
    const std::string sqlCreateTableMeta(
            "CREATE TABLE meta("
                "id             INT PRIMARY KEY,"
                "base           INT,"
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
                "(@id, @version, @base, @pointContext,"
                " @xMin, @yMin, @xMax, @yMax, @numPoints, @schema,"
                " @stats, @srs, @fills)");

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

namespace
{
    const std::string sqlQueryMeta(
            "SELECT "
                "version, base, pointContext, xMin, yMin, xMax, yMax, "
                "numPoints, schema, stats, srs, fills "
            "FROM meta");
}

BBox::BBox()
    : xMin(0)
    , yMin(0)
    , xMax(0)
    , yMax(0)
{ }

BBox::BBox(const BBox& other)
    : xMin(other.xMin)
    , yMin(other.yMin)
    , xMax(other.xMax)
    , yMax(other.yMax)
{ }

BBox::BBox(double xMin, double yMin, double xMax, double yMax)
    : xMin(xMin)
    , yMin(yMin)
    , xMax(xMax)
    , yMax(yMax)
{ }

BBox::BBox(pdal::BOX3D bbox)
    : xMin(bbox.minx)
    , yMin(bbox.miny)
    , xMax(bbox.maxx)
    , yMax(bbox.maxy)
{ }


bool BBox::overlaps(const BBox& other) const
{
    return
        std::abs(xMid() - other.xMid()) < width()  / 2 + other.width()  / 2 &&
        std::abs(yMid() - other.yMid()) < height() / 2 + other.height() / 2;
}

bool BBox::contains(const BBox& other) const
{
    return
        xMin <= other.xMin && xMax >= other.xMax &&
        yMin <= other.yMin && yMax >= other.yMax;
}

GreyWriter::GreyWriter(const pdal::QuadIndex& quadIndex, const GreyMeta meta)
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
        sqlite3_bind_text(stmt, 11, m_meta.stats.c_str(), -1, SQLITE_STATIC) ||
        sqlite3_bind_text(stmt, 12, m_meta.srs.c_str(), -1, SQLITE_STATIC) ||
        sqlite3_bind_blob(stmt, 13,
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

GreyReader::GreyReader(const std::string pipelineId)
    : m_db(0)
    , m_meta()
    , m_idIndex()
    , m_pointContext()
{
    if (sqlite3_open_v2(
                (pipelineId + ".grey").c_str(),
                &m_db,
                SQLITE_OPEN_READONLY,
                0) != SQLITE_OK)
    {
        throw std::runtime_error("Could not open serial DB " + pipelineId);
    }

    readMeta();

    m_idIndex.reset(new IdIndex(m_meta));

    pdal::schema::Reader reader(m_meta.pointContextXml);
    pdal::MetadataNode metaNode = reader.getMetadata();

    m_pointContext.registerDim(pdal::Dimension::Id::X);
    m_pointContext.registerDim(pdal::Dimension::Id::Y);
    m_pointContext.registerDim(pdal::Dimension::Id::Z);

    const pdal::schema::DimInfoList dims(reader.schema().dimInfoList());

    for (auto dim(dims.begin()); dim != dims.end(); ++dim)
    {
        m_pointContext.registerOrAssignDim(dim->m_name, dim->m_type);
    }

}

GreyReader::~GreyReader()
{
    if (m_db)
    {
        sqlite3_close_v2(m_db);
    }
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
        m_meta.stats =
            reinterpret_cast<const char*>(sqlite3_column_text(stmt, 9));
        m_meta.srs =
            reinterpret_cast<const char*>(sqlite3_column_text(stmt, 10));

        const char* rawFills(
                reinterpret_cast<const char*>(
                    sqlite3_column_blob(stmt, 11)));
        const std::size_t rawBytes(sqlite3_column_bytes(stmt, 11));

        m_meta.fills.resize(
                reinterpret_cast<const std::size_t*>(rawFills + rawBytes) -
                reinterpret_cast<const std::size_t*>(rawFills));

        std::memcpy(m_meta.fills.data(), rawFills, rawBytes);
    }

    sqlite3_finalize(stmt);
}

void GreyReader::read(
        std::vector<uint8_t>& buffer,
        const Schema& schema,
        std::size_t depthBegin,
        std::size_t depthEnd)
{
    std::vector<uint64_t> completeResults;
    std::vector<uint64_t> partialResults;

    m_idIndex->find(
            completeResults,
            partialResults,
            depthBegin,
            depthEnd);

    std::ostringstream missingIds;
    m_cacheMutex.lock();
    try
    {
        for (auto i : completeResults)
        {
            if (!m_cache.count(i))
            {
                if (missingIds.str().size()) missingIds << ",";
                missingIds << i;
            }
        }

        for (auto i : partialResults)
        {
            if (!m_cache.count(i))
            {
                if (missingIds.str().size()) missingIds << ",";
                missingIds << i;
            }
        }
    }
    catch (...)
    {
        m_cacheMutex.unlock();
        throw std::runtime_error("Error occurred during serial read.");
    }

    m_cacheMutex.unlock();

    const std::string sqlDataQuery(
            "SELECT id, data FROM clusters WHERE id IN (" +
            missingIds.str() +
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

    std::map<uint64_t, std::shared_ptr<GreyCluster>> tempCache;

    while (sqlite3_step(stmtData) == SQLITE_ROW)
    {
        const int id(sqlite3_column_int64(stmtData, 0));

        const char* rawData(
                reinterpret_cast<const char*>(
                    sqlite3_column_blob(stmtData, 1)));
        const std::size_t rawBytes(sqlite3_column_bytes(stmtData, 1));

        std::vector<char> data(rawBytes);
        std::memcpy(data.data(), rawData, rawBytes);

        // Construct a pdal::PointBuffer from our binary data.
        pdal::Charbuf charbuf(data);
        std::istream stream(&charbuf);
        std::shared_ptr<pdal::PointBuffer> pointBuffer(
                new pdal::PointBuffer(
                    stream,
                    m_pointContext,
                    0,
                    data.size() / m_pointContext.pointSize()));

        std::shared_ptr<GreyCluster> cluster(
                new GreyCluster(pointBuffer));

        // Don't insert into m_cache, we'll do the pointBuffer building and the
        // quad-tree indexing (if necessary) in advance, so we will only have
        // to lock the real cache to insert this premade data.
        tempCache.insert(std::make_pair(id, cluster));
    }

    sqlite3_finalize(stmtData);

    for (auto i : completeResults)
    {
        auto cluster(tempCache.find(i));
        if (cluster != tempCache.end())
        {
            // TODO Perform quad-tree indexing.
            /*
            std::shared_ptr<pdal::QuadIndex> quadTree(
                    new pdal::QuadIndex(*pointBuffer.get(), depthBegin));

            quadTree->build(
                    sqlite3_column_double(stmtData, 3),
                    sqlite3_column_double(stmtData, 4),
                    sqlite3_column_double(stmtData, 5),
                    sqlite3_column_double(stmtData, 6));

            m_quadCache.insert(std::make_pair(id, quadTree));
            */
        }
    }

    m_cacheMutex.lock();
    try
    {
        for (auto it : tempCache)
        {
            m_cache.insert(it.first, it.second);
        }
    }
    catch (...)
    {
        m_cacheMutex.unlock();
        throw std::runtime_error("Error occurred during cache insertion.");
    }
    m_cacheMutex.unlock();

    for (auto id : completeResults)
    {
        // TODO Dumb-grab these whole clusters and append to buffer.
    }

    for (auto id : partialResults)
    {
        // TODO Perform quad-index query on these clusters and append to buffer.
    }
}

IdTree::IdTree(
        uint64_t id,
        std::size_t currentLevel,
        std::size_t endLevel)
    : id(id)
    , nw()
    , ne()
    , sw()
    , se()
{
    if (++currentLevel < endLevel)
    {
        id <<= 2;
        const uint64_t nwId(id | nwFlag);
        const uint64_t neId(id | neFlag);
        const uint64_t swId(id | swFlag);
        const uint64_t seId(id | seFlag);

        nw.reset(new IdTree(nwId, currentLevel, endLevel));
        ne.reset(new IdTree(neId, currentLevel, endLevel));
        sw.reset(new IdTree(swId, currentLevel, endLevel));
        se.reset(new IdTree(seId, currentLevel, endLevel));
    }
}

void IdTree::find(
        std::vector<uint64_t>& results,
        const std::size_t queryLevelBegin,
        const std::size_t queryLevelEnd,
        const std::size_t currentLevel) const
{
    if (currentLevel >= queryLevelBegin && currentLevel < queryLevelEnd)
    {
        results.push_back(id);
    }

    std::size_t nextLevel(currentLevel + 1);
    if (nextLevel < queryLevelEnd)
    {
        if (nw) nw->find(results, queryLevelBegin, queryLevelEnd, nextLevel);
        if (ne) ne->find(results, queryLevelBegin, queryLevelEnd, nextLevel);
        if (sw) sw->find(results, queryLevelBegin, queryLevelEnd, nextLevel);
        if (se) se->find(results, queryLevelBegin, queryLevelEnd, nextLevel);
    }
}

void IdTree::find(
        std::vector<uint64_t>& completeResults,
        std::vector<uint64_t>& partialResults,
        const std::size_t   queryLevelBegin,
        const std::size_t   queryLevelEnd,
        const BBox&         queryBBox,
        const std::size_t   currentLevel,
        const BBox          currentBBox) const
{
    if (queryBBox.overlaps(currentBBox))
    {
        if (currentLevel >= queryLevelBegin && currentLevel < queryLevelEnd)
        {
            if (queryBBox.contains(currentBBox))
            {
                completeResults.push_back(id);
            }
            else
            {
                partialResults.push_back(id);
            }
        }

        std::size_t nextLevel(currentLevel + 1);

        if (nextLevel < queryLevelEnd)
        {
            if (nw)
                nw->find(
                        completeResults,
                        partialResults,
                        queryLevelBegin,
                        queryLevelEnd,
                        queryBBox,
                        nextLevel,
                        currentBBox.getNw());

            if (ne)
                ne->find(
                        completeResults,
                        partialResults,
                        queryLevelBegin,
                        queryLevelEnd,
                        queryBBox,
                        nextLevel,
                        currentBBox.getNe());

            if (sw)
                sw->find(
                        completeResults,
                        partialResults,
                        queryLevelBegin,
                        queryLevelEnd,
                        queryBBox,
                        nextLevel,
                        currentBBox.getSw());

            if (se)
                se->find(
                        completeResults,
                        partialResults,
                        queryLevelBegin,
                        queryLevelEnd,
                        queryBBox,
                        nextLevel,
                        currentBBox.getSe());
        }
    }
}

std::size_t IdTree::info(
        const uint64_t id,
        std::size_t offset,
        BBox&       resultBBox,
        std::size_t currentLevel,
        BBox        currentBBox)
{
    // TODO
}

IdIndex::IdIndex(const GreyMeta& meta)
    : m_base(meta.base)
    , m_idTree(baseId, meta.base, meta.fills.size() + 1)
    , m_bbox(meta.bbox)
{ }

void IdIndex::find(
        std::vector<uint64_t>& completeResults,
        std::vector<uint64_t>& partialResults,
        std::size_t depthBegin,
        std::size_t depthEnd) const
{
    if (depthBegin >= depthEnd) return;

    if (depthBegin < m_base)
    {
        partialResults.push_back(baseId);
    }

    if (depthEnd > m_base)
    {
        m_idTree.find(completeResults, depthBegin, depthEnd, m_base);
    }
}

void IdIndex::find(
        std::vector<uint64_t>& completeResults,
        std::vector<uint64_t>& partialResults,
        std::size_t depthBegin,
        std::size_t depthEnd,
        BBox queryBBox) const
{
    if (depthBegin >= depthEnd) return;

    if (depthBegin < m_base)
    {
        partialResults.push_back(baseId);
    }

    if (depthEnd > m_base)
    {
        m_idTree.find(
                completeResults,
                partialResults,
                depthBegin,
                depthEnd,
                queryBBox,
                m_base,
                m_bbox);
    }
}

GreyCluster::GreyCluster(std::shared_ptr<pdal::PointBuffer> pointBuffer)
    : m_pointBuffer(pointBuffer)
    , m_quadTree(0)
{ }

