#include <greyhound/app.hpp>

#include <fstream>

#include <entwine/third/arbiter/arbiter.hpp>

namespace greyhound
{

namespace
{

bool exists(const std::string path)
{
    return std::ifstream(path).good();
}

const std::string publicRoot(([]()->std::string
{
    const std::string ins(installPrefix() + "/include/greyhound/public");
    const std::string cwd(
            entwine::arbiter::util::join(
                entwine::arbiter::util::getNonBasename(__FILE__), "public"));

    if (exists(entwine::arbiter::util::join(ins, "index.html"))) return ins;
    if (exists(entwine::arbiter::util::join(cwd, "index.html"))) return cwd;
    return "";
})());

}

namespace routes
{

const std::string resourceBase("^/resource/(.*)");

const std::string info(resourceBase + "/info$");
const std::string filesRoot(resourceBase + "/files$");
const std::string files(resourceBase + "/files/(.*)$");
const std::string read(resourceBase + "/read$");
const std::string hierarchy(resourceBase + "/hierarchy$");

const std::string renderRoot(resourceBase + "/static$");
const std::string render(resourceBase + "/static/(.*)$");

const std::string write(resourceBase + "/write$");

}

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

    r.put(routes::write, [this](Resource& resource, Req& req, Res& res)
    {
        resource.write(req, res);
    });

    r.get(routes::info, [this](Resource& resource, Req& req, Res& res)
    {
        resource.info(req, res);
    });

    r.get(routes::hierarchy, [this](Resource& resource, Req& req, Res& res)
    {
        resource.hierarchy(req, res);
    });

    r.get(routes::read, [this](Resource& resource, Req& req, Res& res)
    {
        resource.read(req, res);
    });

    r.get(routes::filesRoot, [this](Resource& resource, Req& req, Res& res)
    {
        resource.files(req, res);
    });

    r.get(routes::files, [this](Resource& resource, Req& req, Res& res)
    {
        resource.files(req, res);
    });

    std::cout << "Static serve:\n\t";
    if (publicRoot.size()) std::cout << publicRoot << std::endl;
    else
    {
        std::cout << "(not found)" << std::endl;;
        return;
    }

    auto render([](Resource& resource, Req& req, Res& res)
    {
        std::string p(req.path_match[2]);
        if (p.empty()) p = "index.html";

        const std::string path(entwine::arbiter::util::join(publicRoot, p));
        auto ifs = std::make_shared<std::ifstream>(
                path,
                std::ifstream::in | std::ios::binary | std::ios::ate);

        if (ifs->good()) res.write(*ifs);
        else res.write(HttpStatusCode::client_error_not_found);
    });

    r.get(routes::render, render);
    r.get(routes::renderRoot, render);
}

template void App::registerRoutes(Router<Http>&);
template void App::registerRoutes(Router<Https>&);

} // namespace greyhound

