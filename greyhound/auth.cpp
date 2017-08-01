#include <greyhound/auth.hpp>

#include <cctype>

#include <entwine/third/arbiter/arbiter.hpp>
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

using Cookies = std::map<std::string, std::string>;

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
        const std::string& cookieName,
        const std::size_t good,
        const std::size_t bad)
    : m_ep(ep)
    , m_cookieName(cookieName)
    , m_good(std::max<std::size_t>(good, 60))
    , m_bad(std::max<std::size_t>(bad, 60))
{ }

HttpStatusCode Auth::check(const std::string& resource, Req& req)
{
    const auto cookies(parseCookies(req));
    auto f(cookies.find(m_cookieName));
    const std::string id(f != cookies.end() ? f->second : "");

    const auto now(getNow());

    std::unique_lock<std::mutex> outerLock(m_mutex);
    Entry& entry(m_map[id][resource]);

    std::unique_lock<std::mutex> lock(entry.mutex());
    outerLock.unlock();

    if (
            ( entry.ok() && secondsBetween(entry.checked(), now) > m_good) ||
            (!entry.ok() && secondsBetween(entry.checked(), now) > m_bad))
    {
        ArbiterHeaders h(req.header.begin(), req.header.end());
        entry.set(m_ep.httpGet(resource, h).code());
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
                auth["cookieName"].asString(),
                good,
                bad);
    }
    else return std::unique_ptr<Auth>();
}

} // namespace greyhound

