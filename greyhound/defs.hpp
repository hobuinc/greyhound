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

using HttpServer = SimpleWeb::Server<SimpleWeb::HTTP>;
using HttpsServer = SimpleWeb::Server<SimpleWeb::HTTPS>;
using Req = HttpServer::Request;
using Res = HttpServer::Response;
using ReqPtr = std::shared_ptr<Req>;
using ResPtr = std::shared_ptr<Res>;
using HttpStatusCode = SimpleWeb::StatusCode;

inline bool ok(HttpStatusCode c) { return static_cast<int>(c) / 100 == 2; }

using ArbiterHttpResponse = entwine::arbiter::http::Response;
using ArbiterHeaders = std::map<std::string, std::string>;

using Headers = SimpleWeb::CaseInsensitiveMultimap;
using Cookies = SimpleWeb::CaseInsensitiveMultimap;

using Paths = std::vector<std::string>;

using Data = std::vector<char>;

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

using TimePoint = std::chrono::high_resolution_clock::time_point;

} // namespace greyhound

