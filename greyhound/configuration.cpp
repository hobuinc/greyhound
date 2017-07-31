#include <greyhound/configuration.hpp>

#include <entwine/util/json.hpp>
#include <entwine/third/arbiter/arbiter.hpp>

namespace greyhound
{

namespace
{

Json::Value defaults()
{
    Json::Value json;
    json["cacheSize"] = static_cast<Json::UInt64>(52224288000);
    json["paths"] = entwine::toJsonArray(
            std::vector<std::string>{
                "/greyhound", "~/greyhound",
                "/entwine", "~/entwine",
                "/opt/data"
            });
    json["resourceTimeoutMinutes"] = 30;
    json["http"]["port"] = 8080;

    Json::Value headers;
    headers["Cache-Control"] = "public, max-age=300";
    headers["Access-Control-Allow-Origin"] = "*";
    headers["Access-Control-Allow-Methods"] = "GET,PUT,POST,DELETE";
    json["http"]["headers"] = headers;

    return json;
}

Configuration::Args normalize(int argc, char** argv)
{
    Configuration::Args args;
    for (int i(1); i < argc; ++i)
    {
        std::string arg(argv[i]);

        if (arg.size() > 2 && arg.front() == '-' && std::isalpha(arg[1]))
        {
            // Expand args of the format "-xvalue" to "-x value".
            args.push_back(arg.substr(0, 2));
            args.push_back(arg.substr(2));
        }
        else
        {
            args.push_back(argv[i]);
        }
    }
    return args;
}

} // unnamed namespace

Configuration::Configuration(const int argc, char** argv)
    : m_json(parse(normalize(argc, argv)))
{ }

Json::Value Configuration::parse(const Args& args)
{
    return fromArgs(fromFile(args), args);
}

Json::Value Configuration::fromFile(const Args& args)
{
    bool configFlag(false);
    std::string configPath;

    for (const auto a : args)
    {
        if (a == "-c") configFlag = true;
        else if (configFlag)
        {
            if (a.front() != '-') configPath = a;
            else configFlag = false;
        }
    }

    if (configPath.size())
    {
        std::cout << "Using configuration at " << configPath << std::endl;
        return entwine::parse(entwine::arbiter::Arbiter().get(configPath));
    }
    else
    {
        std::cout << "Using default config" << std::endl;
        return defaults();
    }
}

Json::Value Configuration::fromArgs(Json::Value json, const Args& args)
{
    std::string flag;
    for (const auto a : args)
    {
        if (a.front() == '-') flag = a;
        else
        {
            if (flag == "-c")
            {
                // We already handled the config-path flag.
            }
            else if (flag == "-p") json["http"]["port"] = std::stoi(a);
            else if (flag == "-d") json["paths"].append(a);
            else
            {
                std::cout << "Ignored argument: " << a << std::endl;
            }
        }
    }
    return json;
}

} // namespace greyhound

