#include "read-queries/base.hpp"

#include <entwine/types/schema.hpp>

#include "util/buffer-pool.hpp"

ReadQuery::ReadQuery(
        const entwine::Schema& schema,
        const bool compress,
        const std::size_t index)
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

void ReadQuery::read(std::vector<char>& buffer)
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

