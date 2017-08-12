#include <greyhound/resource.hpp>

#include <json/json.h>

#include <pdal/Compression.hpp>

#include <entwine/reader/reader.hpp>
#include <entwine/types/delta.hpp>
#include <entwine/types/manifest.hpp>
#include <entwine/types/metadata.hpp>
#include <entwine/types/reprojection.hpp>
#include <entwine/types/schema.hpp>
#include <entwine/types/structure.hpp>
#include <entwine/util/compression.hpp>
#include <entwine/util/unique.hpp>

#include <greyhound/chunker.hpp>
#include <greyhound/manager.hpp>

namespace greyhound
{

namespace
{

std::string dense(const Json::Value& json)
{
    auto s = Json::FastWriter().write(json);
    s.pop_back();
    return s;
}

template<typename Req> Json::Value parseQuery(Req& req)
{
    Json::Value q;
    for (const auto& p : req.parse_query_string())
    {
        q[p.first] = entwine::parse(p.second);
    }
    return q;
}

enum class Color
{
    Black,
    Red,
    Green,
    Yellow,
    Blue,
    Magenta,
    Cyan,
    White
};

const std::map<Color, std::string> colorCodes {
    { Color::Black,     "\x1b[30m" },
    { Color::Red,       "\x1b[31m" },
    { Color::Green,     "\x1b[32m" },
    { Color::Yellow,    "\x1b[33m" },
    { Color::Blue,      "\x1b[34m" },
    { Color::Magenta,   "\x1b[35m" },
    { Color::Cyan,      "\x1b[36m" },
    { Color::White,     "\x1b[37m" }
};

std::string color(const std::string& s, Color c)
{
    return colorCodes.at(c) + s + "\x1b[0m";
}

std::mutex m;

} // unnamed namespace

Resource::Resource(
        const std::string& name,
        const Headers& headers,
        std::unique_ptr<entwine::Reader> reader)
    : m_name(name)
    , m_headers(headers)
    , m_reader(std::move(reader))
{ }

template<typename Req, typename Res>
void Resource::info(Req& req, Res& res)
{
    const auto start(getNow());

    Json::Value json;
    const auto& meta(m_reader->metadata());

    json["type"] = "octree";
    json["numPoints"] = Json::UInt64(meta.manifest().pointStats().inserts());
    json["schema"] = meta.schema().toJson();
    json["bounds"] = meta.boundsNativeCubic().toJson();
    json["boundsConforming"] = meta.boundsNativeConforming().toJson();
    json["srs"] = meta.srs();
    json["baseDepth"] = Json::UInt64(meta.structure().baseDepthBegin());

    if (const auto r = meta.reprojection()) json["reprojection"] = r->toJson();
    if (meta.density()) json["density"] = meta.density();
    if (const auto d = meta.delta()) d->insertInto(json);

    if (const auto s = m_reader->additional())
    {
        entwine::DimList dims(meta.schema().dims());
        dims.insert(dims.end(), s->dims().begin(), s->dims().end());
        json["schema"] = entwine::Schema(dims).toJson();
    }

    auto h(m_headers);
    h.emplace("Content-Type", "application/json");
    res.write(json.toStyledString(), h);

    std::lock_guard<std::mutex> lock(m);
    std::cout << m_name << "/" << color("info", Color::Green) << ": " <<
        color(std::to_string(msSince(start)), Color::Magenta) << " ms" <<
        std::endl;
}

template<typename Req, typename Res>
void Resource::hierarchy(Req& req, Res& res)
{
    const auto start(getNow());

    const Json::Value q(parseQuery(req));
    const Json::Value result(m_reader->hierarchy(q));
    auto h(m_headers);
    h.emplace("Content-Type", "application/json");
    res.write(dense(result), h);

    std::lock_guard<std::mutex> lock(m);
    std::cout << m_name << "/" << color("hier", Color::Yellow) << ": " <<
        color(std::to_string(msSince(start)), Color::Magenta) << " ms ";

    std::cout << "D: [";
    if (q.isMember("depthBegin")) std::cout << q["depthBegin"].asUInt();
    else if (q.isMember("depth")) std::cout << q["depth"].asUInt();
    else std::cout << "all";

    std::cout << ", ";
    if (q.isMember("depthEnd")) std::cout << q["depthEnd"].asUInt();
    else if (q.isMember("depth")) std::cout << q["depth"].asUInt() + 1;
    else std::cout << "all";

    std::cout << ")" << std::endl;
}

template<typename Req, typename Res>
void Resource::files(Req& req, Res& res)
{
    const auto start(getNow());

    auto h(m_headers);
    h.emplace("Content-Type", "application/json");

    const std::string root(req.path_match[2]);
    Json::Value query(parseQuery(req));

    if (root.size())
    {
        // Transform /files/42 -> /files?search=42
        if (query.isNull())
        {
            if (std::all_of(root.begin(), root.end(), ::isdigit))
            {
                query["search"] = Json::UInt64(std::stoul(root));
            }
            else query["search"] = root;
        }
        else throw Http400("Cannot specify an OriginId and a query");
    }

    if (query.isNull())
    {
        // For a root-level /files query, return a JSON array of all paths.
        const auto paths(m_reader->metadata().manifest().paths());
        res.write(dense(entwine::toJsonArray(paths)), h);
    }
    else if (query.isObject())
    {
        Json::Value result;

        if (query.isMember("bounds") && query.isMember("search"))
        {
            throw Http400("Invalid query - cannot specify bounds and search");
        }

        if (query.isMember("bounds"))
        {
            const entwine::Bounds bounds(query["bounds"]);

            if (auto delta = entwine::Delta::maybeCreate(query))
            {
                result = toJson(
                        m_reader->files(
                            bounds,
                            &delta->scale(),
                            &delta->offset()));
            }
            else result = toJson(m_reader->files(bounds));
        }
        else if (query.isMember("search"))
        {
            auto single([this](const Json::Value& v)->Json::Value
            {
                if (v.isIntegral())
                {
                    try { return m_reader->files(v.asUInt64()).toJson(); }
                    catch (...) { return Json::nullValue; }
                }
                else if (v.isString())
                {
                    try { return m_reader->files(v.asString()).toJson(); }
                    catch (...) { return Json::nullValue; }
                }
                else throw Http400("Invalid files query");
            });

            const auto& search(query["search"]);
            if (!search.isArray()) result = single(search);
            else for (const auto& v : search) result.append(single(v));
        }

        res.write(dense(result), h);
    }
    else throw Http400("Invalid files query");

    std::lock_guard<std::mutex> lock(m);
    std::cout << m_name << "/" << color("file", Color::Green) << ": " <<
        color(std::to_string(msSince(start)), Color::Magenta) << " ms " <<
        "Q: " << (root.size() ? root : dense(query)) <<
        std::endl;
}

template<typename Req, typename Res>
void Resource::read(Req& req, Res& res)
{
    const auto start(getNow());

    const Json::Value q(parseQuery(req));
    auto query(m_reader->getQuery(q));

    using Compressor = pdal::LazPerfCompressor<Stream>;
    Stream stream;
    std::unique_ptr<Compressor> compressor;
    if (q["compress"].asBool())
    {
        compressor = entwine::makeUnique<Compressor>(
                stream,
                query->schema().pdalLayout().dimTypes());
    }

    Chunker<Res> chunker(res, m_headers);
    auto& data(chunker.data());

    uint32_t points(0);

    while (!query->done())
    {
        query->next(data);

        if (compressor)
        {
            compressor->compress(data.data(), data.size());
            if (query->done()) compressor->done();
            data = std::move(stream.data());
        }

        if (query->done())
        {
            points = query->numPoints();
            const char* pos(reinterpret_cast<const char*>(&points));
            data.insert(data.end(), pos, pos + sizeof(uint32_t));
        }

        chunker.write(query->done());
    }

    std::lock_guard<std::mutex> lock(m);
    std::cout << m_name << "/" << color("read", Color::Cyan) << ": " <<
        color(std::to_string(msSince(start)), Color::Magenta) << " ms";

    std::cout << " D: [";
    if (q.isMember("depthBegin")) std::cout << q["depthBegin"].asUInt();
    else if (q.isMember("depth")) std::cout << q["depth"].asUInt();
    else std::cout << "all";

    std::cout << ", ";
    if (q.isMember("depthEnd")) std::cout << q["depthEnd"].asUInt();
    else if (q.isMember("depth")) std::cout << q["depth"].asUInt() + 1;
    else std::cout << "all";

    std::cout << ")";
    std::cout << " P: " << points;

    if (q.isMember("filter")) std::cout << " F: " << dense(q["filter"]);

    std::cout << std::endl;
}

template<typename Req, typename Res>
void Resource::write(Req& req, Res& res)
{
    const auto start(getNow());

    const Json::Value q(parseQuery(req));
    const std::size_t size(req.content.size());

    std::vector<char> data;
    data.reserve(size);
    data.assign(
            (std::istreambuf_iterator<char>(req.content)),
            std::istreambuf_iterator<char>());
    if (data.size() != size) throw std::runtime_error("Invalid size");

    const std::size_t points(m_reader->write(q["name"].asString(), data, q));

    res.write("");

    std::lock_guard<std::mutex> lock(m);
    std::cout << m_name << "/" << color("write", Color::Yellow) << ": " <<
        color(std::to_string(msSince(start)), Color::Magenta) << " ms";

    std::cout << " D: [";
    if (q.isMember("depthBegin")) std::cout << q["depthBegin"].asUInt();
    else if (q.isMember("depth")) std::cout << q["depth"].asUInt();
    else std::cout << "all";

    std::cout << ", ";
    if (q.isMember("depthEnd")) std::cout << q["depthEnd"].asUInt();
    else if (q.isMember("depth")) std::cout << q["depth"].asUInt() + 1;
    else std::cout << "all";

    std::cout << ")";
    std::cout << " P: " << points;

    if (q.isMember("filter")) std::cout << " F: " << dense(q["filter"]);

    std::cout << std::endl;
}

SharedResource Resource::create(const Manager& manager, const std::string& name)
{
    using namespace entwine;
    std::cout << "Creating " << name << std::endl;

    for (const auto& path : manager.paths())
    {
        std::cout << "\tTrying " << path << ": ";

        try
        {
            entwine::arbiter::Endpoint endpoint(
                    manager.outerScope().getArbiterPtr()->getEndpoint(
                        entwine::arbiter::util::join(path, name)));

            if (auto r = entwine::makeUnique<entwine::Reader>(
                        endpoint,
                        manager.cache()))
            {
                std::cout << "SUCCESS" << std::endl;
                return std::make_shared<Resource>(
                        name,
                        manager.headers(),
                        std::move(r));
            }
            else std::cout << "fail - null result received" << std::endl;
        }
        catch (const std::exception& e)
        {
            std::cout << "fail - " << e.what() << std::endl;
        }
        catch (...)
        {
            std::cout << "fail - unknown error" << std::endl;
        }
    }

    return SharedResource();
}

template void Resource::info(Http::Request&, Http::Response&);
template void Resource::hierarchy(Http::Request&, Http::Response&);
template void Resource::files(Http::Request&, Http::Response&);
template void Resource::read(Http::Request&, Http::Response&);
template void Resource::write(Http::Request&, Http::Response&);

template void Resource::info(Https::Request&, Https::Response&);
template void Resource::hierarchy(Https::Request&, Https::Response&);
template void Resource::files(Https::Request&, Https::Response&);
template void Resource::read(Https::Request&, Https::Response&);
template void Resource::write(Https::Request&, Https::Response&);

} // namespace greyhound

