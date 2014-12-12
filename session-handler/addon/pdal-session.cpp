#include "pdal-session.hpp"

PdalSession::PdalSession()
    : m_pipelineId()
    , m_liveDataSource()
    , m_serialDataSource()
{ }

void PdalSession::initialize(
        const std::string& pipelineId,
        const std::string& pipeline,
        const bool execute)
{
    m_pipelineId = pipelineId;
    m_pipeline = pipeline;

    // Try to awaken from serialized source.  If unsuccessful, initialize a
    // live source.
    if (!awaken() && !m_liveDataSource)
    {
        m_liveDataSource.reset(
                new LiveDataSource(pipelineId, pipeline, execute));
    }
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

void PdalSession::serialize()
{
    if (m_liveDataSource)
    {
        m_liveDataSource->serialize();
    }
}

bool PdalSession::awaken()
{
    bool awoken(false);

    if (!m_serialDataSource && GreyReader::exists(m_pipelineId))
    {
        try
        {
            m_serialDataSource.reset(new GreyReader(m_pipelineId));
        }
        catch (...)
        {
            std::cout << "Caught exception in serial source init" << std::endl;
            m_serialDataSource.reset();
        }

        if (m_serialDataSource)
        {
            awoken = true;
        }
    }

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
    // TODO
    /*
    if (m_serialDataSource)
    {
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
    else */
    if (m_liveDataSource)
    {
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
    if (m_serialDataSource)
    {
        return m_serialDataSource->read(buffer, schema, depthBegin, depthEnd);
    }
    else if (m_liveDataSource)
    {
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
    // TODO
    /*
    if (m_serialDataSource)
    {
        return m_serialDataSource->read(buffer, schema, rasterize, rasterMeta);
    }
    else */
    if (m_liveDataSource)
    {
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

