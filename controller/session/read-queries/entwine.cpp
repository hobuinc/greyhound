#include "read-queries/entwine.hpp"

#include <entwine/tree/clipper.hpp>
#include <entwine/tree/reader.hpp>
#include <entwine/types/schema.hpp>

EntwineReadQuery::EntwineReadQuery(
        const entwine::Schema& schema,
        bool compress,
        bool rasterize,
        entwine::Reader& entwine,
        std::vector<std::size_t> ids)
    : ReadQuery(schema, compress, rasterize)
    , m_entwine(entwine)
    , m_ids(ids)
{ }

EntwineReadQuery::~EntwineReadQuery()
{ }

void EntwineReadQuery::readPoint(
        char* pos,
        const entwine::Schema& schema,
        bool rasterize)
{
    std::vector<char> point(m_entwine.getPointData(m_ids[index()], schema));
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

