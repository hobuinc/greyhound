#include <thread>

#include <pdal/PipelineReader.hpp>
#include <pdal/PointContext.hpp>

#include "pdal-session.hpp"
#include "read-command.hpp"

PdalSession::PdalSession()
    : m_pipelineManager()
    , m_pointBuffer()
    , m_initOnce()
    , m_pdalIndex(new PdalIndex())
{ }

void PdalSession::initialize(const std::string& pipeline, const bool execute)
{

    m_initOnce.ensure([this, &pipeline, execute]() {
        std::istringstream ssPipeline(pipeline);
        pdal::PipelineReader pipelineReader(m_pipelineManager);
        pipelineReader.readPipeline(ssPipeline);

        // This block could take a substantial amount of time.  The
        // PdalBindings wrapper ensures that it will run in a non-blocking
        // manner in the uv_work_queue.
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
        }
    });
}

std::size_t PdalSession::getNumPoints() const
{
    return m_pointBuffer->size();
}

std::string PdalSession::getDimensions() const
{
    return m_pointBuffer->context().dimsJson();
}

std::string PdalSession::getSrs() const
{
    return m_pointBuffer->context().spatialRef().getRawWKT();
}

std::size_t PdalSession::readDim(
        unsigned char* buffer,
        const DimInfo& dim,
        std::size_t index) const
{
    if (dim.type == "floating")
    {
        if (dim.size == 4)
        {
            float val(m_pointBuffer->getFieldAs<float>(dim.id, index));
            std::memcpy(buffer, &val, dim.size);
        }
        else if (dim.size == 8)
        {
            double val(m_pointBuffer->getFieldAs<double>(dim.id, index));
            std::memcpy(buffer, &val, dim.size);
        }
        else
        {
            throw std::runtime_error("Invalid floating size requested");
        }
    }
    else
    {
        if (dim.size == 1)
        {
            uint8_t val(m_pointBuffer->getFieldAs<uint8_t>(dim.id, index));
            std::memcpy(buffer, &val, dim.size);
        }
        else if (dim.size == 2)
        {
            uint16_t val(m_pointBuffer->getFieldAs<uint16_t>(dim.id, index));
            std::memcpy(buffer, &val, dim.size);
        }
        else if (dim.size == 4)
        {
            uint32_t val(m_pointBuffer->getFieldAs<uint32_t>(dim.id, index));
            std::memcpy(buffer, &val, dim.size);
        }
        else if (dim.size == 8)
        {
            uint64_t val(m_pointBuffer->getFieldAs<uint64_t>(dim.id, index));
            std::memcpy(buffer, &val, dim.size);
        }
        else
        {
            throw std::runtime_error("Invalid integer size requested");
        }
    }

    return dim.size;
}

std::size_t PdalSession::read(
        std::vector<unsigned char>& buffer,
        const Schema& schema,
        const std::size_t start,
        const std::size_t count) const
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
        buffer.resize(pointsToRead * schema.stride());

        unsigned char* pos(buffer.data());

        for (boost::uint32_t i(start); i < start + pointsToRead; ++i)
        {
            for (const auto& dim : schema.dims)
            {
                pos += readDim(pos, dim, i);
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
        std::vector<unsigned char>& buffer,
        const Schema& schema,
        const double xMin,
        const double yMin,
        const double xMax,
        const double yMax,
        const std::size_t depthBegin,
        const std::size_t depthEnd)
{
    m_pdalIndex->ensureIndex(PdalIndex::QuadIndex, m_pointBuffer);
    const pdal::QuadIndex& quadIndex(m_pdalIndex->quadIndex());

    const std::vector<std::size_t> results(quadIndex.getPoints(
            xMin,
            yMin,
            xMax,
            yMax,
            depthBegin,
            depthEnd));

    std::size_t pointsRead = readIndexList(buffer, schema, results);
    return pointsRead;
}

std::size_t PdalSession::read(
        std::vector<unsigned char>& buffer,
        const Schema& schema,
        const bool is3d,
        const double radius,
        const double x,
        const double y,
        const double z)
{
    m_pdalIndex->ensureIndex(
            is3d ? PdalIndex::KdIndex3d : PdalIndex::KdIndex2d,
            m_pointBuffer);

    const pdal::KDIndex& kdIndex(
            is3d ? m_pdalIndex->kdIndex3d() : m_pdalIndex->kdIndex2d());

    // KDIndex::radius() takes r^2.
    const std::vector<std::size_t> results(
            kdIndex.radius(x, y, z, radius * radius));

    return readIndexList(buffer, schema, results);
}

std::size_t PdalSession::readIndexList(
        std::vector<unsigned char>& buffer,
        const Schema& schema,
        const std::vector<std::size_t>& indexList) const
{
    const std::size_t pointsToRead(indexList.size());

    try
    {
        buffer.resize(pointsToRead * schema.stride());

        unsigned char* pos(buffer.data());

        for (std::size_t i : indexList)
        {
            for (const auto& dim : schema.dims)
            {
                pos += readDim(pos, dim, i);
            }
        }
    }
    catch (...)
    {
        throw std::runtime_error("Failed to read points from PDAL");
    }

    return pointsToRead;
}

void PdalIndex::ensureIndex(
        const IndexType indexType,
        const pdal::PointBufferPtr pointBufferPtr)
{
    const pdal::PointBuffer& pointBuffer(*pointBufferPtr.get());

    switch (indexType)
    {
        case KdIndex2d:
            m_kd2dOnce.ensure([this, &pointBuffer]() {
                m_kdIndex2d.reset(new pdal::KDIndex(pointBuffer));
                m_kdIndex2d->build(pointBuffer.context(), false);
            });
            break;
        case KdIndex3d:
            m_kd3dOnce.ensure([this, &pointBuffer]() {
                m_kdIndex3d.reset(new pdal::KDIndex(pointBuffer));
                m_kdIndex3d->build(pointBuffer.context(), true);
            });
            break;
        case QuadIndex:
            m_quadOnce.ensure([this, &pointBuffer]() {
                m_quadIndex.reset(new pdal::QuadIndex(pointBuffer));
                m_quadIndex->build();
            });
            break;
        default:
            throw std::runtime_error("Invalid PDAL index type");
    }
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

