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
            std::size_t index = 0);
    virtual ~ReadQuery() { }

    void read(ItcBuffer& buffer);
    bool compress() const { return m_compressor.get() != 0; }
    bool done() const { return m_done; }
    virtual uint64_t numPoints() const = 0;

protected:
    // Must return true if done, else false.
    virtual bool readSome(ItcBuffer& buffer) = 0;

    void compressionSwap(ItcBuffer& buffer);

    entwine::CompressionStream m_compressionStream;
    std::unique_ptr<pdal::LazPerfCompressor<
            entwine::CompressionStream>> m_compressor;
    std::size_t m_compressionOffset;

    const entwine::Schema& m_schema;
    bool m_done;
};

