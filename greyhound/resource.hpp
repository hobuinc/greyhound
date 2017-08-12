#pragma once

#include <memory>

#include <greyhound/defs.hpp>

namespace entwine { class Reader; }

namespace greyhound
{

class Manager;

class Resource
{
public:
    template<typename Req, typename Res> void info(Req& req, Res& res);
    template<typename Req, typename Res> void hierarchy(Req& req, Res& res);
    template<typename Req, typename Res> void files(Req& req, Res& res);
    template<typename Req, typename Res> void read(Req& req, Res& res);

    template<typename Req, typename Res> void write(Req& req, Res& res);

    static std::shared_ptr<Resource> create(
            const Manager& manager,
            const std::string& name);

    Resource(
            const std::string& name,
            const Headers& headers,
            std::unique_ptr<entwine::Reader> reader);

    entwine::Reader& reader() { return *m_reader; }

private:
    const std::string m_name;
    const Headers& m_headers;
    std::unique_ptr<entwine::Reader> m_reader;
};

using SharedResource = std::shared_ptr<Resource>;

} // namespace greyhound

