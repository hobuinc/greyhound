#include <pdal/PipelineReader.hpp>
#include <thread>

#include "pdal-session.hpp"

PdalSession::PdalSession()
    : m_pipelineManager()
    , m_schema()
    , m_pointBuffer()
    , m_parsed(false)
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

        try
        {
            m_schema = packSchema(*m_pipelineManager.schema());

            m_schema.getDimension("X");
            m_schema.getDimension("Y");
            m_schema.getDimension("Z");
        }
        catch (pdal::dimension_not_found&)
        {
            throw std::runtime_error(
                    "Pipeline output should contain X, Y and Z dimensions");
        }
    }
}

std::size_t PdalSession::getNumPoints() const
{
    return m_pointBuffer->size();
}

std::string PdalSession::getSchema() const
{
    return pdal::Schema::to_xml(m_schema);
}

std::size_t PdalSession::getStride() const
{
    return m_schema.getByteSize();
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
    *buffer = 0;
    if (start >= getNumPoints())
        throw std::runtime_error("Invalid starting offset in 'read'");

    // If zero points specified, read all points after 'start'.
    const std::size_t pointsToRead(
            count > 0 ?
                std::min<std::size_t>(count, getNumPoints() - start) :
                getNumPoints() - start);

    try
    {
        const pdal::Schema* fullSchema(m_pipelineManager.schema());
        const pdal::schema::index_by_index& idx(
                fullSchema->getDimensions().get<pdal::schema::index>());

        *buffer = new unsigned char[m_schema.getByteSize() * pointsToRead];

        boost::uint8_t* pos(static_cast<boost::uint8_t*>(*buffer));

        for (boost::uint32_t i(start); i < start + pointsToRead; ++i)
        {
            for (boost::uint32_t d = 0; d < idx.size(); ++d)
            {
                if (!idx[d].isIgnored())
                {
                    m_pointBuffer->context().rawPtBuf()->getField(
                            idx[d],
                            i,
                            pos);

                    pos += idx[d].getByteSize();
                }
            }
        }
    }
    catch (...)
    {
        throw std::runtime_error("Failed to read points from PDAL");
    }

    return pointsToRead;
}

pdal::Schema PdalSession::packSchema(const pdal::Schema& fullSchema)
{
    pdal::Schema packedSchema;

    const pdal::schema::index_by_index& idx(
            fullSchema.getDimensions().get<pdal::schema::index>());

    for (boost::uint32_t d = 0; d < idx.size(); ++d)
    {
        if (!idx[d].isIgnored())
        {
            packedSchema.appendDimension(idx[d]);
        }
    }

    return packedSchema;
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

