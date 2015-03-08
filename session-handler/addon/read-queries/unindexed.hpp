#pragma once

#include "read-queries/base.hpp"

namespace pdal
{
    class PointBuffer;
}

namespace entwine
{
    class Schema;
}

class UnindexedReadQuery : public ReadQuery
{
public:
    UnindexedReadQuery(
            const entwine::Schema& schema,
            bool compress,
            std::shared_ptr<pdal::PointBuffer> pointBuffer,
            std::size_t start,
            std::size_t count);

    virtual std::size_t numPoints() const;
    virtual bool eof() const;

private:
    const std::shared_ptr<pdal::PointBuffer> m_pointBuffer;

    const std::size_t m_begin;
    const std::size_t m_end;

    virtual void readPoint(
            uint8_t* pos,
            const entwine::Schema& schema,
            bool rasterize) const;
};

