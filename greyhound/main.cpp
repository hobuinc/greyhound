#include <csignal>

#include <entwine/util/stack-trace.hpp>

#include <greyhound/app.hpp>
#include <greyhound/configuration.hpp>

int main(int argv, char** argc)
{
    signal(SIGINT, [](int sig) { exit(1); });
    entwine::stackTraceOn(SIGSEGV);

    greyhound::Configuration config(argv, argc);
    greyhound::App app(config);
    app.start();
}

