#include <algorithm>
#include <limits>

#include <pdal/PointBuffer.hpp>
#include <entwine/types/schema.hpp>

#include "util/schema.hpp"
#include "read-queries/live.hpp"

LiveReadQuery::LiveReadQuery(
        const entwine::Schema& schema,
        bool compress,
        bool rasterize,
        std::shared_ptr<pdal::PointBuffer> pointBuffer,
        std::vector<std::size_t> indexList)
    : ReadQuery(pointBuffer->dimTypes(), schema, compress, rasterize)
    , m_pointBuffer(pointBuffer)
    , m_indexList(indexList)
{ }

void LiveReadQuery::readPoint(
        uint8_t* pos,
        const entwine::Schema& schema,
        bool rasterize) const
{
    const std::size_t i(m_indexList[m_index]);
    if (i != std::numeric_limits<std::size_t>::max())
    {
        if (rasterize)
        {
            // Mark this point as a valid point.
            std::fill(pos, pos + 1, 1);
            ++pos;
        }

        for (const auto& dim : schema.dims())
        {
            if (Util::use(dim, rasterize))
            {
                pos += readDim(pos, m_pointBuffer.get(), dim, i);
            }
        }
    }
    else if (rasterize)
    {
        // Mark this point as a hole.  Don't clear the rest of the
        // point data so it will compress nicely if enabled.
        std::fill(pos, pos + 1, 0);
    }
}

const std::vector<std::size_t>& LiveReadQuery::indexList() const
{
    return m_indexList;
}

bool LiveReadQuery::eof() const
{
    return m_index == m_indexList.size();
}

std::size_t LiveReadQuery::numPoints() const
{
    return m_indexList.size();
}

