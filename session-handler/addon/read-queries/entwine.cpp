#include <entwine/tree/branches/clipper.hpp>
#include <entwine/tree/sleepy-tree.hpp>
#include <entwine/types/schema.hpp>

#include "util/schema.hpp"
#include "read-queries/entwine.hpp"

EntwineReadQuery::EntwineReadQuery(
        const entwine::Schema& schema,
        bool compress,
        bool rasterize,
        entwine::SleepyTree& sleepyTree,
        std::unique_ptr<entwine::Clipper> clipper,
        const std::vector<std::size_t>& ids)
    : ReadQuery(schema, compress, rasterize)
    , m_sleepyTree(sleepyTree)
    , m_clipper(std::move(clipper))
    , m_ids(ids)
{ }

EntwineReadQuery::~EntwineReadQuery()
{ }

void EntwineReadQuery::readPoint(
        char* pos,
        const entwine::Schema& schema,
        bool rasterize) const
{
    std::vector<char> point(
            m_sleepyTree.getPointData(m_clipper.get(), m_ids[index()], schema));

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

