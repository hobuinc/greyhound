#pragma once

#include <boost/asio.hpp>

#include <pdal/PipelineManager.hpp>
#include <pdal/Schema.hpp>
#include <pdal/KDIndex.hpp>

class PdalSession
{
public:
    PdalSession();

    void initialize(const std::string& pipeline, bool execute = true);
    void indexData(bool build3d = false);

    std::size_t getNumPoints() const;
    std::string getSchema() const;
    std::size_t getStride() const;
    std::string getSrs() const;

    // Read un-indexed data with and offset and a count.
    std::size_t read(
            unsigned char** buffer,
            std::size_t start,
            std::size_t count);

    // Perform KD-indexed query of point + radius.
    std::size_t read(
            unsigned char** buffer,
            bool is3d,
            double radius,
            double x,
            double y,
            double z);
private:
    pdal::PipelineManager m_pipelineManager;
    pdal::Schema m_schema;
    pdal::PointBufferPtr m_pointBuffer;
    bool m_parsed;
    bool m_initialized;

    std::unique_ptr<pdal::KDIndex> m_kdIndex2d;
    std::unique_ptr<pdal::KDIndex> m_kdIndex3d;

    pdal::Schema packSchema(const pdal::Schema& fullSchema);
    bool indexed(bool is3d) const
    {
        return is3d ? m_kdIndex3d.get() : m_kdIndex2d.get();
    }
};

class BufferTransmitter
{
public:
    BufferTransmitter(
            const std::string& host,
            int port,
            const unsigned char* data,
            std::size_t size);

    void transmit(std::size_t offset = 0, std::size_t bytes = 0);

private:
    std::unique_ptr<boost::asio::ip::tcp::socket> m_socket;
    const unsigned char* const m_data;
    const std::size_t m_size;
};

