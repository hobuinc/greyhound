#include "read-command.hpp"
#include "buffer-pool.hpp"
#include "grey-reader.hpp"
#include "read-query.hpp"
#include "compression-stream.hpp"

namespace
{
    pdal::DimTypeList pruneDims(
            pdal::DimTypeList input,
            const Schema& schema,
            bool rasterize)
    {
        using namespace pdal;
        using namespace pdal::Dimension;
        pdal::DimTypeList output;

        for (const auto& dim : schema.dims)
        {
            if (schema.use(dim, rasterize))
            {
                const BaseType::Enum type(fromName(dim.type));
                if (type == BaseType::Signed)
                {
                    switch (dim.size)
                    {
                    case 1:
                        output.push_back(DimType(dim.id, Type::Signed8));
                        break;
                    case 2:
                        output.push_back(DimType(dim.id, Type::Signed16));
                        break;
                    case 4:
                        output.push_back(DimType(dim.id, Type::Signed32));
                        break;
                    case 8:
                        output.push_back(DimType(dim.id, Type::Signed64));
                        break;
                    default:
                        throw std::runtime_error("Invalid dimension");
                        break;
                    }
                }
                else if (type == BaseType::Unsigned)
                {
                    switch (dim.size)
                    {
                    case 1:
                        output.push_back(DimType(dim.id, Type::Unsigned8));
                        break;
                    case 2:
                        output.push_back(DimType(dim.id, Type::Unsigned16));
                        break;
                    case 4:
                        output.push_back(DimType(dim.id, Type::Unsigned32));
                        break;
                    case 8:
                        output.push_back(DimType(dim.id, Type::Unsigned64));
                        break;
                    default:
                        throw std::runtime_error("Invalid dimension");
                        break;
                    }
                }
                else
                {
                    if (dim.size == 4)
                    {
                        output.push_back(DimType(dim.id, Type::Float));
                    }
                    else if (dim.size == 8)
                    {
                        output.push_back(DimType(dim.id, Type::Double));
                    }
                    else {
                        throw std::runtime_error("Invalid dimension");
                    }
                }
            }
        }

        return output;
    }
}

QueryData::QueryData(
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

UnindexedQueryData::UnindexedQueryData(
        const Schema& schema,
        bool compress,
        std::shared_ptr<pdal::PointBuffer> pointBuffer,
        std::size_t start,
        std::size_t count)
    : QueryData(
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

LiveQueryData::LiveQueryData(
        const Schema& schema,
        bool compress,
        bool rasterize,
        std::shared_ptr<pdal::PointBuffer> pointBuffer,
        std::vector<std::size_t> indexList)
    : QueryData(pointBuffer->dimTypes(), schema, compress, rasterize)
    , m_pointBuffer(pointBuffer)
    , m_indexList(indexList)
{ }

SerialQueryData::SerialQueryData(
        const Schema& schema,
        bool compress,
        bool rasterize,
        GreyQuery greyQuery)
    : QueryData(greyQuery.dimTypes(), schema, compress, rasterize)
    , m_greyQuery(greyQuery)
{ }

bool QueryData::done() const
{
    return eof() &&
            (!compress() ||
             m_compressionOffset == m_compressionStream.data().size());
}

void QueryData::read(
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

void UnindexedQueryData::readPoint(
        uint8_t* pos,
        const Schema& schema,
        bool) const
{
    for (const auto& dim : schema.dims)
    {
        pos += readDim(pos, m_pointBuffer, dim, m_index);
    }
}

void LiveQueryData::readPoint(
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

void SerialQueryData::readPoint(
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

std::size_t QueryData::readDim(
        uint8_t* buffer,
        const std::shared_ptr<pdal::PointBuffer> pointBuffer,
        const DimInfo& dim,
        const std::size_t index) const
{
    if (dim.type == "floating")
    {
        if (dim.size == 4)
        {
            float val(pointBuffer->getFieldAs<float>(dim.id, index));
            std::memcpy(buffer, &val, dim.size);
        }
        else if (dim.size == 8)
        {
            double val(pointBuffer->getFieldAs<double>(dim.id, index));
            std::memcpy(buffer, &val, dim.size);
        }
        else
        {
            throw std::runtime_error("Invalid floating size requested");
        }
    }
    else if (dim.type == "signed" || dim.type == "unsigned")
    {
        if (dim.size == 1)
        {
            uint8_t val(pointBuffer->getFieldAs<uint8_t>(dim.id, index));
            std::memcpy(buffer, &val, dim.size);
        }
        else if (dim.size == 2)
        {
            uint16_t val(pointBuffer->getFieldAs<uint16_t>(dim.id, index));
            std::memcpy(buffer, &val, dim.size);
        }
        else if (dim.size == 4)
        {
            uint32_t val(pointBuffer->getFieldAs<uint32_t>(dim.id, index));
            std::memcpy(buffer, &val, dim.size);
        }
        else if (dim.size == 8)
        {
            uint64_t val(pointBuffer->getFieldAs<uint64_t>(dim.id, index));
            std::memcpy(buffer, &val, dim.size);
        }
        else
        {
            throw std::runtime_error("Invalid integer size requested");
        }
    }
    else
    {
        throw std::runtime_error("Invalid dimension type requested");
    }

    return dim.size;
}

