#include "read-command.hpp"
#include "buffer-pool.hpp"
#include "grey-reader.hpp"
#include "read-query.hpp"

UnindexedQueryData::UnindexedQueryData(
        std::shared_ptr<pdal::PointBuffer> pointBuffer,
        std::size_t start,
        std::size_t count)
    : m_pointBuffer(pointBuffer)
    , m_begin(std::min<std::size_t>(start, m_pointBuffer->size()))
    , m_end(
            count == 0 ?
                m_pointBuffer->size() :
                std::min<std::size_t>(start + count, m_pointBuffer->size()))
    , m_index(m_begin)
{ }

LiveQueryData::LiveQueryData(
        std::shared_ptr<pdal::PointBuffer> pointBuffer,
        std::vector<std::size_t> indexList)
    : m_pointBuffer(pointBuffer)
    , m_indexList(indexList)
    , m_index(0)
{ }

SerialQueryData::SerialQueryData(GreyQuery greyQuery)
    : m_greyQuery(greyQuery)
    , m_index(0)
{ }

void UnindexedQueryData::read(
        std::shared_ptr<ItcBuffer> buffer,
        std::size_t maxNumBytes,
        const Schema& schema,
        bool)
{
    const std::size_t stride(schema.stride());
    const std::size_t pointsToRead(
            std::min(m_end - m_index, maxNumBytes / stride));
    const std::size_t doneIndex(m_index + pointsToRead);

    try
    {
        buffer->resize(pointsToRead * stride);
        uint8_t* pos(buffer->data());

        while (m_index < doneIndex)
        {
            for (const auto& dim : schema.dims)
            {
                pos += readDim(pos, m_pointBuffer, dim, m_index);
            }

            ++m_index;
        }
    }
    catch (...)
    {
        throw std::runtime_error("Failed to read points from PDAL");
    }
}

void LiveQueryData::read(
        std::shared_ptr<ItcBuffer> buffer,
        std::size_t maxNumBytes,
        const Schema& schema,
        bool rasterize)
{
    const std::size_t stride(schema.stride(rasterize));

    const std::size_t pointsToRead(
            std::min(m_indexList.size() - m_index, maxNumBytes / stride));
    const std::size_t doneIndex(m_index + pointsToRead);

    try
    {
        buffer->resize(pointsToRead * stride);
        uint8_t* pos(buffer->data());

        while (m_index < doneIndex)
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
            else
            {
                if (rasterize)
                {
                    // Mark this point as a hole.
                    std::fill(pos, pos + 1, 0);
                }

                pos += stride;
            }

            ++m_index;
        }
    }
    catch (...)
    {
        throw std::runtime_error("Failed to read points from PDAL");
    }
}

void SerialQueryData::read(
        std::shared_ptr<ItcBuffer> buffer,
        std::size_t maxNumBytes,
        const Schema& schema,
        bool rasterize)
{
    const std::size_t stride(schema.stride(rasterize));

    const std::size_t pointsToRead(
            std::min(m_greyQuery.numPoints() - m_index, maxNumBytes / stride));
    const std::size_t doneIndex(m_index + pointsToRead);

    try
    {
        buffer->resize(pointsToRead * stride);
        uint8_t* pos(buffer->data());

        while (m_index < doneIndex)
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
            else
            {
                if (rasterize)
                {
                    // Mark this point as a hole.
                    std::fill(pos, pos + 1, 0);
                }

                pos += stride;
            }

            ++m_index;
        }
    }
    catch (...)
    {
        throw std::runtime_error("Failed to read points from PDAL");
    }
}

std::size_t QueryData::readDim(
        uint8_t* buffer,
        const std::shared_ptr<pdal::PointBuffer> pointBuffer,
        const DimInfo& dim,
        const std::size_t index)
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

