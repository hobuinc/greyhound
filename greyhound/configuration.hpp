#pragma once

#include <string>
#include <vector>

#include <json/json.h>

namespace greyhound
{

class Configuration
{
public:
    Configuration(int argc, char** argv);

    const Json::Value& operator[](const std::string& s) const
    {
        return m_json[s];
    }

    const Json::Value& json() const { return m_json; }

    using Args = std::vector<std::string>;

private:
    Json::Value parse(const Args& args);
    Json::Value fromFile(const Args& args);
    Json::Value fromArgs(Json::Value base, const Args& args);

    Json::Value m_json;
};

} // namespace greyhound

