#pragma once

#include <mutex>

// The Once class allows concurrent users of a shared session to avoid
// duplicating work, while hiding the shared aspect of the session from callers.
class Once
{
public:
    Once(std::function<void()> destruct = []()->void { });
    ~Once();

    // This function ensures that the function parameter is executed only one
    // time, even with multithreaded callers.  It also ensures that subsequent
    // callers will block until the work is completed if they call while the
    // work is in progress.
    //
    // After execution is complete, no additional blocking or work will be
    // performed.
    void ensure(std::function<void()> function);

    bool done() const;
    bool err() const;

private:
    void lock();
    void unlock(bool err = false);

    bool m_done;
    bool m_err;
    std::mutex m_mutex;

    std::function<void()> m_destruct;
};

