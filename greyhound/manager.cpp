#include <greyhound/manager.hpp>

#include <algorithm>
#include <cctype>
#include <thread>

#include <entwine/new-reader/new-reader.hpp>
#include <entwine/util/json.hpp>

namespace greyhound
{

namespace
{
    /*
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
    */

    std::string dense(const Json::Value& json)
    {
        auto s = Json::FastWriter().write(json);
        s.pop_back();
        return s;
    }
}

Manager::Manager(const Configuration& config)
    : m_cache()
    /*
            config["cacheSize"].isString() ?
                parseBytes(config["cacheSize"].asString()) :
                config["cacheSize"].asUInt64())
    */
    , m_paths(entwine::extract<std::string>(config["paths"]))
    , m_threads(std::max<std::size_t>(config["threads"].asUInt(), 4))
    , m_config(config)
    , m_swept(getNow())
{
    m_outerScope.getArbiter(config["arbiter"]);
    m_auth = Auth::maybeCreate(config, *m_outerScope.getArbiter());

    for (const auto key : config["http"]["headers"].getMemberNames())
    {
        m_headers.emplace(key, config["http"]["headers"][key].asString());
    }

    m_headers.emplace("Connection", "keep-alive");
    m_headers.emplace("X-powered-by", "Hobu, Inc.");
    m_headers.emplace("Access-Control-Allow-Headers", "Content-Type");

    m_timeoutSeconds = std::max<double>(
            60.0 * config["resourceTimeoutMinutes"].asDouble(), 15);

    std::cout << "Settings:" << std::endl;
    // std::cout << "\tCache: " << m_cache.maxBytes() << " bytes" << std::endl;
    std::cout << "\tThreads: " << m_threads << std::endl;
    std::cout << "\tResource timeout: " <<
        (m_timeoutSeconds / 60.0)  << " minutes" << std::endl;
    std::cout << "\tTmp dir: " << m_config["tmp"].asString() << std::endl;
    std::cout << "Paths:" << std::endl;
    for (const auto p : m_paths) std::cout << "\t" << p << std::endl;

    std::cout << "Headers:" << std::endl;
    for (const auto key : config["http"]["headers"].getMemberNames())
    {
        std::cout << "\t" << key << ": " <<
            config["http"]["headers"][key].asString() << std::endl;
    }

    if (config.json().isMember("aliases"))
    {
        if (config["aliases"].size())
        {
            std::cout << "Registering alias:" << std::endl;
        }

        for (const std::string k : config["aliases"].getMemberNames())
        {
            std::cout << "\t" << k << std::endl;
            m_aliases[k] = entwine::extract<std::string>(config["aliases"][k]);
        }
    }

    std::cout << "Writes " << (config["allowWrite"].asBool() ? "are" : "NOT") <<
            " allowed" << std::endl;

    if (m_auth)
    {
        std::cout << "Auth:" << std::endl;
        std::cout << "\tPath: " << m_auth->path() << std::endl;
        if (m_auth->cookies().size())
        {
            std::cout << "\tCookies: " <<
                dense(entwine::toJsonArray(m_auth->cookies())) << std::endl;
        }
        if (m_auth->queries().size())
        {
            std::cout << "\tQuery params: " <<
                dense(entwine::toJsonArray(m_auth->queries())) << std::endl;
        }
        std::cout << "\tSuccess timeout: " << m_auth->goodSeconds() << "s" <<
            std::endl;
        std::cout << "\tFailure timeout: " << m_auth->badSeconds() << "s" <<
            std::endl;
    }
}

void Manager::sweep()
{
    if (secondsSince(m_swept) < m_timeoutSeconds) return;

    for (auto it(m_readers.begin()); it != m_readers.end(); ++it)
    {
        TimedReader& tr(it->second);
        if (tr.sweep()) return;
    }
}

} // namespace greyhound

