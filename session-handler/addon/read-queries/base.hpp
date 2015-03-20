#pragma once

#include <memory>

#include <pdal/Dimension.hpp>
#include <pdal/Compression.hpp>

#include <entwine/compression/stream.hpp>

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
    std::size_t index() const;

    entwine::CompressionStream m_compressionStream;
    std::shared_ptr<pdal::LazPerfCompressor<
            entwine::CompressionStream>> m_compressor;
    std::size_t m_compressionOffset;

    const entwine::Schema& m_schema;
    const bool m_rasterize;

private:
    virtual void readPoint(
            char* pos,
            const entwine::Schema& schema,
            bool rasterize) const = 0;

    virtual bool eof() const = 0;

    std::size_t m_index;
};

