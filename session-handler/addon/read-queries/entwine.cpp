#include <entwine/tree/sleepy-tree.hpp>
#include <entwine/types/schema.hpp>

#include "util/schema.hpp"
#include "read-queries/entwine.hpp"

EntwineReadQuery::EntwineReadQuery(
        const entwine::Schema& schema,
        bool compress,
        bool rasterize,
        entwine::SleepyTree& sleepyTree,
        const std::vector<std::size_t>& ids)
    : ReadQuery(schema, compress, rasterize)
    , m_sleepyTree(sleepyTree)
    , m_ids(ids)
{ }

void EntwineReadQuery::readPoint(
        char* pos,
        const entwine::Schema& schema,
        bool rasterize) const
{
    std::vector<char> point(
            m_sleepyTree.getPointData(m_ids[index()], schema));

    if (point.empty())
    {
        throw std::runtime_error("Got empty point");
    }

    std::memcpy(pos, point.data(), point.size());
}

bool EntwineReadQuery::eof() const
{
    return index() == numPoints();
}

std::size_t EntwineReadQuery::numPoints() const
{
    return m_ids.size();
}

