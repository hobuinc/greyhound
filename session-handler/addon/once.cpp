#include "once.hpp"

Once::Once()
    : m_done(false)
    , m_err(false)
    , m_mutex()
{ }

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

bool Once::done() const {
    return m_done;
}

bool Once::err() const {
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
}

