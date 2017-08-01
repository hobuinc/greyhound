#pragma once

#include <chrono>
#include <map>
#include <stdexcept>
#include <string>

#include <entwine/third/arbiter/arbiter.hpp>

#include <simple-web-server/server_http.hpp>
#include <simple-web-server/server_https.hpp>

namespace greyhound
{

using Http = SimpleWeb::Server<SimpleWeb::HTTP>;
using Https = SimpleWeb::Server<SimpleWeb::HTTPS>;
using HttpStatusCode = SimpleWeb::StatusCode;

inline bool ok(HttpStatusCode c) { return static_cast<int>(c) / 100 == 2; }

using ArbiterHttpResponse = entwine::arbiter::http::Response;
using ArbiterHeaders = std::map<std::string, std::string>;

using Headers = SimpleWeb::CaseInsensitiveMultimap;
using Cookies = std::map<std::string, std::string>;

using Paths = std::vector<std::string>;
using Data = std::vector<char>;

using TimePoint = std::chrono::high_resolution_clock::time_point;

class HttpError : public std::runtime_error
{
public:
    HttpError(std::string message)
        : std::runtime_error(message)
    { }

    HttpError(HttpStatusCode code, std::string message = "Unknown error")
        : std::runtime_error(message)
        , m_code(code)
    { }

    HttpStatusCode code() const { return m_code; }

private:
    const HttpStatusCode m_code =
        HttpStatusCode::server_error_internal_server_error;
};

class Http400 : public HttpError
{
public:
    Http400(std::string message) :
        HttpError(HttpStatusCode::client_error_bad_request, message)
    { }
};

inline TimePoint getNow()
{
    return std::chrono::high_resolution_clock::now();
}

inline std::size_t secondsSince(TimePoint start)
{
    std::chrono::duration<double> d(getNow() - start);
    return std::chrono::duration_cast<std::chrono::seconds>(d).count();
}

inline std::size_t secondsBetween(TimePoint start, TimePoint end)
{
    std::chrono::duration<double> d(end - start);
    return std::chrono::duration_cast<std::chrono::seconds>(d).count();
}

} // namespace greyhound

