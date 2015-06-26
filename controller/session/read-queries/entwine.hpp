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
            bool rasterize,
            std::unique_ptr<entwine::Query> query);

    ~EntwineReadQuery();

    virtual bool eof() const;

    virtual std::size_t numPoints() const;

private:
    std::unique_ptr<entwine::Query> m_query;

    virtual void readPoint(
            char* pos,
            const entwine::Schema& schema,
            bool rasterize);
};

