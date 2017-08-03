#include <csignal>

#include <greyhound/app.hpp>
#include <greyhound/configuration.hpp>

int main(int argv, char** argc)
{
    signal(SIGINT, [](int sig) { exit(1); });
    greyhound::Configuration config(argv, argc);
    greyhound::App app(config);
    app.start();
}

