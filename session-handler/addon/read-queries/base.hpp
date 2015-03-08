#pragma once

#include <memory>

#include <pdal/Dimension.hpp>
#include <pdal/Compression.hpp>

#include "compression-stream.hpp"
#include "grey/reader-types.hpp"

namespace pdal
{
    class PointBuffer;
}

namespace entwine
{
    class Schema;
    class DimInfo;
}

class ItcBuffer;

class ReadQuery
{
public:
    ReadQuery(
            pdal::DimTypeList dimTypes,
            const entwine::Schema& schema,
            bool compress,
            bool rasterize,
            std::size_t index = 0);

    virtual std::size_t numPoints() const = 0;

    bool done() const;
    bool compress() const { return m_compressor.get() != 0; }
    virtual bool serial() const { return false; }

    void read(
            std::shared_ptr<ItcBuffer> buffer,
            std::size_t maxNumBytes,
            bool rasterize = false);

protected:
    std::size_t readDim(
            uint8_t* buffer,
            const pdal::PointBuffer* pointBuffer,
            const entwine::DimInfo& dim,
            const std::size_t index) const;

    CompressionStream m_compressionStream;
    std::shared_ptr<pdal::LazPerfCompressor<CompressionStream>> m_compressor;
    std::size_t m_compressionOffset;

    const entwine::Schema& m_schema;
    const bool m_rasterize;
    std::size_t m_index;

private:
    virtual void readPoint(
            uint8_t* pos,
            const entwine::Schema& schema,
            bool rasterize) const = 0;

    virtual bool eof() const = 0;
};

