#include <greyhound/app.hpp>

namespace greyhound
{

App::App(const Configuration& config)
    : m_config(config)
    , m_manager(config)
{
    auto http(m_config["http"]);
    if (http.isNull()) http["port"] = 8080;

    if (http.isMember("port"))
    {
        unsigned int port(http["port"].asUInt());
        try
        {
            m_http = entwine::makeUnique<Router<Http>>(m_manager, port);
        }
        catch (std::exception& e)
        {
            throw std::runtime_error(
                    std::string("Error creating HTTP server: ") + e.what());
        }

        registerRoutes(*m_http);
    }

    if (http.isMember("securePort"))
    {
        unsigned int port(http["securePort"].asUInt());
        const auto keyFile(http["keyFile"].asString());
        const auto certFile(http["certFile"].asString());

        try
        {
            m_https = entwine::makeUnique<Router<Https>>(
                    m_manager,
                    port,
                    keyFile,
                    certFile);
        }
        catch (std::exception& e)
        {
            throw std::runtime_error(
                    std::string("Error creating HTTPS server: ") + e.what());
        }

        registerRoutes(*m_https);
    }
}

void App::start()
{
    std::cout << "Listening:" << std::endl;
    if (m_http)
    {
        std::cout << "\tHTTP: " << m_http->port() << std::endl;
        m_httpThread = std::thread([this]() { m_http->start(); });
    }

    if (m_https)
    {
        std::cout << "\tHTTPS: " << m_https->port() << std::endl;
        m_httpsThread = std::thread([this]() { m_https->start(); });
    }

    if (m_http) m_httpThread.join();
    if (m_https) m_httpsThread.join();
}

void App::stop()
{
    if (m_http) m_http->stop();
    if (m_https) m_https->stop();
}

template<typename S>
void App::registerRoutes(Router<S>& r)
{
    using Req = typename S::Request;
    using Res = typename S::Response;

    r.get(routes::info, [&](Resource& resource, Req& req, Res& res)
    {
        resource.info(req, res);
    });

    r.get(routes::hierarchy, [&](Resource& resource, Req& req, Res& res)
    {
        resource.hierarchy(req, res);
    });

    r.get(routes::read, [&](Resource& resource, Req& req, Res& res)
    {
        resource.read(req, res);
    });

    r.get(routes::filesRoot, [&](Resource& resource, Req& req, Res& res)
    {
        resource.files(req, res);
    });

    r.get(routes::files, [&](Resource& resource, Req& req, Res& res)
    {
        resource.files(req, res);
    });
}

template void App::registerRoutes(Router<Http>&);
template void App::registerRoutes(Router<Https>&);

} // namespace greyhound

