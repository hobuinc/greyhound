#pragma once

#include <greyhound/configuration.hpp>
#include <greyhound/manager.hpp>
#include <greyhound/router.hpp>

namespace greyhound
{

class App
{
public:
    App(const Configuration& config);

    void start();
    void stop();

private:
    template<typename S> void registerRoutes(Router<S>& r);

    Configuration m_config;
    Manager m_manager;

    std::unique_ptr<Router<Http>> m_http;
    std::unique_ptr<Router<Https>> m_https;

    std::thread m_httpThread;
    std::thread m_httpsThread;
};

} // namespace greyhound

