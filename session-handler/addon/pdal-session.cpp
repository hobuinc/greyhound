#include <thread>

#include <pdal/PipelineReader.hpp>
#include <pdal/PointContext.hpp>

#include "pdal-session.hpp"

namespace
{
    // TODO Client should specify these in their read request.
    const pdal::Dimension::IdList dims(
        {
            pdal::Dimension::Id::X,
            pdal::Dimension::Id::Y,
            pdal::Dimension::Id::Z,
            pdal::Dimension::Id::Intensity,
            pdal::Dimension::Id::Red,
            pdal::Dimension::Id::Green,
            pdal::Dimension::Id::Blue
        }
    );
}

PdalSession::PdalSession()
    : m_pipelineManager()
    , m_pointBuffer()
    , m_parsed(false)
    , m_initialized(false)
    , m_quadIndex()
    , m_kdIndex2d()
    , m_kdIndex3d()
{ }

void PdalSession::initialize(const std::string& pipeline, const bool execute)
{
    if (!m_parsed)
    {
        // Set this before doing the actual parsing, which may throw.  If we
        // fail mid-parse, don't want to allow re-parsing on top of a
        // possibly partially initialized pipeline.
        m_parsed = true;

        std::istringstream ssPipeline(pipeline);
        pdal::PipelineReader pipelineReader(m_pipelineManager);
        pipelineReader.readPipeline(ssPipeline);
    }
    else
    {
        throw std::runtime_error("Reinitialization not allowed");
    }

    if (execute)
    {
        m_pipelineManager.execute();
        const pdal::PointBufferSet& pbSet(m_pipelineManager.buffers());
        m_pointBuffer = *pbSet.begin();

        if (!m_pointBuffer->context().hasDim(pdal::Dimension::Id::X) ||
            !m_pointBuffer->context().hasDim(pdal::Dimension::Id::Y) ||
            !m_pointBuffer->context().hasDim(pdal::Dimension::Id::Z))
        {
            throw std::runtime_error(
                    "Pipeline output should contain X, Y and Z dimensions");
        }
        else
        {
            m_initialized = true;
        }
    }
}

void PdalSession::indexData(const bool build3d)
{
    if (build3d)
    {
        m_kdIndex3d.reset(new pdal::KDIndex(*m_pointBuffer.get()));
        m_kdIndex3d->build(m_pointBuffer->context(), build3d);
    }
    else
    {
        m_kdIndex2d.reset(new pdal::KDIndex(*m_pointBuffer.get()));
        m_kdIndex2d->build(m_pointBuffer->context(), build3d);
    }
}

std::size_t PdalSession::getNumPoints() const
{
    return m_pointBuffer->size();
}

std::string PdalSession::getDimensions() const
{
    return m_pointBuffer->context().dimsJson();
}

std::size_t PdalSession::getStride() const
{
    std::size_t stride(0);

    for (const auto& dim : dims)
    {
        stride += pdal::Dimension::size(
                pdal::Dimension::defaultType(dim));
    }

    return stride;
}

std::string PdalSession::getSrs() const
{
    return m_pointBuffer->context().spatialRef().getRawWKT();
}

std::size_t PdalSession::read(
        unsigned char** buffer,
        const std::size_t start,
        const std::size_t count)
{
    if (start >= getNumPoints())
        throw std::runtime_error("Invalid starting offset in 'read'");

    // If zero points specified, read all points after 'start'.
    const std::size_t pointsToRead(
            count > 0 ?
                std::min<std::size_t>(count, getNumPoints() - start) :
                getNumPoints() - start);

    try
    {
        unsigned char* pos(*buffer);

        for (boost::uint32_t i(start); i < start + pointsToRead; ++i)
        {
            for (const auto& dim : dims)
            {
                // TODO Type/size should come from client request.
                m_pointBuffer->getRawField(dim, i, pos);

                pos += pdal::Dimension::size(
                        pdal::Dimension::defaultType(dim));

            }
        }
    }
    catch (...)
    {
        throw std::runtime_error("Failed to read points from PDAL");
    }

    return pointsToRead;
}

