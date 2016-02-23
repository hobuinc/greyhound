#pragma once

#include <entwine/reader/reader.hpp>

#include "read-queries/base.hpp"

namespace entwine
{
    class Query;
    class Schema;
}

class EntwineReadQuery : public ReadQuery
{
public:
    EntwineReadQuery(
            const entwine::Schema& schema,
            bool compress,
            std::unique_ptr<entwine::Query> query);

    ~EntwineReadQuery();

private:
    virtual bool readSome(ItcBuffer& buffer);
    virtual std::size_t numPoints() const;

    std::unique_ptr<entwine::Query> m_query;
};

