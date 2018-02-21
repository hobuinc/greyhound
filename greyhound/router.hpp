#pragma once

#include <entwine/util/pool.hpp>

#include <greyhound/defs.hpp>
#include <greyhound/manager.hpp>

namespace greyhound
{

template<typename S>
class Router
{
    using Req = typename S::Request;
    using Res = typename S::Response;
    using ReqPtr = std::shared_ptr<typename S::Request>;
    using ResPtr = std::shared_ptr<typename S::Response>;

public:
    template<typename... Args>
    Router(Manager& manager, unsigned int port, Args&&... args)
        : m_manager(manager)
        , m_server(std::forward<Args>(args)...)
        , m_pool(m_manager.threads())
    {
        m_server.config.port = port;
        m_server.config.timeout_request = 0;
        m_server.config.timeout_content = 0;
        m_server.config.thread_pool_size = m_manager.threads();

        m_server.default_resource["GET"] = [](ResPtr res, ReqPtr req)
        {
            res->write(HttpStatusCode::client_error_not_found);
        };

        m_server.on_error = [](ReqPtr req, const SimpleWeb::error_code& ec)
        {
            if (
                    ec &&
                    ec != SimpleWeb::errc::operation_canceled &&
                    ec != SimpleWeb::errc::broken_pipe &&
                    ec != SimpleWeb::asio::error::eof)
            {
                std::cout << "Error " << ec << ": " << ec.message() <<
                    std::endl;
            }
        };
    }

    template<typename F>
    void get(std::string match, F f) { route("GET", match, f); }

    template<typename F>
    void put(std::string match, F f) { route("PUT", match, f); }

    template<typename F>
    void route(std::string method, std::string match, F f)
    {
        m_server.resource[match][method] = [this, &f](ResPtr res, ReqPtr req)
        {
            // res->close_connection_after_response = true;

            // m_pool.add([this, &f, req, res]() mutable
            // {
                auto error(
                        [this, &res](HttpStatusCode code, std::string message)
                {
                    // Don't cache errors.  This is a multi-map, so remove any
                    // existing Cache-Control setting.
                    Headers h(m_manager.headers());
                    for (auto it(h.begin()); it != h.end(); )
                    {
                        if (it->first == "Cache-Control") it = h.erase(it);
                        else ++it;
                    }

                    h.emplace("Cache-Control", "public, max-age=0");
                    res->write(code, message, h);
                });

                try
                {
                    const std::string name(req->path_match[1]);
                    if (auto resource = m_manager.get(name, *req))
                    {
                        f(*resource, *req, *res);
                    }
                    else
                    {
                        throw HttpError(
                                HttpStatusCode::client_error_not_found,
                                name + " could not be created");
                    }
                }
                catch (HttpError& e)
                {
                    std::cout << "HTTP error: " << e.what() << std::endl;
                    error(e.code(), e.what());
                }
                catch (std::exception& e)
                {
                    std::cout << "Caught: " << e.what() << std::endl;
                    error(HttpStatusCode::client_error_bad_request, e.what());
                }
                catch (...)
                {
                    std::cout << "Caught unknown error" << std::endl;
                    error(
                            HttpStatusCode::server_error_internal_server_error,
                            "Internal server error");
                }

                res.reset();
                req.reset();

                m_manager.sweep();
            // });
        };
    }

    void start() { m_server.start(); }
    void stop() { m_server.stop(); m_pool.join(); }
    unsigned int port() const { return m_server.config.port; }

private:
    Manager& m_manager;
    S m_server;

    entwine::Pool m_pool;
};

} // namespace greyhound

