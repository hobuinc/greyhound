#include "once.hpp"

Once::Once(std::function<void()> destruct)
    : m_done(false)
    , m_err(false)
    , m_mutex()
    , m_destruct(destruct)
{ }

Once::~Once()
{
    if (m_done && !m_err)
    {
        m_destruct();
    }
}

void Once::ensure(std::function<void()> function)
{
    if (!done())
    {
        lock();

        if (err())
        {
            unlock(true);
            throw std::runtime_error(
                    "Could not ensure function - previous error");
        }

        if (!done())
        {
            try
            {
                function();
            }
            catch(std::runtime_error& e)
            {
                unlock(true);
                throw e;
            }
            catch(...)
            {
                unlock(true);
                throw std::runtime_error(
                        "Could not ensure function - unknown error");
            }
        }

        unlock();
    }
}

bool Once::await()
{
    if (!done())
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_cv.wait(lock, [this]()->bool { return done(); });
    }
    return m_err;
}

bool Once::done() const
{
    return m_done;
}

bool Once::err() const
{
    return m_err;
}

void Once::lock()
{
    m_mutex.lock();
}

void Once::unlock(bool err)
{
    m_done = !err;
    m_err = err;

    m_mutex.unlock();
    m_cv.notify_all();
}

