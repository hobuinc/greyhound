#pragma once

#include <boost/asio.hpp>

#include <pdal/PipelineManager.hpp>
#include <pdal/Schema.hpp>

class PdalSession
{
public:
    PdalSession();

    void initialize(const std::string& pipeline, bool execute = true);

    std::size_t getNumPoints() const;
    std::string getSchema() const;
    std::size_t getStride() const;

    std::size_t read(
            unsigned char** buffer,
            std::size_t start,
            std::size_t count);

private:
    pdal::PipelineManager m_pipelineManager;
    pdal::Schema m_schema;
    const pdal::PointBuffer* m_pointBuffer;
    bool m_parsed;

    pdal::Schema packSchema(const pdal::Schema& fullSchema);
};

class BufferTransmitter
{
public:
    BufferTransmitter(
            const std::string& host,
            int port,
            const unsigned char* data,
            std::size_t size);

    void transmit();

private:
    std::unique_ptr<boost::asio::ip::tcp::socket> m_socket;
    const unsigned char* const m_data;
    const std::size_t m_size;
};

