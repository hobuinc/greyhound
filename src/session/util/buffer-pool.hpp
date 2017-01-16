#pragma once

#include <cassert>
#include <condition_variable>
#include <set>
#include <stack>
#include <mutex>
#include <vector>

class BufferPool
{
    using Data = std::vector<char>;
    static constexpr std::size_t reservation = 1024 * 512;

public:
    BufferPool(std::size_t count)
        : m_startingSize(count)
        , m_buffers(count)
    {
        for (auto& b : m_buffers)
        {
            b.reserve(reservation);
            m_available.push(&b);
        }
    }

    Data& acquire()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_available.empty())
        {
            const std::size_t initialSize(m_buffers.size());
            m_buffers.insert(m_buffers.end(), initialSize, Data());
            for (std::size_t i(initialSize); i < m_buffers.size(); ++i)
            {
                m_buffers[i].reserve(reservation);
                m_available.push(&m_buffers[i]);
            }
            std::cout << "Alloc to " << m_buffers.size() << std::endl;
        }

        Data& buffer(*m_available.top());
        m_available.pop();

        return buffer;
    }

    void release(Data& buffer)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_available.push(&buffer);

        if (buffer.capacity() > reservation)
        {
            buffer.resize(reservation);
            buffer.shrink_to_fit();
        }

        buffer.clear();

        const std::size_t total(m_buffers.size());
        if (total == m_available.size() && total > m_startingSize)
        {
            std::cout << "Reset buffer pool" << std::endl;
            reset();
        }
    }

    void capture(std::vector<char>& buffer)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_captures.emplace(buffer.data(), &buffer);
    }

    void release(char* pos)
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        Data& buffer(*m_captures.at(pos));
        m_captures.erase(pos);
        lock.unlock();

        release(buffer);
    }

    void reset()
    {
        m_buffers.resize(m_startingSize);
        while (m_available.size()) m_available.pop();

        for (auto& b : m_buffers) m_available.push(&b);
    }

private:
    const std::size_t m_startingSize;
    std::deque<Data> m_buffers;
    std::stack<Data*> m_available;
    std::map<const char*, Data*> m_captures;

    std::mutex m_mutex;
    std::condition_variable m_cv;
};

