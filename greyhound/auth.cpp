#include <greyhound/auth.hpp>

#include <cctype>

#include <entwine/util/json.hpp>
#include <entwine/util/unique.hpp>

namespace greyhound
{
namespace
{

std::string trim(std::string s)
{
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](char c) {
        return !::isspace(c);
    }));

    s.erase(std::find_if(s.rbegin(), s.rend(), [](char c) {
        return !::isspace(c);
    }).base(), s.end());

    return s;
}

template<typename Req>
Cookies parseCookies(Req& req)
{
    Cookies cookies;

    auto cit = req.header.find("Cookie");
    if (cit == req.header.end()) return cookies;
    const std::string cookie = cit->second;

    std::string s;
    std::size_t pos(0), end(0), mid(0);

    while (pos != std::string::npos)
    {
        end = cookie.find_first_of(";", pos);
        s = cookie.substr(pos, end == std::string::npos ? end : end - pos);
        mid = s.find_first_of("=");

        if (mid != std::string::npos)
        {
            cookies.emplace(trim(s.substr(0, mid)), trim(s.substr(mid + 1)));
        }
        else
        {
            cookies.emplace("", trim(s));
        }

        pos = end == std::string::npos ? end : end + 1;
    }

    return cookies;
}

} // unnamed namespace

Auth::Auth(
        const entwine::arbiter::Endpoint& ep,
        const std::vector<std::string> cookies,
        const std::vector<std::string> queries,
        const std::size_t good,
        const std::size_t bad)
    : m_ep(ep)
    , m_cookies(cookies)
    , m_queries(queries)
    , m_good(std::max<std::size_t>(good, 60))
    , m_bad(std::max<std::size_t>(bad, 60))
{ }

template<typename Req>
HttpStatusCode Auth::check(const std::string& resource, Req& req)
{
    const auto cookies(parseCookies(req));
    const Query inQuery(req.parse_query_string());

    std::string id;
    for (const auto c : m_cookies)
    {
        auto it(cookies.find(c));
        const std::string val(it != cookies.end() ? it->second : "");
        id += val + "-";
    }
    for (const auto q : m_queries)
    {
        auto it(inQuery.find(q));
        const std::string val(it != inQuery.end() ? it->second : "");
        id += val + "-";
    }

    const auto now(getNow());

    std::unique_lock<std::mutex> outerLock(m_mutex);
    Entry& entry(m_map[id][resource]);

    std::unique_lock<std::mutex> lock(entry.mutex());
    outerLock.unlock();

    if (
            ( entry.ok() && secondsBetween(entry.checked(), now) > m_good) ||
            (!entry.ok() && secondsBetween(entry.checked(), now) > m_bad))
    {
        std::cout << "Authing " << id << std::endl;
        const ArbiterHeaders h(req.header.begin(), req.header.end());
        const ArbiterQuery q(inQuery.begin(), inQuery.end());

        entry.set(m_ep.httpGet(resource, h, q).code());
    }

    return entry.code();
}

std::unique_ptr<Auth> Auth::maybeCreate(
    const Configuration& config,
    const entwine::arbiter::Arbiter& a)
{
    if (config.json().isMember("auth"))
    {
        const auto& auth(config["auth"]);
        auto& time(auth["cacheMinutes"]);

        std::vector<std::string> cookies;

        if (auth.isMember("cookies"))
        {
            if (auth.isMember("cookieName"))
            {
                throw std::runtime_error(
                        "Cannot specify both cookies and cookieName");
            }

            if (auth["cookies"].isArray())
            {
                cookies = entwine::extract<std::string>(auth["cookies"]);
            }
            else if (auth["cookies"].isString())
            {
                cookies.push_back(auth["cookies"].asString());
            }
            else
            {
                throw std::runtime_error("Invalid cookies specification");
            }
        }
        else if (auth.isMember("cookieName"))
        {
            cookies.push_back(auth["cookieName"].asString());
        }

        std::vector<std::string> queries;

        if (auth.isMember("queryParams"))
        {
            if (auth["queryParams"].isArray())
            {
                queries = entwine::extract<std::string>(auth["queryParams"]);
            }
            else if (auth["queryParams"].isString())
            {
                queries.push_back(auth["queryParams"].asString());
            }
            else
            {
                throw std::runtime_error("Invalid queryParams specification");
            }
        }

        const std::size_t good(
                time.isMember("good") ?
                    time["good"].asDouble() * 60.0 :
                    time.asDouble() * 60.0);

        const std::size_t bad(
                time.isMember("bad") ?
                    time["bad"].asDouble() * 60.0 :
                    time.asDouble() * 60.0);

        return entwine::makeUnique<Auth>(
                a.getEndpoint(auth["path"].asString()),
                cookies,
                queries,
                good,
                bad);
    }
    else return std::unique_ptr<Auth>();
}

template HttpStatusCode Auth::check(const std::string&, Http::Request&);
template HttpStatusCode Auth::check(const std::string&, Https::Request&);

} // namespace greyhound

