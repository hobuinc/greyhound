#include <iostream>

#include "pdal-session.hpp"

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
    if (m_serialDataSource)
    {
        return m_serialDataSource->getNumPoints();
    }
    else if (m_liveDataSource)
    {
        return m_liveDataSource->getNumPoints();
    }
    else
    {
        throw std::runtime_error("Not initialized!");
    }
}

std::string PdalSession::getSchema() const
{
    if (m_serialDataSource)
    {
        return m_serialDataSource->getSchema();
    }
    else if (m_liveDataSource)
    {
        return m_liveDataSource->getSchema();
    }
    else
    {
        throw std::runtime_error("Not initialized!");
    }
}

std::string PdalSession::getStats() const
{
    if (m_serialDataSource)
    {
        return m_serialDataSource->getStats();
    }
    else if (m_liveDataSource)
    {
        return m_liveDataSource->getStats();
    }
    else
    {
        throw std::runtime_error("Not initialized!");
    }
}

std::string PdalSession::getSrs() const
{
    if (m_serialDataSource)
    {
        return m_serialDataSource->getSrs();
    }
    else if (m_liveDataSource)
    {
        return m_liveDataSource->getSrs();
    }
    else
    {
        throw std::runtime_error("Not initialized!");
    }
}

std::vector<std::size_t> PdalSession::getFills() const
{
    if (m_serialDataSource)
    {
        return m_serialDataSource->getFills();
    }
    else if (m_liveDataSource)
    {
        return m_liveDataSource->getFills();
    }
    else
    {
        throw std::runtime_error("Not initialized!");
    }
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

std::size_t PdalSession::readUnindexed(
        std::vector<unsigned char>& buffer,
        const Schema& schema,
        std::size_t start,
        std::size_t count)
{
    // Unindexed read queries are only supported by a live data source.
    if (!m_liveDataSource)
    {
        m_liveDataSource.reset(
                new LiveDataSource(m_pipelineId, m_pipeline, true));
    }

    return m_liveDataSource ?
        m_liveDataSource->readUnindexed(buffer, schema, start, count) : 0;
}

std::size_t PdalSession::read(
        std::vector<unsigned char>& buffer,
        const Schema& schema,
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

        return m_serialDataSource->read(
                buffer,
                schema,
                xMin,
                yMin,
                xMax,
                yMax,
                depthBegin,
                depthEnd);
    }
    else if (m_liveDataSource)
    {
        std::cout <<
            "Reading from live source\n" <<
            logParams.str() <<
            std::endl;

        return m_liveDataSource->read(
                buffer,
                schema,
                xMin,
                yMin,
                xMax,
                yMax,
                depthBegin,
                depthEnd);
    }
    else
    {
        throw std::runtime_error("Not initialized!");
    }
}

std::size_t PdalSession::read(
        std::vector<unsigned char>& buffer,
        const Schema& schema,
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

        return m_serialDataSource->read(buffer, schema, depthBegin, depthEnd);
    }
    else if (m_liveDataSource)
    {
        std::cout <<
            "Reading from live source\n" <<
            logParams.str() <<
            std::endl;

        return m_liveDataSource->read(buffer, schema, depthBegin, depthEnd);
    }
    else
    {
        throw std::runtime_error("Not initialized!");
    }
}

std::size_t PdalSession::read(
        std::vector<unsigned char>& buffer,
        const Schema& schema,
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

        return m_serialDataSource->read(buffer, schema, rasterize, rasterMeta);
    }
    else if (m_liveDataSource)
    {
        std::cout <<
            "Reading from live source\n" <<
            logParams.str() <<
            std::endl;

        return m_liveDataSource->read(buffer, schema, rasterize, rasterMeta);
    }
    else
    {
        throw std::runtime_error("Not initialized!");
    }
}

std::size_t PdalSession::read(
        std::vector<unsigned char>& buffer,
        const Schema& schema,
        const RasterMeta& rasterMeta)
{
    // Custom-res raster queries are only supported by a live data source.
    if (!m_liveDataSource)
    {
        m_liveDataSource.reset(
                new LiveDataSource(m_pipelineId, m_pipeline, true));
    }

    return
        m_liveDataSource ?
            m_liveDataSource->read(buffer, schema, rasterMeta) : 0;
}

std::size_t PdalSession::read(
        std::vector<unsigned char>& buffer,
        const Schema& schema,
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

    return
        m_liveDataSource ?
            m_liveDataSource->read(buffer, schema, is3d, radius, x, y, z) : 0;
}

