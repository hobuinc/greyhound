#pragma once

#include "read-queries/base.hpp"

namespace entwine
{
    class Schema;
    class SleepyTree;
}

class EntwineReadQuery : public ReadQuery
{
public:
    EntwineReadQuery(
            const entwine::Schema& schema,
            bool compress,
            bool rasterize,
            entwine::SleepyTree& sleepyTree,
            const std::vector<std::size_t>& ids);

    virtual bool eof() const;

    virtual std::size_t numPoints() const;

private:
    entwine::SleepyTree& m_sleepyTree;
    std::vector<std::size_t> m_ids;

    virtual void readPoint(
            char* pos,
            const entwine::Schema& schema,
            bool rasterize) const;
};