std::size_t PdalSession::read(
        unsigned char** buffer,
        const double xMin,
        const double yMin,
        const double xMax,
        const double yMax,
        const std::size_t depthBegin,
        const std::size_t depthEnd)
{
    if (!m_quadIndex)
    {
        m_quadIndex.reset(new pdal::QuadIndex(*m_pointBuffer.get()));

        try
        {
            m_quadIndex->build();
        }
        catch (...)
        {
            throw std::runtime_error("Error creating quadtree index");
        }
    }

    double x0, y0, x1, y1;
    m_quadIndex->getBounds(x0, y0, x1, y1);
    std::cout <<
        x0 <<
        ", " <<
        y0 <<
        ", " <<
        x1 <<
        ", " <<
        y1 << std::endl;

    const std::vector<std::size_t> results(m_quadIndex->getPoints(
            xMin,
            yMin,
            xMax,
            yMax,
            depthBegin,
            depthEnd));

    std::size_t pointsRead = readIndexList(buffer, results);
    m_quadIndex.reset(0);
    return pointsRead;
}

std::size_t PdalSession::read(
        unsigned char** buffer,
        const bool is3d,
        const double radius,
        const double x,
        const double y,
        const double z)
{
    if (!indexed(is3d))
    {
        indexData(is3d);
    }

    pdal::KDIndex* activeKdIndex(
            is3d ? m_kdIndex3d.get() : m_kdIndex2d.get());

    // KDIndex::radius() takes r^2.
    const std::vector<std::size_t> results(
            activeKdIndex->radius(x, y, z, radius * radius));

    return readIndexList(buffer, results);
}

std::size_t PdalSession::readIndexList(
        unsigned char** buffer,
        const std::vector<std::size_t>& indexList)
{

    const std::size_t pointsToRead(indexList.size());

    try
    {
        unsigned char* pos(*buffer);

        for (std::size_t i : indexList)
        {
            for (const auto& dim : dims)
            {
                // TODO Type/size should come from client request.
                m_pointBuffer->getRawField(dim, i, pos);

                pos += pdal::Dimension::size(
                        pdal::Dimension::defaultType(dim));
            }
        }
    }
    catch (...)
    {
        throw std::runtime_error("Failed to read points from PDAL");
    }

    return pointsToRead;
}

//////////////////////////////////////////////////////////////////////////////

BufferTransmitter::BufferTransmitter(
        const std::string& host,
        const int port,
        const unsigned char* data,
        const std::size_t size)
    : m_socket()
    , m_data(data)
    , m_size(size)
{
    namespace asio = boost::asio;
    using boost::asio::ip::tcp;

    std::stringstream portStream;
    portStream << port;

    asio::io_service service;
    tcp::resolver resolver(service);

    tcp::resolver::query q(host, portStream.str());
    tcp::resolver::iterator iter = resolver.resolve(q), end;

    m_socket.reset(new tcp::socket(service));

    int retryCount = 0;
    boost::system::error_code ignored_error;

    // Don't fail yet, the setup service may be setting up the receiver.
    tcp::resolver::iterator connectIter;

    while (
        (connectIter = asio::connect(*m_socket, iter, ignored_error)) == end &&
            retryCount++ < 500)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    if (connectIter == end)
    {
        std::stringstream errStream;
        errStream << "Could not connect to " << host << ":" << port;
        throw std::runtime_error(errStream.str());
    }
}

void BufferTransmitter::transmit(
        const std::size_t offset,
        const std::size_t bytes)
{
    boost::system::error_code ignored_error;

    boost::asio::write(
            *m_socket,
            boost::asio::buffer(
                m_data + offset,
                bytes ?
                    std::min(bytes, m_size - offset) :
                    m_size - offset),
            ignored_error);
}

