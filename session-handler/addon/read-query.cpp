#include "buffer-pool.hpp"
#include "types/schema.hpp"

#include "read-query.hpp"

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

UnindexedReadQuery::UnindexedReadQuery(
        const Schema& schema,
        bool compress,
        std::shared_ptr<pdal::PointBuffer> pointBuffer,
        std::size_t start,
        std::size_t count)
    : ReadQuery(
            pointBuffer->dimTypes(),
            schema,
            compress,
            false,
            std::min<std::size_t>(start, pointBuffer->size()))
    , m_pointBuffer(pointBuffer)
    , m_begin(std::min<std::size_t>(start, m_pointBuffer->size()))
    , m_end(
            count == 0 ?
                m_pointBuffer->size() :
                std::min<std::size_t>(start + count, m_pointBuffer->size()))
{ }

LiveReadQuery::LiveReadQuery(
        const Schema& schema,
        bool compress,
        bool rasterize,
        std::shared_ptr<pdal::PointBuffer> pointBuffer,
        std::vector<std::size_t> indexList)
    : ReadQuery(pointBuffer->dimTypes(), schema, compress, rasterize)
    , m_pointBuffer(pointBuffer)
    , m_indexList(indexList)
{ }

SerialReadQuery::SerialReadQuery(
        const Schema& schema,
        bool compress,
        bool rasterize,
        GreyQuery greyQuery)
    : ReadQuery(greyQuery.dimTypes(), schema, compress, rasterize)
    , m_greyQuery(greyQuery)
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

void UnindexedReadQuery::readPoint(
        uint8_t* pos,
        const Schema& schema,
        bool) const
{
    for (const auto& dim : schema.dims)
    {
        pos += readDim(pos, m_pointBuffer, dim, m_index);
    }
}

void LiveReadQuery::readPoint(
        uint8_t* pos,
        const Schema& schema,
        bool rasterize) const
{
    const std::size_t i(m_indexList[m_index]);
    if (i != std::numeric_limits<std::size_t>::max())
    {
        if (rasterize)
        {
            // Mark this point as a valid point.
            std::fill(pos, pos + 1, 1);
            ++pos;
        }

        for (const auto& dim : schema.dims)
        {
            if (schema.use(dim, rasterize))
            {
                pos += readDim(pos, m_pointBuffer, dim, i);
            }
        }
    }
    else if (rasterize)
    {
        // Mark this point as a hole.  Don't clear the rest of the
        // point data so it will compress nicely if enabled.
        std::fill(pos, pos + 1, 0);
    }
}

void SerialReadQuery::readPoint(
        uint8_t* pos,
        const Schema& schema,
        bool rasterize) const
{
    QueryIndex queryIndex(m_greyQuery.queryIndex(m_index));

    if (queryIndex.index != std::numeric_limits<std::size_t>::max())
    {
        if (rasterize)
        {
            // Mark this point as a valid point.
            std::fill(pos, pos + 1, 1);
            ++pos;
        }

        for (const auto& dim : schema.dims)
        {
            if (schema.use(dim, rasterize))
            {
                pos += readDim(
                        pos,
                        m_greyQuery.pointBuffer(queryIndex.id),
                        dim,
                        queryIndex.index);
            }
        }
    }
    else if (rasterize)
    {
        // Mark this point as a hole.  Don't clear the rest of the
        // point data so it will compress nicely if enabled.
        std::fill(pos, pos + 1, 0);
    }
}

std::size_t ReadQuery::readDim(
        uint8_t* buffer,
        const std::shared_ptr<pdal::PointBuffer> pointBuffer,
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

