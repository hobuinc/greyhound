#pragma once

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
    {
        m_server.config.port = port;
        m_server.config.thread_pool_size = 8;

        m_server.default_resource["GET"] = [](ResPtr res, ReqPtr req)
        {
            res->write(HttpStatusCode::client_error_not_found);
        };

        m_server.on_error = [](ReqPtr req, const SimpleWeb::error_code& ec)
        {
            std::cout << "Error " << ec << ": " << ec.message() << std::endl;
        };
    }

    template<typename F>
    void get(std::string match, F f)
    {
        m_server.resource[match]["GET"] = [this, &f](ResPtr res, ReqPtr req)
        {
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
                res->write(e.code(), e.what());
            }
            catch (std::exception& e)
            {
                res->write(HttpStatusCode::client_error_bad_request, e.what());
            }
            catch (...)
            {
                res->write(
                        HttpStatusCode::server_error_internal_server_error,
                        "Unknown error");
            }
        };
    }

    void start() { m_server.start(); }
    void stop() { m_server.stop(); }
    unsigned int port() const { return m_server.config.port; }

private:
    Manager& m_manager;
    S m_server;
};

} // namespace greyhound

