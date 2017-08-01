#pragma once

#include <greyhound/defs.hpp>
#include <greyhound/manager.hpp>
#include <greyhound/auth.hpp>

namespace routes
{

const std::string resourceBase("^/resource/(.*)");

const std::string info(resourceBase + "/info$");
const std::string filesRoot(resourceBase + "/files$");
const std::string files(resourceBase + "/files/(.*)$");
const std::string read(resourceBase + "/read$");
const std::string hierarchy(resourceBase + "/hierarchy$");

}

namespace greyhound
{

class Router
{
public:
    Router(const Configuration& config)
        : m_manager(config)
    {
        m_http.config.port = config["http"]["port"].asUInt();
        m_http.config.thread_pool_size = 8;
        m_http.default_resource["GET"] = [](ResPtr res, ReqPtr req)
        {
            res->write(HttpStatusCode::client_error_not_found);
        };
    }

    template<typename F>
    void get(std::string match, F f)
    {
        m_http.resource[match]["GET"] = [this, &f](ResPtr res, ReqPtr req)
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

    void start()
    {
        std::cout << "HTTP port: " << m_http.config.port <<
            std::endl;
        m_http.start();
    }

private:
    HttpServer m_http;
    HttpsServer m_https;
    Manager m_manager;
};

} // namespace greyhound

