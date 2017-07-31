#pragma once

#include <mutex>
#include <set>

#include <greyhound/configuration.hpp>
#include <greyhound/defs.hpp>
#include <greyhound/resource.hpp>

namespace greyhound
{

class Manager
{
public:
    Manager(const Configuration& config)
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
    }

    SharedResource get(std::string name)
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
        return it->second;
    }

    entwine::Cache& cache() const { return m_cache; }
    entwine::OuterScope& outerScope() const { return m_outerScope; }
    const Paths& paths() const { return m_paths; }
    const Headers& headers() const { return m_headers; }

private:
    mutable entwine::Cache m_cache;
    mutable entwine::OuterScope m_outerScope;

    Paths m_paths;
    Headers m_headers;

    std::map<std::string, SharedResource> m_resources;

    mutable std::mutex m_mutex;
};

} // namespace greyhound

