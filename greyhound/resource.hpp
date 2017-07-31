#pragma once

#include <memory>

#include <json/json.h>

#include <greyhound/defs.hpp>

namespace entwine { class Reader; }

namespace greyhound
{

class Manager;

class Resource
{
public:
    void info(Req& req, Res& res);
    void hierarchy(Req& req, Res& res);
    void files(Req& req, Res& res);
    void read(Req& req, Res& res);

    static std::shared_ptr<Resource> create(
            const Manager& manager,
            const std::string name);

    Resource(const Manager& manager, std::unique_ptr<entwine::Reader> reader)
        : m_manager(manager)
        , m_reader(std::move(reader))
    { }

    entwine::Reader& reader() { return *m_reader; }

private:
    Json::Value parseQuery(Req& req) const;

    const Manager& m_manager;
    std::unique_ptr<entwine::Reader> m_reader;
};

using SharedResource = std::shared_ptr<Resource>;

} // namespace greyhound

