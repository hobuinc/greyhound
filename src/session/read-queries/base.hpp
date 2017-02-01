#pragma once

#include <memory>
#include <vector>

#include <pdal/Dimension.hpp>
#include <pdal/Compression.hpp>

#include <entwine/types/schema.hpp>
#include <entwine/util/compression.hpp>

namespace entwine
{
    class Schema;
    class DimInfo;
}

class ReadQuery
{
public:
    ReadQuery(const entwine::Schema& schema, bool compress)
        : m_compressionStream(0)
        , m_compressor(
                compress ?
                    new pdal::LazPerfCompressor<entwine::CompressionStream>(
                        m_compressionStream,
                        schema.pdalLayout().dimTypes()) :
                    0)
        , m_compressionOffset(0)
        , m_schema(schema)
        , m_done(false)
    { }

    virtual ~ReadQuery() { if (m_compressor) m_compressor->done(); }

    void read(std::vector<char>& buffer)
    {
        if (m_done) throw std::runtime_error("Tried to call read() after done");

        buffer.clear();
        m_done = readSome(buffer);

        if (compress())
        {
            m_compressor->compress(buffer.data(), buffer.size());
            if (m_done) m_compressor->done();
            buffer = std::move(*m_compressionStream.data());
        }

        if (m_done)
        {
            const uint32_t points(numPoints());
            const char* pos(reinterpret_cast<const char*>(&points));
            buffer.insert(buffer.end(), pos, pos + sizeof(uint32_t));
        }
    }

    bool compress() const { return m_compressor.get() != 0; }
    bool done() const { return m_done; }
    virtual uint64_t numPoints() const = 0;

protected:
    // Must return true if done, else false.
    virtual bool readSome(std::vector<char>& buffer) = 0;

    entwine::CompressionStream m_compressionStream;
    std::unique_ptr<pdal::LazPerfCompressor<
            entwine::CompressionStream>> m_compressor;
    std::size_t m_compressionOffset;

    const entwine::Schema& m_schema;
    bool m_done;
};

