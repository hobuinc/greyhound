#pragma once

#include <string>

#include <entwine/third/arbiter/arbiter.hpp>

#include <greyhound/defs.hpp>
#include <greyhound/configuration.hpp>
#include <greyhound/timing.hpp>

namespace greyhound
{

class Auth
{
public:
    Auth(
            const entwine::arbiter::Endpoint& ep,
            const std::string& cookieName,
            std::size_t good,
            std::size_t bad);

    static std::unique_ptr<Auth> maybeCreate(
        const Configuration& config,
        const entwine::arbiter::Arbiter& a);

    HttpStatusCode check(const std::string& name, Req& req);

    std::string path() const { return m_ep.prefixedRoot(); }
    const std::string& cookieName() const { return m_cookieName; }
    std::size_t goodSeconds() const { return m_good; }
    std::size_t badSeconds() const { return m_bad; }

    class Entry
    {
    public:
        void set(int c)
        {
            m_code = static_cast<HttpStatusCode>(c);
            m_checked = getNow();
        }

        const TimePoint& checked() const { return m_checked; }
        HttpStatusCode code() const { return m_code; }
        bool ok() const { return greyhound::ok(m_code); }
        std::mutex& mutex() const { return m_mutex; }

    private:
        HttpStatusCode m_code = HttpStatusCode::client_error_unauthorized;
        TimePoint m_checked;
        mutable std::mutex m_mutex;
    };

private:
    const entwine::arbiter::Endpoint m_ep;
    const std::string m_cookieName;
    const std::size_t m_good;
    const std::size_t m_bad;

    using UserId = std::string;
    using ResourceName = std::string;
    std::map<UserId, std::map<ResourceName, Entry>> m_map;
    mutable std::mutex m_mutex;
};

} // namespace greyhound

