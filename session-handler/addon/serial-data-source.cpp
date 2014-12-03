#include <pdal/Charbuf.hpp>
#include <pdal/XMLSchema.hpp>

#include "serial-data-source.hpp"
#include "read-command.hpp"

namespace
{
    bool rasterOmit(pdal::Dimension::Id::Enum id)
    {
        // These Dimensions are not explicitly placed in the output buffer
        // for rasterized requests.
        return id == pdal::Dimension::Id::X || id == pdal::Dimension::Id::Y;
    }
}

SerialDataSource::SerialDataSource(std::string pipelineId)
    : m_pipelineId(pipelineId)
    , m_numPoints(0)
    , m_schema()
    , m_stats()
    , m_srs()
    , m_fills()
    , m_pointContext()
    , m_pointBuffer()
    , m_db(0)
    , m_quadCache()
    , m_pbSet()
{
    // Read metadata file with dimension info to make the PointContext.
    std::ifstream metaFile(m_pipelineId + ".grey");
    std::string meta;

    if (metaFile)
    {
        metaFile.seekg(0, std::ios::end);
        meta.reserve(metaFile.tellg());
        metaFile.seekg(0, std::ios::beg);

        meta.assign(
                std::istreambuf_iterator<char>(metaFile),
                std::istreambuf_iterator<char>());
        metaFile.close();
    }
    else
    {
        throw std::runtime_error("No meta file found for " + pipelineId);
    }

    pdal::schema::Reader reader(meta);
    pdal::MetadataNode metaNode = reader.getMetadata();

    // TODO Handle missing entries.
    m_numPoints =   metaNode.findChild("numPoints").value<std::size_t>();
    m_schema =      metaNode.findChild("schema").value();
    m_stats =       metaNode.findChild("stats").value();
    m_srs =         metaNode.findChild("srs").value();

    // Decode fills.
    std::vector<uint8_t> rawFills(
            pdal::Utils::base64_decode(metaNode.findChild("fills").value()));
    m_fills.resize(
            reinterpret_cast<std::size_t*>(rawFills.data() + rawFills.size()) -
            reinterpret_cast<std::size_t*>(rawFills.data()));
    std::memcpy(m_fills.data(), rawFills.data(), rawFills.size());

    m_pointContext.registerDim(pdal::Dimension::Id::X);
    m_pointContext.registerDim(pdal::Dimension::Id::Y);
    m_pointContext.registerDim(pdal::Dimension::Id::Z);

    const pdal::schema::DimInfoList dims(reader.schema().dimInfoList());

    for (auto dim(dims.begin()); dim != dims.end(); ++dim)
    {
        m_pointContext.registerOrAssignDim(dim->m_name, dim->m_type);
    }

    if (sqlite3_open_v2(
                (pipelineId + ".cls").c_str(),
                &m_db,
                SQLITE_OPEN_READONLY,
                0) != SQLITE_OK)
    {
        throw std::runtime_error("Could not open serial database");
    }
}

SerialDataSource::~SerialDataSource()
{
    sqlite3_close_v2(m_db);
}

std::size_t SerialDataSource::getNumPoints() const
{
    return m_numPoints;
}

std::string SerialDataSource::getSchema() const
{
    return m_schema;
}

std::string SerialDataSource::getStats() const
{
    return m_stats;
}

std::string SerialDataSource::getSrs() const
{
    return m_srs;
}

std::vector<std::size_t> SerialDataSource::getFills() const
{
    return m_fills;
}

bool SerialDataSource::exists(const std::string pipelineId)
{
    bool exists(false);
    std::ifstream stream(pipelineId + ".cls");
    if (stream) exists = true;
    stream.close();
    return exists;
}

std::size_t SerialDataSource::read(
        std::vector<unsigned char>& buffer,
        const Schema& schema,
        const double xMin,
        const double yMin,
        const double xMax,
        const double yMax,
        const std::size_t depthBegin,
        const std::size_t depthEnd)
{
    std::ostringstream sqlQuery;
    sqlQuery
        << "SELECT id FROM clusters WHERE id IN "
        << "("
            << "SELECT id FROM clusters WHERE depthBegin<=" << depthBegin
                << " AND depthEnd>" << depthBegin
                << " UNION "
            << "SELECT id FROM clusters WHERE depthBegin<" << depthEnd
                << " AND depthEnd>=" << depthEnd
        << ")"
        << " INTERSECT "
        << "SELECT id FROM clusters WHERE id IN "
        << "("
            << "SELECT id FROM clusters WHERE xMax>" << xMin
                << " INTERSECT "
            << "SELECT id FROM clusters WHERE xMin<" << xMax
                << " INTERSECT "
            << "SELECT id FROM clusters WHERE yMax>" << yMin
                << " INTERSECT "
            << "SELECT id FROM clusters WHERE yMin<" << yMax
        << ")";

    const std::vector<int> rows(getRows(sqlQuery.str()));
    std::size_t offset(0);
    std::size_t points(0);

    for (auto id : rows)
    {
        const std::vector<std::size_t> indexList(
                m_quadCache[id]->getPoints(
                    xMin,
                    yMin,
                    xMax,
                    yMax,
                    depthBegin,
                    depthEnd));

        points += indexList.size();

        offset += appendIndexList(
                buffer,
                offset,
                m_quadCache[id]->pointBuffer(),
                schema,
                indexList,
                false);
    }

    return points;
}

