#include <iostream>

#include <pdal/PointBuffer.hpp>

#include "pdal-session.hpp"
#include "buffer-pool.hpp"

namespace
{
    bool rasterOmit(pdal::Dimension::Id::Enum id)
    {
        // These Dimensions are not explicitly placed in the output buffer
        // for rasterized requests.
        return id == pdal::Dimension::Id::X || id == pdal::Dimension::Id::Y;
    }

    std::size_t readDim(
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
}

UnindexedPrepData::UnindexedPrepData(
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

LivePrepData::LivePrepData(
        std::shared_ptr<pdal::PointBuffer> pointBuffer,
        std::vector<std::size_t> indexList)
    : m_pointBuffer(pointBuffer)
    , m_indexList(indexList)
    , m_index(0)
{ }

SerialPrepData::SerialPrepData(GreyQuery query)
    : m_query(query)
    , m_index(0)
{ }

void UnindexedPrepData::read(
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

void LivePrepData::read(
        std::shared_ptr<ItcBuffer> buffer,
        std::size_t maxNumBytes,
        const Schema& schema,
        bool rasterize)
{
    std::size_t stride(schema.stride());

    if (rasterize)
    {
        // Clientward rasterization schemas always contain a byte to specify
        // whether a point at this location in the raster exists.
        ++stride;

        for (auto dim : schema.dims)
        {
            if (rasterOmit(dim.id))
            {
                stride -= dim.size;
            }
        }
    }

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
                    if (!rasterize || !rasterOmit(dim.id))
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

void SerialPrepData::read(
        std::shared_ptr<ItcBuffer> buffer,
        std::size_t maxNumBytes,
        const Schema& schema,
        bool rasterize)
{
    std::size_t stride(schema.stride());

    if (rasterize)
    {
        // Clientward rasterization schemas always contain a byte to specify
        // whether a point at this location in the raster exists.
        ++stride;

        for (auto dim : schema.dims)
        {
            if (rasterOmit(dim.id))
            {
                stride -= dim.size;
            }
        }
    }

    const std::size_t pointsToRead(
            std::min(m_query.numPoints() - m_index, maxNumBytes / stride));
    const std::size_t doneIndex(m_index + pointsToRead);

    try
    {
        buffer->resize(pointsToRead * stride);
        uint8_t* pos(buffer->data());

        while (m_index < doneIndex)
        {
            QueryIndex queryIndex(m_query.queryIndex(m_index));

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
                    if (!rasterize || !rasterOmit(dim.id))
                    {
                        pos += readDim(
                                pos,
                                m_query.pointBuffer(queryIndex.id),
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

PdalSession::PdalSession()
    : m_pipelineId()
    , m_pipeline()
    , m_liveDataSource()
    , m_serialDataSource()
    , m_initOnce()
    , m_awakenMutex()
{ }

void PdalSession::initialize(
        const std::string& pipelineId,
        const std::string& pipeline,
        const std::vector<std::string>& serialPaths,
        const bool execute)
{
    m_initOnce.ensure([this, &pipelineId, &pipeline, &serialPaths, execute]() {
        m_pipelineId = pipelineId;
        m_pipeline = pipeline;

        // Try to awaken from serialized source.  If unsuccessful, initialize a
        // live source.  For pipeline validation only, this will be called with
        // an empty pipelineId since it does not exist yet.  In this case,
        // always perform live initialization.
        if (!pipelineId.size() || (!awaken(serialPaths) && !m_liveDataSource))
        {
            m_liveDataSource.reset(
                    new LiveDataSource(pipelineId, pipeline, execute));

            if (pipelineId.size())
                std::cout << "Created live source " << pipelineId << std::endl;
        }
    });
}

std::size_t PdalSession::getNumPoints() const
{
    if (m_serialDataSource)     return m_serialDataSource->getNumPoints();
    else if (m_liveDataSource)  return m_liveDataSource->getNumPoints();
    else throw std::runtime_error("Not initialized!");
}

std::string PdalSession::getSchema() const
{
    if (m_serialDataSource)     return m_serialDataSource->getSchema();
    else if (m_liveDataSource)  return m_liveDataSource->getSchema();
    else throw std::runtime_error("Not initialized!");
}

std::string PdalSession::getStats() const
{
    if (m_serialDataSource)     return m_serialDataSource->getStats();
    else if (m_liveDataSource)  return m_liveDataSource->getStats();
    else throw std::runtime_error("Not initialized!");
}

std::string PdalSession::getSrs() const
{
    if (m_serialDataSource)     return m_serialDataSource->getSrs();
    else if (m_liveDataSource)  return m_liveDataSource->getSrs();
    else throw std::runtime_error("Not initialized!");
}

std::vector<std::size_t> PdalSession::getFills() const
{
    if (m_serialDataSource)     return m_serialDataSource->getFills();
    else if (m_liveDataSource)  return m_liveDataSource->getFills();
    else throw std::runtime_error("Not initialized!");
}

void PdalSession::serialize(const std::vector<std::string>& serialPaths)
{
    if (m_liveDataSource && !m_serialDataSource)
    {
        m_liveDataSource->serialize(serialPaths);
        std::cout << "Serialized - awakening " << m_pipelineId << std::endl;
        awaken(serialPaths);

        // TODO Safely clear the quadIndex from the live data source.  All its
        // pending READs must complete first - new ones (except for the custom
        // raster) will already be routed to the serial data source.
    }
}

bool PdalSession::awaken(const std::vector<std::string>& serialPaths)
{
    bool awoken(false);

    m_awakenMutex.lock();
    try
    {
        if (!m_serialDataSource &&
            GreyReader::exists(m_pipelineId, serialPaths))
        {
            m_serialDataSource.reset(new GreyReader(m_pipelineId, serialPaths));

            if (m_serialDataSource)
            {
                std::cout << "Created serial source ";
                awoken = true;
            }
            else
            {
                std::cout << "Failed serial source ";
            }

            std::cout << m_pipelineId << std::endl;
        }
        else if (m_serialDataSource)
        {
            awoken = true;
        }

    }
    catch (...)
    {
        std::cout << "Caught exception in awaken" << std::endl;
        m_serialDataSource.reset();
        m_awakenMutex.unlock();
    }
    m_awakenMutex.unlock();

    return awoken;
}

std::shared_ptr<PrepData> PdalSession::prepUnindexed(
        std::size_t start,
        std::size_t count)
{
    std::cout << "Unindexed read - prep" << std::endl;
    // Unindexed read queries are only supported by a live data source.
    if (!m_liveDataSource)
    {
        std::cout << "Resetting live data source" << std::endl;
        m_liveDataSource.reset(
                new LiveDataSource(m_pipelineId, m_pipeline, true));
    }

    return std::shared_ptr<PrepData>(new UnindexedPrepData(
                m_liveDataSource->pointBuffer(),
                start,
                count));
}

std::shared_ptr<PrepData> PdalSession::prep(
        double xMin,
        double yMin,
        double xMax,
        double yMax,
        std::size_t depthBegin,
        std::size_t depthEnd)
{
    std::ostringstream logParams;
    logParams <<
            "\tPipeline ID: " << m_pipelineId << "\n" <<
            "\tBBox: [" << xMin << "," << yMin << "," <<
                           xMax << "," << yMax << "]\n" <<
            "\tDepth range: " << depthBegin << "-" << depthEnd;

    if (m_serialDataSource)
    {
        std::cout <<
            "Reading from serial source\n" <<
            logParams.str() <<
            std::endl;

        return std::shared_ptr<PrepData>(
                new SerialPrepData(m_serialDataSource->prep(
                    xMin,
                    yMin,
                    xMax,
                    yMax,
                    depthBegin,
                    depthEnd)));
    }
    else if (m_liveDataSource)
    {
        std::cout <<
            "Reading from live source\n" <<
            logParams.str() <<
            std::endl;

        return std::shared_ptr<PrepData>(
                new LivePrepData(
                    m_liveDataSource->pointBuffer(),
                    m_liveDataSource->prep(
                        xMin,
                        yMin,
                        xMax,
                        yMax,
                        depthBegin,
                        depthEnd)));
    }
    else
    {
        throw std::runtime_error("Not initialized!");
    }
}

std::shared_ptr<PrepData> PdalSession::prep(
        std::size_t depthBegin,
        std::size_t depthEnd)
{
    std::ostringstream logParams;
    logParams <<
            "\tPipeline ID: " << m_pipelineId << "\n" <<
            "\tDepth range: " << depthBegin << "-" << depthEnd;

    if (m_serialDataSource)
    {
        std::cout <<
            "Reading from serial source\n" <<
            logParams.str() <<
            std::endl;

        return std::shared_ptr<PrepData>(
                new SerialPrepData(m_serialDataSource->prep(
                    depthBegin,
                    depthEnd)));
    }
    else if (m_liveDataSource)
    {
        std::cout <<
            "Reading from live source\n" <<
            logParams.str() <<
            std::endl;

        return std::shared_ptr<PrepData>(
                new LivePrepData(
                    m_liveDataSource->pointBuffer(),
                    m_liveDataSource->prep(
                        depthBegin,
                        depthEnd)));
    }
    else
    {
        throw std::runtime_error("Not initialized!");
    }
}

std::shared_ptr<PrepData> PdalSession::prep(
        std::size_t rasterize,
        RasterMeta& rasterMeta)
{
    std::ostringstream logParams;
    logParams <<
            "\tPipeline ID: " << m_pipelineId << "\n" <<
            "\tRasterize: " << rasterize << "\n";

    if (m_serialDataSource)
    {
        std::cout <<
            "Reading from serial source\n" <<
            logParams.str() <<
            std::endl;

        return std::shared_ptr<PrepData>(
                new SerialPrepData(m_serialDataSource->prep(
                    rasterize,
                    rasterMeta)));
    }
    else if (m_liveDataSource)
    {
        std::cout <<
            "Reading from live source\n" <<
            logParams.str() <<
            std::endl;

        return std::shared_ptr<PrepData>(
                new LivePrepData(
                    m_liveDataSource->pointBuffer(),
                    m_liveDataSource->prep(
                        rasterize,
                        rasterMeta)));
    }
    else
    {
        throw std::runtime_error("not initialized!");
    }
}

std::shared_ptr<PrepData> PdalSession::prep(const RasterMeta& rasterMeta)
{
    // Custom-res raster queries are only supported by a live data source.
    if (!m_liveDataSource)
    {
        m_liveDataSource.reset(
                new LiveDataSource(m_pipelineId, m_pipeline, true));
    }

    if (!m_liveDataSource)
        throw std::runtime_error("not initialized!");

    return std::shared_ptr<PrepData>(
            new LivePrepData(
                m_liveDataSource->pointBuffer(),
                m_liveDataSource->prep(rasterMeta)));
}

std::shared_ptr<PrepData> PdalSession::prep(
        bool is3d,
        double radius,
        double x,
        double y,
        double z)
{
    // KD-indexed read queries are only supported by a live data source.
    if (!m_liveDataSource)
    {
        m_liveDataSource.reset(
                new LiveDataSource(m_pipelineId, m_pipeline, true));
    }

    if (!m_liveDataSource)
        throw std::runtime_error("not initialized!");

    return std::shared_ptr<PrepData>(
            new LivePrepData(
                m_liveDataSource->pointBuffer(),
                m_liveDataSource->prep(is3d, radius, x, y, z)));
}

