#pragma once

#include <map>
#include <mutex>
#include <vector>
#include <memory>
#include <cstring>
#include <condition_variable>

class ItcBufferPool;

class ItcBuffer
{
    // Don't let anyone besides the ItcBufferPool create or release these.
    friend class ItcBufferPool;

public:
    // This class does not support data races where multiple threads may be
    // trying to grab this buffer.  Supports a well-defined hand-off/receive
    // where a thread hands off to another thread, which must call flush()
    // before the original thread may continue work.

    // Acquire the buffer, after which point its data may be altered.  After
    // the data is set, this buffer may be sent to a different thread for
    // processing.
    void grab();

    // A worker thread uses this to signal that the processing is complete,
    // and another thread may grab() and reuse this buffer.
    void flush();

    // Append vector onto this buffer.  If current size plus the size of the
    // incoming bytes is greater than the max capacity, std::runtime_error is
    // thrown.
    std::size_t push(const char* data, std::size_t size);

    std::size_t size() const;
    void resize(std::size_t);

    const std::vector<char>& vecRef() const { return m_buffer; }
    char* data();

private:
    ItcBuffer(std::size_t id, std::size_t capacity);

    // Release this buffer back into the buffer pool.
    void release();

    std::size_t id() const { return m_id; }

    const std::size_t m_maxCapacity;
    std::vector<char> m_buffer;
    const std::size_t m_id;

    std::mutex m_mutex;
    std::condition_variable m_cv;
    bool m_available;
};

class ItcBufferPool
{
public:
    ItcBufferPool(std::size_t numBuffers, std::size_t capacity);

    std::shared_ptr<ItcBuffer> acquire();
    void release(std::shared_ptr<ItcBuffer> buffer);

private:
    // If there are no buffers available, wait for one.  Otherwise return.
    // m_mutex must be locked by the caller.
    void await();

    std::vector<std::size_t> m_available;
    std::map<std::size_t, std::shared_ptr<ItcBuffer>> m_buffers;

    std::mutex m_mutex;
    std::condition_variable m_cv;
};