std::size_t SerialDataSource::read(
            std::vector<unsigned char>& buffer,
            const Schema& schema,
            const std::size_t depthBegin,
            const std::size_t depthEnd)
{
    std::ostringstream sqlQuery;
    sqlQuery
        << "SELECT id FROM clusters WHERE id IN "
        << "(SELECT id FROM clusters WHERE depthBegin<=" << depthBegin
            << " AND depthEnd>" << depthBegin
        << " UNION"
        << " SELECT id FROM clusters WHERE depthBegin<" << depthEnd
            << " AND depthEnd>=" << depthEnd << ")";

    const std::vector<int> rows(getRows(sqlQuery.str()));
    std::size_t offset(0);
    std::size_t points(0);

    for (auto id : rows)
    {
        const std::vector<std::size_t> indexList(
                m_quadCache[id]->getPoints(
                    depthBegin,
                    depthEnd));

        points += indexList.size();

        offset += appendIndexList(
                buffer,
                offset,
                m_quadCache[id]->pointBuffer(),
                schema,
                indexList,
                false);
    }

    return points;
}

std::size_t SerialDataSource::read(
            std::vector<unsigned char>& buffer,
            const Schema& schema,
            const std::size_t rasterize,
            RasterMeta& rasterMeta)
{
    std::ostringstream sqlQuery;
    sqlQuery
        << "SELECT id FROM clusters WHERE id IN "
        << "("
            << "SELECT id FROM clusters WHERE depthBegin<=" << rasterize
                << " INTERSECT "
            << "SELECT id FROM clusters WHERE depthEnd>" << rasterize
        << ")"
        << " UNION "
        << "SELECT id FROM clusters WHERE id=0";

    const std::vector<int> rows(getRows(sqlQuery.str()));

    if (!m_quadCache[0]->getRasterMeta(
            rasterize,
            rasterMeta.xBegin,
            rasterMeta.xEnd,
            rasterMeta.xStep,
            rasterMeta.yBegin,
            rasterMeta.yEnd,
            rasterMeta.yStep))
    {
        return 0;
    }

    const std::size_t exp(std::pow(2, rasterize));
    std::size_t points(exp * exp);
    std::size_t stride(schema.stride() + 1);

    for (auto dim : schema.dims)
    {
        if (rasterOmit(dim.id))
        {
            stride -= dim.size;
        }
    }

    buffer.resize(points * stride, 0);

    for (auto id : rows)
    {
        std::map<std::size_t, std::size_t> indexMap(
                m_quadCache[id]->getPointsMap(
                    rasterize,
                    rasterMeta.xBegin,
                    rasterMeta.xEnd,
                    rasterMeta.xStep,
                    rasterMeta.yBegin,
                    rasterMeta.yEnd,
                    rasterMeta.yStep));

        insertIndexMap(
                buffer,
                m_quadCache[id]->pointBuffer(),
                schema,
                indexMap);
    }

    return points;
}

