#include <pdal/PointBuffer.hpp>
#include <entwine/types/schema.hpp>

#include "read-queries/unindexed.hpp"

UnindexedReadQuery::UnindexedReadQuery(
        const entwine::Schema& schema,
        bool compress,
        std::shared_ptr<pdal::PointBuffer> pointBuffer,
        std::size_t start,
        std::size_t count)
    : ReadQuery(
            pointBuffer->dimTypes(),
            schema,
            compress,
            false,
            std::min<std::size_t>(start, pointBuffer->size()))
    , m_pointBuffer(pointBuffer)
    , m_begin(std::min<std::size_t>(start, m_pointBuffer->size()))
    , m_end(
            count == 0 ?
                m_pointBuffer->size() :
                std::min<std::size_t>(start + count, m_pointBuffer->size()))
{ }

void UnindexedReadQuery::readPoint(
        uint8_t* pos,
        const entwine::Schema& schema,
        bool) const
{
    for (const auto& dim : schema.dims())
    {
        pos += readDim(pos, m_pointBuffer.get(), dim, m_index);
    }
}

std::size_t UnindexedReadQuery::numPoints() const
{
    return m_end - m_begin;
}

bool UnindexedReadQuery::eof() const
{
    return m_index == m_end;
}

