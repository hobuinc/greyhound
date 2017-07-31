#include <greyhound/manager.hpp>

#include <thread>

#include <entwine/reader/reader.hpp>

namespace greyhound
{

namespace
{
    TimePoint now()
    {
        return std::chrono::high_resolution_clock::now();
    }

    std::size_t secondsSince(TimePoint start)
    {
        std::chrono::duration<double> d(now() - start);
        return std::chrono::duration_cast<std::chrono::seconds>(d).count();
    }
}

TimedResource::TimedResource(SharedResource resource)
    : m_resource(resource)
    , m_touched(now())
{ }

void TimedResource::touch() { m_touched = now(); }
std::size_t TimedResource::since() const { return secondsSince(m_touched); }

Manager::Manager(const Configuration& config)
    : m_cache(config["cacheBytes"].asUInt64())
    , m_paths(entwine::extract<std::string>(config["paths"]))
{
    std::cout << "Paths" << std::endl;
    for (const auto p : m_paths) std::cout << "\t" << p << std::endl;

    m_outerScope.getArbiter(config["arbiter"]);

    for (const auto key : config["http"]["headers"].getMemberNames())
    {
        std::cout << key << ": " <<
            config["http"]["headers"][key].asString() << std::endl;

        m_headers.emplace(key, config["http"]["headers"][key].asString());
    }

    m_headers.emplace("Connection", "keep-alive");
    m_headers.emplace("X-powered-by", "Hobu, Inc.");
    m_headers.emplace("Access-Control-Allow-Headers", "Content-Type");

    m_timeoutSeconds = std::max<double>(
            60.0 * config["resourceTimeoutMinutes"].asDouble(), 30);
    m_lastSweep = now();

    auto loop([this]()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        while (!m_done)
        {
            m_cv.wait_for(lock, std::chrono::seconds(m_timeoutSeconds), [this]()
            {
                return m_done || secondsSince(m_lastSweep) > m_timeoutSeconds;
            });
            sweep();
        }
    });

    m_sweepThread = std::thread(loop);
}

Manager::~Manager()
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_done = true;
    }
    m_cv.notify_all();
    m_sweepThread.join();
}

SharedResource Manager::get(std::string name)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    // TODO Authorization check would be here.

    auto it(m_resources.find(name));
    if (it == m_resources.end())
    {
        if (auto resource = Resource::create(*this, name))
        {
            it = m_resources.insert(std::make_pair(name, resource)).first;
        }
        else return SharedResource();
    }
    it->second.touch();
    return it->second.get();
}

void Manager::sweep()
{
    m_lastSweep = now();
    auto it(m_resources.begin());
    while (it != m_resources.end())
    {
        if (it->second.since() > m_timeoutSeconds)
        {
            std::cout << "Purging " << it->first << std::endl;
            m_cache.release(it->second.get()->reader());
            it = m_resources.erase(it);
        }
        else ++it;
    }
}

} // namespace greyhound

