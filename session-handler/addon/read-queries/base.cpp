#include <pdal/PointBuffer.hpp>

#include "buffer-pool.hpp"
#include "types/schema.hpp"

#include "read-queries/base.hpp"

namespace
{
    pdal::DimTypeList pruneDims(
            pdal::DimTypeList input,
            const Schema& schema,
            bool rasterize)
    {
        pdal::DimTypeList output;

        for (const auto& dim : schema.dims)
        {
            if (schema.use(dim, rasterize))
            {
                output.push_back(pdal::DimType(dim.id, dim.type));
            }
        }

        return output;
    }
}

ReadQuery::ReadQuery(
        pdal::DimTypeList dimTypes,
        const Schema& schema,
        const bool compress,
        const bool rasterize,
        const std::size_t index)
    : m_compressionStream()
    , m_compressor(
            compress ?
                new pdal::LazPerfCompressor<CompressionStream>(
                    m_compressionStream,
                    pruneDims(dimTypes, schema, rasterize)) :
                0)
    , m_compressionOffset(0)
    , m_schema(schema)
    , m_rasterize(rasterize)
    , m_index(index)
{ }

bool ReadQuery::done() const
{
    return eof() &&
            (!compress() ||
             m_compressionOffset == m_compressionStream.data().size());
}

void ReadQuery::read(
        std::shared_ptr<ItcBuffer> buffer,
        std::size_t maxNumBytes,
        bool rasterize)
{
    try
    {
        buffer->resize(0);
        const std::size_t stride(m_schema.stride(rasterize));
        std::vector<uint8_t> point(stride);
        uint8_t* pos(0);

        const bool doCompress(compress());

        while (
                !eof() &&
                ((!doCompress &&
                    buffer->size() + stride <= maxNumBytes) ||
                 (doCompress &&
                    m_compressionStream.data().size() - m_compressionOffset <=
                            maxNumBytes)))
        {
            pos = point.data();

            // Delegate to specialized subclass.
            readPoint(pos, m_schema, rasterize);

            if (doCompress)
            {
                m_compressor->compress(
                    reinterpret_cast<char*>(point.data()), stride);
            }
            else
            {
                buffer->push(point.data(), stride);
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

std::size_t ReadQuery::readDim(
        uint8_t* buffer,
        const pdal::PointBuffer* pointBuffer,
        const DimInfo& dim,
        const std::size_t index) const
{
    pointBuffer->getField(
            reinterpret_cast<char*>(buffer),
            dim.id,
            dim.type,
            index);

    return dim.size();
}

