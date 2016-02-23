#include "buffer-pool.hpp"

ItcBuffer::ItcBuffer(std::size_t id)
    : m_buffer()
    , m_id(id)
    , m_mutex()
    , m_cv()
    , m_available(true)
{ }

std::size_t ItcBuffer::push(const char* data, const std::size_t size)
{
    m_buffer.insert(m_buffer.end(), data, data + size);
    return size;
}

std::size_t ItcBuffer::size() const
{
    return m_buffer.size();
}

void ItcBuffer::resize(std::size_t size)
{
    m_buffer.resize(size);
}

char* ItcBuffer::data()
{
    return m_buffer.data();
}

///////////////////////////////////////////////////////////////////////////////

ItcBufferPool::ItcBufferPool(std::size_t numBuffers)
    : m_available(numBuffers)
    , m_buffers()
    , m_mutex()
    , m_cv()
{
    for (std::size_t i(0); i < numBuffers; ++i)
    {
        m_available[i] = i;
        m_buffers.insert(std::make_pair(
                    i,
                    std::shared_ptr<ItcBuffer>(new ItcBuffer(i))));
    }
}

std::shared_ptr<ItcBuffer> ItcBufferPool::acquire()
{
    std::unique_lock<std::mutex> lock(m_mutex);
    m_cv.wait(lock, [this]()->bool { return !m_available.empty(); });
    std::shared_ptr<ItcBuffer> itcBuffer(m_buffers[m_available.back()]);
    m_available.pop_back();
    lock.unlock();
    return itcBuffer;
}

void ItcBufferPool::release(std::shared_ptr<ItcBuffer> buffer)
{
    std::unique_lock<std::mutex> lock(m_mutex);
    m_available.push_back(buffer->id());
    lock.unlock();
    m_cv.notify_one();
}

