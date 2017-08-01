#pragma once

#include <chrono>
#include <condition_variable>
#include <map>
#include <mutex>
#include <string>

#include <entwine/reader/cache.hpp>
#include <entwine/types/outer-scope.hpp>

#include <greyhound/auth.hpp>
#include <greyhound/configuration.hpp>
#include <greyhound/defs.hpp>
#include <greyhound/resource.hpp>
#include <greyhound/timing.hpp>

namespace greyhound
{

class TimedResource
{
public:
    TimedResource(SharedResource resource);

    SharedResource& get() { return m_resource; }
    void touch();
    std::size_t since() const;

private:
    SharedResource m_resource;
    TimePoint m_touched;
};

class Manager
{
public:
    Manager(const Configuration& config);
    ~Manager();

    SharedResource get(std::string name, Req& req);

    entwine::Cache& cache() const { return m_cache; }
    entwine::OuterScope& outerScope() const { return m_outerScope; }
    const Paths& paths() const { return m_paths; }
    const Headers& headers() const { return m_headers; }

private:
    void sweep();

    mutable entwine::Cache m_cache;
    mutable entwine::OuterScope m_outerScope;

    Paths m_paths;
    Headers m_headers;

    std::map<std::string, TimedResource> m_resources;
    std::unique_ptr<Auth> m_auth;

    mutable std::mutex m_mutex;

    bool m_done = false;
    std::size_t m_timeoutSeconds = 0;
    TimePoint m_lastSweep;
    mutable std::condition_variable m_cv;
    std::thread m_sweepThread;
};

} // namespace greyhound

