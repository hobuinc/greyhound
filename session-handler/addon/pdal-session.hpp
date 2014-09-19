#pragma once

#include <mutex>

#include <boost/asio.hpp>

#include <pdal/KDIndex.hpp>
#include <pdal/QuadIndex.hpp>
#include <pdal/PointContext.hpp>
#include <pdal/PipelineManager.hpp>

class DimInfo;
class Schema;
class PdalIndex;

// The Once class allows concurrent users of a shared session to avoid
// duplicating work, while hiding the shared aspect of the session from callers.
class Once
{
public:
    Once() : m_done(false), m_mutex() { }

    bool done() const { return m_done; }

    // This function ensures that the function parameter is executed only one
    // time, even with multithreaded callers.  It also ensures that subsequent
    // callers will block until the work is completed if they call while the
    // work is in progress.
    //
    // After execution is complete, no additional blocking or work will be
    // performed.
    void ensure(std::function<void ()> function)
    {
        if (!done())
        {
            lock();

            if (!done())
            {
                try
                {
                    function();
                }
                catch(std::runtime_error& e)
                {
                    unlock();
                    throw e;
                }
                catch(...)
                {
                    unlock();
                    throw std::runtime_error("Error in ensure function");
                }
            }

            unlock();
        }
    }

private:
    void lock()     { m_mutex.lock(); }
    void unlock()   { m_done = true; m_mutex.unlock(); } // Must be this order!

    bool m_done;
    std::mutex m_mutex;
};

struct RasterMeta
{
    double xBegin;
    double xEnd;
    double xStep;
    double yBegin;
    double yEnd;
    double yStep;

    std::size_t xNum() const { return std::round((xEnd - xBegin) / xStep); }
    std::size_t yNum() const { return std::round((yEnd - yBegin) / yStep); }
};

class PdalSession
{
public:
    PdalSession();

    void initialize(const std::string& pipeline, bool execute = true);
    void indexData(bool build3d = false);

    std::size_t getNumPoints() const;
    std::string getSchema() const;
    std::string getStats() const;
    std::string getSrs() const;

    // Read un-indexed data with an offset and a count.
    std::size_t readUnindexed(
            std::vector<unsigned char>& buffer,
            const Schema& schema,
            std::size_t start,
            std::size_t count) const;

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

    // Read quad-tree indexed data with a bounding box query and depth level
    // for rasterization.
    // TODO
    /*
    std::size_t read(
            std::vector<unsigned char>& buffer,
            const Schema& schema,
            double xMin,
            double yMin,
            double xMax,
            double yMax,
            std::size_t rasterize);
    */

    // Read quad-tree indexed data with min/max tree depths to search.
    std::size_t read(
            std::vector<unsigned char>& buffer,
            const Schema& schema,
            std::size_t depthBegin,
            std::size_t depthEnd);

    // Read quad-tree indexed data with depth level for rasterization.
    std::size_t read(
            std::vector<unsigned char>& buffer,
            const Schema& schema,
            std::size_t rasterize,
            RasterMeta& rasterMeta);

    // Perform KD-indexed query of point + radius.
    std::size_t read(
            std::vector<unsigned char>& buffer,
            const Schema& schema,
            bool is3d,
            double radius,
            double x,
            double y,
            double z);

    const pdal::PointBuffer& pointBuffer() const
    {
        return *m_pointBuffer.get();
    }

    const pdal::PointContext& pointContext() const
    {
        return m_pointContext;
    }

private:
    pdal::PipelineManager m_pipelineManager;
    pdal::PointBufferPtr m_pointBuffer;
    pdal::PointContext m_pointContext;

    Once m_initOnce;

    std::unique_ptr<PdalIndex> m_pdalIndex;

    // Read points out from a list that represents indices into m_pointBuffer.
    std::size_t readIndexList(
            std::vector<unsigned char>& buffer,
            const Schema& schema,
            const std::vector<std::size_t>& indexList,
            bool rasterize = false) const;

    // Returns number of bytes read into buffer.
    std::size_t readDim(
            unsigned char* buffer,
            const DimInfo& dimReq,
            std::size_t index) const;

    // Disallow copy/assignment.
    PdalSession(const PdalSession&);
    PdalSession& operator=(const PdalSession&);
};

class PdalIndex
{
public:
    PdalIndex()
        : m_kdIndex2d()
        , m_kdIndex3d()
        , m_quadIndex()
        , m_kd2dOnce()
        , m_kd3dOnce()
        , m_quadOnce()
    { }

    enum IndexType
    {
        KdIndex2d,
        KdIndex3d,
        QuadIndex
    };

    void ensureIndex(
            IndexType indexType,
            const pdal::PointBufferPtr pointBufferPtr);

    const pdal::KDIndex& kdIndex2d()    { return *m_kdIndex2d.get(); }
    const pdal::KDIndex& kdIndex3d()    { return *m_kdIndex3d.get(); }
    const pdal::QuadIndex& quadIndex()  { return *m_quadIndex.get(); }

private:
    std::unique_ptr<pdal::KDIndex> m_kdIndex2d;
    std::unique_ptr<pdal::KDIndex> m_kdIndex3d;
    std::unique_ptr<pdal::QuadIndex> m_quadIndex;

    Once m_kd2dOnce;
    Once m_kd3dOnce;
    Once m_quadOnce;

    // Disallow copy/assignment.
    PdalIndex(const PdalIndex&);
    PdalIndex& operator=(const PdalIndex&);
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

    BufferTransmitter(const BufferTransmitter&);
    BufferTransmitter& operator=(const BufferTransmitter&);
};

