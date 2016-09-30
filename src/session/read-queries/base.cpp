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

void ReadQuery::read(ItcBuffer& buffer)
{
    if (m_done) throw std::runtime_error("Tried to call read() after done");

    buffer.resize(0);
    m_done = readSome(buffer);

    if (buffer.size())
    {
        std::cout << "Read " << buffer.size() << " bytes.  Done? " << m_done <<
            std::endl;
    }

    if (compress())
    {
        m_compressor->compress(buffer.data(), buffer.size());
        if (m_done) m_compressor->done();
        compressionSwap(buffer);
    }

    if (m_done)
    {
        std::cout << "Done.  NP: " << numPoints() << std::endl;
        const uint32_t points(numPoints());
        const char* pos(reinterpret_cast<const char*>(&points));
        buffer.push(pos, sizeof(uint32_t));
    }
}

void ReadQuery::compressionSwap(ItcBuffer& buffer)
{
    std::unique_ptr<std::vector<char>> compressed(m_compressionStream.data());
    buffer.resize(0);
    buffer.push(compressed->data(), compressed->size());
}

