#include <algorithm>
#include <limits>

#include "types/schema.hpp"
#include "serial.hpp"

SerialReadQuery::SerialReadQuery(
        const Schema& schema,
        bool compress,
        bool rasterize,
        GreyQuery greyQuery)
    : ReadQuery(greyQuery.dimTypes(), schema, compress, rasterize)
    , m_greyQuery(greyQuery)
{ }

void SerialReadQuery::readPoint(
        uint8_t* pos,
        const Schema& schema,
        bool rasterize) const
{
    QueryIndex queryIndex(m_greyQuery.queryIndex(m_index));

    if (queryIndex.index != std::numeric_limits<std::size_t>::max())
    {
        if (rasterize)
        {
            // Mark this point as a valid point.
            std::fill(pos, pos + 1, 1);
            ++pos;
        }

        for (const auto& dim : schema.dims)
        {
            if (schema.use(dim, rasterize))
            {
                pos += readDim(
                        pos,
                        m_greyQuery.pointBuffer(queryIndex.id),
                        dim,
                        queryIndex.index);
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

std::size_t SerialReadQuery::numPoints() const
{
    return m_greyQuery.numPoints();
}

bool SerialReadQuery::eof() const
{
    return m_index == numPoints();
}

bool SerialReadQuery::serial() const
{
    return true;
}

