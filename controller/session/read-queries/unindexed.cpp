#include "read-queries/unindexed.hpp"

#include <pdal/Reader.hpp>
#include <pdal/StageFactory.hpp>

#include <entwine/types/linking-point-view.hpp>
#include <entwine/types/schema.hpp>
#include <entwine/types/simple-point-table.hpp>
#include <entwine/types/single-point-table.hpp>

#include "types/source-manager.hpp"

namespace
{
    const std::size_t chunkSize(65536);
    pdal::StageFactory stageFactory;
}

UnindexedReadQuery::UnindexedReadQuery(
        const entwine::Schema& schema,
        bool compress,
        SourceManager& sourceManager)
    : ReadQuery(schema, compress, false)
    , m_reader(sourceManager.createReader())
    , m_numPoints(sourceManager.numPoints())
    , m_hasChunk(false)
    , m_producerIndex(0)
    , m_consumerIndex(0)
    , m_table(new entwine::SimplePointTable(schema))
    , m_schema(schema)
    , m_mutex()
    , m_executor()
{
    m_reader->setReadCb([this](pdal::PointView& view, pdal::PointId id)
    {
        addPoint(view, id);
    });

    m_reader->prepare(*m_table);

    m_executor.reset(new std::thread([this]()
    {
        m_reader->execute(*m_table);
    }));

    m_executor->detach();
}

UnindexedReadQuery::~UnindexedReadQuery()
{ }

void UnindexedReadQuery::readPoint(
        char* pos,
        const entwine::Schema&,
        bool rasterize)
{
    if (!m_hasChunk)
    {
        // Wait for data to be produced.
        std::unique_lock<std::mutex> lock(m_mutex);
        m_cv.wait(lock, [this]()->bool { return m_hasChunk; });
    }

    const std::size_t pointSize(m_schema.pointSize());
    std::memcpy(
            pos,
            m_table->data().data() + m_consumerIndex * pointSize,
            pointSize);

    // The point table might contain more than one point for dimension-oriented
    // readers.  Don't clear and notify the producer until all are consumed.
    if (++m_consumerIndex == m_table->size())
    {
        m_table->clear();
        m_hasChunk = false;
        m_consumerIndex = 0;

        m_cv.notify_one();
    }
}

void UnindexedReadQuery::addPoint(pdal::PointView& view, pdal::PointId id)
{
    if (id % 1000000 == 0)
        std::cout << id << "/" << m_numPoints << std::endl;

    // Wait until all in-progress points are written to the table.
    if (
            id + 1 == m_numPoints ||
            (id + 1 - m_producerIndex == m_table->size() &&
                    m_table->data().size() >= chunkSize))
    {
        m_hasChunk = true;
        m_producerIndex += m_table->size();

        // Notify the consumer that there is data ready.
        m_cv.notify_one();

        // Block production by the executor thread until all current data is
        // consumed.
        std::unique_lock<std::mutex> lock(m_mutex);
        m_cv.wait(lock, [this]()->bool { return !m_hasChunk; });
    }
}

bool UnindexedReadQuery::eof() const
{
    return index() == m_numPoints;
}

std::size_t UnindexedReadQuery::numPoints() const
{
    return m_numPoints;
}

