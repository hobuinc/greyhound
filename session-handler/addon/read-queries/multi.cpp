#include <pdal/PointContext.hpp>

#include "read-queries/multi.hpp"
#include "types/schema.hpp"

MultiReadQuery::MultiReadQuery(
        const Schema& schema,
        bool compress,
        bool rasterize,
        std::shared_ptr<SleepyTree> sleepyTree,
        MultiResults multiResults)
    : ReadQuery(
            sleepyTree->pointContext().dimTypes(),
            schema,
            compress,
            rasterize)
    , m_sleepyTree(sleepyTree)
    , m_multiResults(multiResults)
    , m_offset(0)
{ }

void MultiReadQuery::readPoint(
        uint8_t* pos,
        const Schema& schema,
        bool rasterize) const
{
    const std::shared_ptr<pdal::PointBuffer> pointBuffer(
            m_sleepyTree->pointBuffer(m_multiResults[m_index].first));

    // TODO
    if (rasterize) throw std::runtime_error("No raster for multi yet.");

    for (const auto& dim : schema.dims)
    {
        if (schema.use(dim, rasterize))
        {
            pos += readDim(
                    pos,
                    pointBuffer,
                    dim,
                    m_multiResults[m_index].second);
        }
    }
}

bool MultiReadQuery::eof() const
{
    return m_index == numPoints();
}

std::size_t MultiReadQuery::numPoints() const
{
    return m_multiResults.size();
}

