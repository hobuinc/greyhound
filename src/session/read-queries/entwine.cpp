#include "read-queries/entwine.hpp"

#include <entwine/reader/query.hpp>
#include <entwine/tree/clipper.hpp>
#include <entwine/types/schema.hpp>

#include "util/buffer-pool.hpp"

EntwineReadQuery::EntwineReadQuery(
        const entwine::Schema& schema,
        bool compress,
        std::unique_ptr<entwine::Query> query)
    : ReadQuery(schema, compress)
    , m_query(std::move(query))
{ }

EntwineReadQuery::~EntwineReadQuery()
{ }

bool EntwineReadQuery::readSome(std::vector<char>& buffer)
{
    m_query->next(buffer);
    return m_query->done();
}

std::uint64_t EntwineReadQuery::numPoints() const
{
    return m_query->numPoints();
}

