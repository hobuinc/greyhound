#pragma once

#include <entwine/reader/reader.hpp>
#include <entwine/types/schema.hpp>

#include "read-queries/base.hpp"

class EntwineReadQuery : public ReadQuery
{
public:
    EntwineReadQuery(bool compress, std::unique_ptr<entwine::Query> query)
        : ReadQuery(query->schema(), compress)
        , m_query(std::move(query))
    { }

private:
    virtual bool readSome(std::vector<char>& buffer) override
    {
        m_query->next(buffer);
        return m_query->done();
    }

    virtual uint64_t numPoints() const override
    {
        return m_query->numPoints();
    }

    std::unique_ptr<entwine::Query> m_query;
};

