#include <iostream>

#include <pdal/PointBuffer.hpp>

#include "pdal-session.hpp"
#include "buffer-pool.hpp"
#include "read-query.hpp"

PdalSession::PdalSession()
    : m_pipelineId()
    , m_filename()
    , m_serialCompress()
    , m_liveDataSource()
    , m_serialDataSource()
    , m_initOnce()
    , m_awakenMutex()
{ }

void PdalSession::initialize(
        const std::string& pipelineId,
        const std::string& filename,
        const bool serialCompress,
        const SerialPaths& serialPaths)
{
    m_initOnce.ensure([
            this,
            &pipelineId,
            &filename,
            serialCompress,
            &serialPaths]()
    {
        m_pipelineId = pipelineId;
        m_filename = filename;
        m_serialCompress = serialCompress;
        std::cout << "Initializing " << pipelineId << " - " << filename <<
            std::endl;

        // Try to awaken from serialized source.  If unsuccessful, initialize a
        // live source.
        if (!awaken(serialPaths) && !m_liveDataSource)
        {
            m_liveDataSource.reset(new LiveDataSource(pipelineId, filename));
            std::cout << "Created live source " << pipelineId << std::endl;
        }
    });
}

std::size_t PdalSession::getNumPoints()
{
    if (m_initOnce.await()) throw std::runtime_error("Not initialized!");

    if (m_serialDataSource)     return m_serialDataSource->getNumPoints();
    else if (m_liveDataSource)  return m_liveDataSource->getNumPoints();
    else throw std::runtime_error("Not initialized!");
}

std::string PdalSession::getSchema()
{
    if (m_initOnce.await()) throw std::runtime_error("Not initialized!");

    if (m_serialDataSource)     return m_serialDataSource->getSchema();
    else if (m_liveDataSource)  return m_liveDataSource->getSchema();
    else throw std::runtime_error("Not initialized!");
}

std::string PdalSession::getStats()
{
    if (m_initOnce.await()) throw std::runtime_error("Not initialized!");

    if (m_serialDataSource)     return m_serialDataSource->getStats();
    else if (m_liveDataSource)  return m_liveDataSource->getStats();
    else throw std::runtime_error("Not initialized!");
}

std::string PdalSession::getSrs()
{
    if (m_initOnce.await()) throw std::runtime_error("Not initialized!");

    if (m_serialDataSource)     return m_serialDataSource->getSrs();
    else if (m_liveDataSource)  return m_liveDataSource->getSrs();
    else throw std::runtime_error("Not initialized!");
}

std::vector<std::size_t> PdalSession::getFills()
{
    if (m_initOnce.await()) throw std::runtime_error("Not initialized!");

    if (m_serialDataSource)     return m_serialDataSource->getFills();
    else if (m_liveDataSource)  return m_liveDataSource->getFills();
    else throw std::runtime_error("Not initialized!");
}

void PdalSession::serialize(const SerialPaths& serialPaths)
{
    if (m_initOnce.await()) throw std::runtime_error("Not initialized!");

    if (m_liveDataSource && !m_serialDataSource)
    {
        m_liveDataSource->serialize(m_serialCompress, serialPaths);
        std::cout << "Serialized - awakening " << m_pipelineId << std::endl;
        awaken(serialPaths);

        // TODO Safely clear the quadIndex from the live data source.  All its
        // pending READs must complete first - new ones (except for the custom
        // raster) will already be routed to the serial data source.
    }
    else if (m_serialDataSource)
    {
        std::cout << "Already serialized." << std::endl;
    }
}

bool PdalSession::awaken(const SerialPaths& serialPaths)
{
    bool awoken(false);

    std::lock_guard<std::mutex> lock(m_awakenMutex);
    if (!m_serialDataSource)
    {
        GreyReader* reader(
                GreyReaderFactory::create(
                    m_pipelineId,
                    serialPaths));

        if (reader)
        {
            m_serialDataSource.reset(reader);
            awoken = true;
        }
    }
    else
    {
        awoken = true;
    }

    return awoken;
}

std::shared_ptr<QueryData> PdalSession::queryUnindexed(
        const Schema& schema,
        bool compress,
        std::size_t start,
        std::size_t count)
{
    if (m_initOnce.await()) throw std::runtime_error("Not initialized!");

    std::cout << "Unindexed read - query" << std::endl;
    // Unindexed read queries are only supported by a live data source.
    if (!m_liveDataSource)
    {
        std::cout << "Resetting live data source" << std::endl;
        m_liveDataSource.reset(new LiveDataSource(m_pipelineId, m_filename));
    }

    return std::shared_ptr<QueryData>(new UnindexedQueryData(
                schema,
                compress,
                m_liveDataSource->pointBuffer(),
                start,
                count));
}

