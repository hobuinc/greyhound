#include <greyhound/app.hpp>
#include <greyhound/configuration.hpp>

int main(int argv, char** argc)
{
    greyhound::Configuration config(argv, argc);
    greyhound::App app(config);
    app.start();
}

