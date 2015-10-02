#pragma once

#include <condition_variable>
#include <mutex>
#include <thread>

#include <pdal/pdal_types.hpp>

#include "read-queries/base.hpp"

namespace pdal
{
    class PointView;
    class Reader;
    class StageFactory;
}

namespace entwine
{
    class SimplePointTable;
}

class SourceManager;

class UnindexedReadQuery : public ReadQuery
{
public:
    UnindexedReadQuery(
            const entwine::Schema& schema,
            bool compress,
            SourceManager& sourceManager);
    ~UnindexedReadQuery();

private:
    virtual bool readSome(ItcBuffer& buffer);
    virtual uint64_t numPoints() const;

    std::unique_ptr<pdal::Reader> m_reader;
    std::size_t m_numPoints;

    bool m_hasChunk;
    std::size_t m_producerIndex;
    std::size_t m_consumerIndex;

    std::unique_ptr<entwine::SimplePointTable> m_table;
    const entwine::Schema& m_schema;

    std::mutex m_mutex;
    std::condition_variable m_cv;
    bool m_first;
    std::thread m_executor;
};