std::shared_ptr<QueryData> PdalSession::query(
        const Schema& schema,
        bool compress,
        double xMin,
        double yMin,
        double xMax,
        double yMax,
        std::size_t depthBegin,
        std::size_t depthEnd)
{
    if (m_initOnce.await()) throw std::runtime_error("Not initialized!");

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

        return std::shared_ptr<QueryData>(
                new SerialQueryData(
                    schema,
                    compress,
                    false,
                    m_serialDataSource->query(
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

        return std::shared_ptr<QueryData>(
                new LiveQueryData(
                    schema,
                    compress,
                    false,
                    m_liveDataSource->pointBuffer(),
                    m_liveDataSource->query(
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

std::shared_ptr<QueryData> PdalSession::query(
        const Schema& schema,
        bool compress,
        std::size_t depthBegin,
        std::size_t depthEnd)
{
    if (m_initOnce.await()) throw std::runtime_error("Not initialized!");

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

        return std::shared_ptr<QueryData>(
                new SerialQueryData(
                    schema,
                    compress,
                    false,
                    m_serialDataSource->query(
                        depthBegin,
                        depthEnd)));
    }
    else if (m_liveDataSource)
    {
        std::cout <<
            "Reading from live source\n" <<
            logParams.str() <<
            std::endl;

        return std::shared_ptr<QueryData>(
                new LiveQueryData(
                    schema,
                    compress,
                    false,
                    m_liveDataSource->pointBuffer(),
                    m_liveDataSource->query(
                        depthBegin,
                        depthEnd)));
    }
    else
    {
        throw std::runtime_error("Not initialized!");
    }
}

std::shared_ptr<QueryData> PdalSession::query(
        const Schema& schema,
        bool compress,
        std::size_t rasterize,
        RasterMeta& rasterMeta)
{
    if (m_initOnce.await()) throw std::runtime_error("Not initialized!");

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

        return std::shared_ptr<QueryData>(
                new SerialQueryData(
                    schema,
                    compress,
                    true,
                    m_serialDataSource->query(
                        rasterize,
                        rasterMeta)));
    }
    else if (m_liveDataSource)
    {
        std::cout <<
            "Reading from live source\n" <<
            logParams.str() <<
            std::endl;

        return std::shared_ptr<QueryData>(
                new LiveQueryData(
                    schema,
                    compress,
                    true,
                    m_liveDataSource->pointBuffer(),
                    m_liveDataSource->query(
                        rasterize,
                        rasterMeta)));
    }
    else
    {
        throw std::runtime_error("not initialized!");
    }
}

std::shared_ptr<QueryData> PdalSession::query(
        const Schema& schema,
        bool compress,
        const RasterMeta& rasterMeta)
{
    if (m_initOnce.await()) throw std::runtime_error("Not initialized!");

    // Custom-res raster queries are only supported by a live data source.
    if (!m_liveDataSource)
    {
        m_liveDataSource.reset(new LiveDataSource(m_pipelineId, m_filename));
    }

    if (!m_liveDataSource)
        throw std::runtime_error("not initialized!");

    return std::shared_ptr<QueryData>(
            new LiveQueryData(
                schema,
                compress,
                true,
                m_liveDataSource->pointBuffer(),
                m_liveDataSource->query(rasterMeta)));
}

std::shared_ptr<QueryData> PdalSession::query(
        const Schema& schema,
        bool compress,
        bool is3d,
        double radius,
        double x,
        double y,
        double z)
{
    if (m_initOnce.await()) throw std::runtime_error("Not initialized!");

    // KD-indexed read queries are only supported by a live data source.
    if (!m_liveDataSource)
    {
        m_liveDataSource.reset(new LiveDataSource(m_pipelineId, m_filename));
    }

    if (!m_liveDataSource)
        throw std::runtime_error("not initialized!");

    return std::shared_ptr<QueryData>(
            new LiveQueryData(
                schema,
                compress,
                false,
                m_liveDataSource->pointBuffer(),
                m_liveDataSource->query(is3d, radius, x, y, z)));
}