std::vector<int> SerialDataSource::getRows(const std::string sql)
{
    std::vector<int> results;

    sqlite3_stmt* stmtRows(0);
    sqlite3_prepare_v2(m_db, sql.c_str(), sql.size() + 1, &stmtRows, 0);

    if (!stmtRows)
    {
        throw std::runtime_error("Could not prepare SELECT stmt - rows");
    }

    int id;
    std::ostringstream missingRows;

    while (sqlite3_step(stmtRows) == SQLITE_ROW)
    {
        id = sqlite3_column_int(stmtRows, 0);

        if (!m_quadCache.count(id))
        {
            if (missingRows.str().size())
            {
                missingRows << ",";
            }

            missingRows << id;
        }

        results.push_back(id);
    }

    // Fetch the data for the rows that aren't in the cache.
    std::ostringstream sqlData;
    sqlData
        << "SELECT id, depthBegin, data, xMin, yMin, xMax, yMax "
        << "FROM clusters WHERE id IN ("
        << missingRows.str() << ")";

    sqlite3_stmt* stmtData(0);
    sqlite3_prepare_v2(
            m_db,
            sqlData.str().c_str(),
            sqlData.str().size() + 1,
            &stmtData,
            0);

    if (!stmtData)
    {
        throw std::runtime_error("Could not prepare SELECT stmt - data");
    }

    while (sqlite3_step(stmtData) == SQLITE_ROW)
    {
        const int id(sqlite3_column_int64(stmtData, 0));
        const std::size_t depthBegin(sqlite3_column_int64(stmtData, 1));

        const char* rawData(
                reinterpret_cast<const char*>(
                    sqlite3_column_blob(stmtData, 2)));
        const std::size_t rawBytes(sqlite3_column_bytes(stmtData, 2));

        std::vector<char> data(rawBytes);
        std::memcpy(data.data(), rawData, rawBytes);

        // Construct a pdal::PointBuffer from our binary data.
        pdal::Charbuf charbuf(data);
        std::istream stream(&charbuf);
        pdal::PointBufferPtr pointBuffer(
                new pdal::PointBuffer(
                    stream,
                    m_pointContext,
                    0,
                    data.size() / m_pointContext.pointSize()));

        // Perform indexing on the cluster.
        m_pbSet.insert(pointBuffer);

        std::shared_ptr<pdal::QuadIndex> quadTree(
                new pdal::QuadIndex(*pointBuffer.get(), depthBegin));

        quadTree->build(
                sqlite3_column_double(stmtData, 3),
                sqlite3_column_double(stmtData, 4),
                sqlite3_column_double(stmtData, 5),
                sqlite3_column_double(stmtData, 6));

        m_quadCache.insert(std::make_pair(id, quadTree));
    }

    return results;
}

std::size_t SerialDataSource::appendIndexList(
        std::vector<unsigned char>& buffer,
        const std::size_t pointOffset,
        const pdal::PointBuffer& pointBuffer,
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
        buffer.resize(buffer.size() + pointsToRead * stride, 0);

        unsigned char* pos(buffer.data() + pointOffset * stride);

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
                        pos += readDim(pos, dim, pointBuffer, i);
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

// This function is only for creating rasterized buffers.
std::size_t SerialDataSource::insertIndexMap(
        std::vector<unsigned char>& buffer,
        const pdal::PointBuffer& pointBuffer,
        const Schema& schema,
        const std::map<std::size_t, std::size_t>& indexList)
{
    const std::size_t pointsToRead(indexList.size());

    // Clientward rasterization schemas always contain a byte to specify
    // whether a point at this location in the raster exists.
    std::size_t stride(schema.stride() + 1);

    for (auto dim : schema.dims)
    {
        if (rasterOmit(dim.id))
        {
            stride -= dim.size;
        }
    }

    try
    {
        for (auto entry : indexList)
        {
            // TODO Is this check needed?
            if (entry.second != std::numeric_limits<std::size_t>::max())
            {
                unsigned char* pos(buffer.data() + entry.first * stride);

                // Mark this point as a valid point.
                std::fill(pos, pos + 1, 1);
                ++pos;

                for (const auto& dim : schema.dims)
                {
                    if (!rasterOmit(dim.id))
                    {
                        pos += readDim(pos, dim, pointBuffer, entry.second);
                    }
                }
            }
        }
    }
    catch (...)
    {
        throw std::runtime_error("Failed to read points from PDAL");
    }

    return pointsToRead;
}

// TODO Merge with DataSource nearly duplicate code.
std::size_t SerialDataSource::readDim(
        unsigned char* buffer,
        const DimInfo& dim,
        const pdal::PointBuffer& pointBuffer,
        const std::size_t index) const
{
    if (dim.type == "floating")
    {
        if (dim.size == 4)
        {
            float val(pointBuffer.getFieldAs<float>(dim.id, index));
            std::memcpy(buffer, &val, dim.size);
        }
        else if (dim.size == 8)
        {
            double val(pointBuffer.getFieldAs<double>(dim.id, index));
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
            uint8_t val(pointBuffer.getFieldAs<uint8_t>(dim.id, index));
            std::memcpy(buffer, &val, dim.size);
        }
        else if (dim.size == 2)
        {
            uint16_t val(pointBuffer.getFieldAs<uint16_t>(dim.id, index));
            std::memcpy(buffer, &val, dim.size);
        }
        else if (dim.size == 4)
        {
            uint32_t val(pointBuffer.getFieldAs<uint32_t>(dim.id, index));
            std::memcpy(buffer, &val, dim.size);
        }
        else if (dim.size == 8)
        {
            uint64_t val(pointBuffer.getFieldAs<uint64_t>(dim.id, index));
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
