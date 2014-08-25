#pragma once

#include <boost/asio.hpp>

#include <pdal/PipelineManager.hpp>
#include <pdal/KDIndex.hpp>
#include <pdal/QuadIndex.hpp>

class DimInfo;
class Schema;

class PdalSession
{
public:
    PdalSession();

    void initialize(const std::string& pipeline, bool execute = true);
    void indexData(bool build3d = false);

    std::size_t getNumPoints() const;
    std::string getDimensions() const;
    std::string getSrs() const;

    // Read un-indexed data with an offset and a count.
    std::size_t read(
            std::vector<unsigned char>& buffer,
            const Schema& schema,
            std::size_t start,
            std::size_t count);

    // Read quad-tree indexed data with a bounding box query and min/max tree
    // depths to search.
    std::size_t read(
            std::vector<unsigned char>& buffer,
            const Schema& schema,
            double xMin,
            double yMin,
            double xMax,
            double yMax,
            std::size_t depthBegin,
            std::size_t depthEnd);

    // Perform KD-indexed query of point + radius.
    std::size_t read(
            std::vector<unsigned char>& buffer,
            const Schema& schema,
            bool is3d,
            double radius,
            double x,
            double y,
            double z);

    const pdal::PointBuffer& pointBuffer()
    {
        return *m_pointBuffer.get();
    }

private:
    pdal::PipelineManager m_pipelineManager;
    pdal::PointBufferPtr m_pointBuffer;
    bool m_parsed;
    bool m_initialized;

    std::unique_ptr<pdal::QuadIndex> m_quadIndex;

    std::unique_ptr<pdal::KDIndex> m_kdIndex2d;
    std::unique_ptr<pdal::KDIndex> m_kdIndex3d;

    bool indexed(bool is3d) const
    {
        return is3d ? m_kdIndex3d.get() : m_kdIndex2d.get();
    }

    // Read points out from a list that represents indices into m_pointBuffer.
    std::size_t readIndexList(
            std::vector<unsigned char>& buffer,
            const Schema& schema,
            const std::vector<std::size_t>& indexList);

    // Returns number of bytes read into buffer.
    std::size_t readDim(
            unsigned char* buffer,
            const DimInfo& dimReq,
            std::size_t index) const;
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

