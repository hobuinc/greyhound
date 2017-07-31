#include <greyhound/manager.hpp>

#include <algorithm>
#include <cctype>
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

    std::size_t parseBytes(std::string s)
    {
        s.erase(std::remove_if(s.begin(), s.end(), ::isspace), s.end());
        std::transform(s.begin(), s.end(), s.begin(), ::tolower);

        const auto alpha(std::find_if(s.begin(), s.end(), ::isalpha));
        const std::string numeric(s.begin(), alpha);
        const std::string postfix(alpha, s.end());

        double n(std::stod(numeric));
        double m(1);

        if      (postfix == "b")  m = 1;
        else if (postfix == "kb") m = (1ull << 10);
        else if (postfix == "mb") m = (1ull << 20);
        else if (postfix == "gb") m = (1ull << 30);
        else if (postfix == "tb") m = (1ull << 40);
        else throw std::runtime_error("Could not parse: " + s);

        return n * m;
    }
}

TimedResource::TimedResource(SharedResource resource)
    : m_resource(resource)
    , m_touched(now())
{ }

void TimedResource::touch() { m_touched = now(); }
std::size_t TimedResource::since() const { return secondsSince(m_touched); }

Manager::Manager(const Configuration& config)
    : m_cache(
            config["cacheSize"].isString() ?
                parseBytes(config["cacheSize"].asString()) :
                config["cacheSize"].asUInt64())
    , m_paths(entwine::extract<std::string>(config["paths"]))
{
    m_outerScope.getArbiter(config["arbiter"]);

    for (const auto key : config["http"]["headers"].getMemberNames())
    {
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

    std::cout << "Cache size: " << m_cache.maxBytes() << " bytes" << std::endl;
    std::cout << "Resource timeout: " <<
        (m_timeoutSeconds / 60.0)  << " minutes" << std::endl;
    std::cout << "Paths:" << std::endl;
    for (const auto p : m_paths) std::cout << "\t" << p << std::endl;

    std::cout << "Headers:" << std::endl;
    for (const auto key : config["http"]["headers"].getMemberNames())
    {
        std::cout << "\t" << key << ": " <<
            config["http"]["headers"][key].asString() << std::endl;
    }
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

