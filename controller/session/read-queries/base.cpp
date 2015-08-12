#include "read-queries/base.hpp"

#include <entwine/types/schema.hpp>

#include "util/buffer-pool.hpp"

ReadQuery::ReadQuery(
        const entwine::Schema& schema,
        const bool compress,
        const std::size_t index)
    : m_compressionStream()
    , m_compressor(
            compress ?
                new pdal::LazPerfCompressor<entwine::CompressionStream>(
                    m_compressionStream,
                    schema.pdalLayout().dimTypes()) :
                0)
    , m_compressionOffset(0)
    , m_schema(schema)
    , m_index(index)
{ }

bool ReadQuery::done() const
{
    return eof() &&
            (!compress() ||
             m_compressionOffset == m_compressionStream.data().size());
}

void ReadQuery::read(std::shared_ptr<ItcBuffer> buffer, std::size_t maxNumBytes)
{
    try
    {
        buffer->resize(0);
        const std::size_t pointSize(m_schema.pointSize());
        std::vector<char> point(pointSize);
        char* pos(0);

        const bool doCompress(compress());

        while (
                !eof() &&
                ((!doCompress &&
                    buffer->size() + pointSize <= maxNumBytes) ||
                 (doCompress &&
                    m_compressionStream.data().size() - m_compressionOffset <=
                            maxNumBytes)))
        {
            pos = point.data();

            // Delegate to specialized subclass.
            readPoint(pos, m_schema);

            if (doCompress)
            {
                m_compressor->compress(point.data(), pointSize);
            }
            else
            {
                buffer->push(point.data(), pointSize);
            }

            ++m_index;
        }

        if (doCompress)
        {
            if (eof()) m_compressor->done();

            const std::size_t size(
                    std::min(
                        m_compressionStream.data().size() - m_compressionOffset,
                        maxNumBytes));

            buffer->push(
                    m_compressionStream.data().data() + m_compressionOffset,
                    size);

            m_compressionOffset += size;
        }
    }
    catch (...)
    {
        throw std::runtime_error("Failed to read points from PDAL");
    }
}

std::size_t ReadQuery::index() const
{
    return m_index;
}

