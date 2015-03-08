#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "read-queries/base.hpp"

namespace pdal
{
    class PointBuffer;
}

namespace entwine
{
    class Schema;
}

class LiveReadQuery : public ReadQuery
{
public:
    LiveReadQuery(
            const entwine::Schema& schema,
            bool compress,
            bool rasterize,
            std::shared_ptr<pdal::PointBuffer> pointBuffer,
            std::vector<std::size_t> indexList);

    const std::vector<std::size_t>& indexList() const;
    virtual bool eof() const;

    virtual std::size_t numPoints() const;

private:
    const std::shared_ptr<pdal::PointBuffer> m_pointBuffer;

    const std::vector<std::size_t> m_indexList;

    virtual void readPoint(
            uint8_t* pos,
            const entwine::Schema& schema,
            bool rasterize) const;
};

