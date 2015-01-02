#include "buffer-pool.hpp"

ItcBuffer::ItcBuffer(std::size_t id, std::size_t capacity)
    : m_maxCapacity(capacity)
    , m_buffer()
    , m_id(id)
    , m_mutex()
    , m_cv()
    , m_available(true)
{ }

void ItcBuffer::grab()
{
    std::unique_lock<std::mutex> lock(m_mutex);
    m_cv.wait(lock, [this]()->bool { return this->m_available; });
    m_available = false;
    lock.unlock();
}

void ItcBuffer::flush()
{
    std::unique_lock<std::mutex> lock(m_mutex);
    m_available = true;
    lock.unlock();
    m_cv.notify_all();
}

std::size_t ItcBuffer::size() const
{
    return m_buffer.size();
}

void ItcBuffer::resize(std::size_t size)
{
    if (size > m_maxCapacity)
        throw std::runtime_error("Resize request over capacity");

    if (m_buffer.capacity() < m_maxCapacity)
    {
        m_buffer.reserve(m_maxCapacity);
    }

    m_buffer.resize(size);
}

uint8_t* ItcBuffer::data()
{
    return m_buffer.data();
}

void ItcBuffer::release()
{
    std::unique_lock<std::mutex> lock(m_mutex);
    m_cv.wait(lock, [this]()->bool { return this->m_available; });
    m_available = true;
    lock.unlock();
}

///////////////////////////////////////////////////////////////////////////////

ItcBufferPool::ItcBufferPool(std::size_t numBuffers, std::size_t capacity)
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
                    std::shared_ptr<ItcBuffer>(new ItcBuffer(i, capacity))));
    }
}

std::shared_ptr<ItcBuffer> ItcBufferPool::acquire()
{
    std::unique_lock<std::mutex> lock(m_mutex);
    m_cv.wait(lock, [this]()->bool { return m_buffers.size(); });
    std::shared_ptr<ItcBuffer> itcBuffer(m_buffers[m_available.back()]);
    m_available.pop_back();
    lock.unlock();
    return itcBuffer;
}

void ItcBufferPool::release(std::shared_ptr<ItcBuffer> buffer)
{
    std::unique_lock<std::mutex> lock(m_mutex);
    buffer->release();
    m_available.push_back(buffer->id());
    lock.unlock();
    m_cv.notify_one();
}

