#pragma once

#include "read-queries/base.hpp"

namespace entwine
{
    class Schema;
    class Reader;
}

class EntwineReadQuery : public ReadQuery
{
public:
    EntwineReadQuery(
            const entwine::Schema& schema,
            bool compress,
            bool rasterize,
            entwine::Reader& entwine,
            std::vector<std::size_t> ids);
    ~EntwineReadQuery();

    virtual bool eof() const;

    virtual std::size_t numPoints() const;

private:
    entwine::Reader& m_entwine;
    std::vector<std::size_t> m_ids;

    virtual void readPoint(
            char* pos,
            const entwine::Schema& schema,
            bool rasterize);
};

