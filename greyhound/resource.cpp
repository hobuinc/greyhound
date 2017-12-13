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

SharedReader& TimedReader::get()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_touched = getNow();
    if (m_reader) return m_reader;

    create();

    if (!m_reader)
    {
        throw HttpError(
                HttpStatusCode::client_error_not_found,
                "Not found: " + m_name);
    }
    else return m_reader;
}

void TimedReader::create()
{
    std::cout << "Creating " << m_name << std::endl;

    for (const auto& path : m_manager.paths())
    {
        std::cout << "\tTrying " << path << ": ";

        try
        {
            entwine::arbiter::Endpoint ep(
                    m_manager.outerScope().getArbiterPtr()->getEndpoint(
                        entwine::arbiter::util::join(path, m_name)));

            entwine::arbiter::Endpoint tmp(
                    m_manager.outerScope().getArbiterPtr()->getEndpoint(
                        m_manager.config()["tmp"].asString()));

            auto& cache(m_manager.cache());

            if (auto r = std::make_shared<entwine::Reader>(ep, tmp, cache))
            {
                std::cout << "SUCCESS" << std::endl;
                m_reader = r;
                return;
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
}

void TimedReader::reset()
{
    if (m_reader)
    {
        m_manager.cache().release(*m_reader);
        m_reader.reset();
    }
}

Resource::Resource(
        const Manager& manager,
        const std::string& name,
        std::vector<TimedReader*> readers)
    : m_manager(manager)
    , m_name(name)
    , m_readers(readers)
{ }

Json::Value Resource::infoSingle() const
{
    Json::Value json;
    SharedReader reader(m_readers.front()->get());
    const auto& meta(reader->metadata());

    entwine::Schema schema(meta.schema());
    entwine::Schema addons;
    for (const auto& p : reader->appends())
    {
        addons = addons.append(entwine::Schema(p.second));
    }

    json["type"] = "octree";
    json["numPoints"] = Json::UInt64(meta.manifest().pointStats().inserts());
    json["schema"] = schema.toJson();
    if (addons.pointSize()) json["addons"] = addons.toJson();
    json["bounds"] = meta.boundsNativeCubic().toJson();
    json["boundsConforming"] = meta.boundsNativeConforming().toJson();
    json["srs"] = meta.srs();
    json["baseDepth"] = Json::UInt64(meta.structure().baseDepthBegin());

    if (const auto r = meta.reprojection()) json["reprojection"] = r->toJson();
    if (meta.density()) json["density"] = meta.density();
    if (const auto d = meta.delta()) d->insertInto(json);

    return json;
}

Json::Value Resource::infoMulti() const
{
    entwine::Schema schema;
    entwine::Schema addons;
    entwine::Bounds bounds(entwine::Bounds::expander());
    entwine::Bounds boundsConforming(entwine::Bounds::expander());
    std::set<std::string> srsList;
    std::size_t numPoints(0);
    std::size_t baseDepth(0);
    double density(0);
    entwine::Scale scale(1);

    for (const auto& tr : m_readers)
    {
        auto r(tr->get());

        const auto& meta(r->metadata());
        schema = schema.merge(meta.schema());
        for (const auto& a : r->appends()) addons = addons.append(a.second);
        bounds.grow(meta.boundsNativeCubic());
        boundsConforming.grow(meta.boundsNativeConforming());
        if (!meta.srs().empty()) srsList.insert(meta.srs());
        numPoints += meta.manifest().pointStats().inserts();
        baseDepth = std::max(baseDepth, meta.structure().baseDepthBegin());
        density = std::max(density, meta.density());
        if (const auto d = meta.delta())
        {
            scale = entwine::Point::min(scale, d->scale());
        }
    }

    Json::Value json;
    json["type"] = "octree";
    json["schema"] = schema.toJson();
    json["bounds"] = bounds.cubeify().toJson();
    json["numPoints"] = Json::UInt64(numPoints);
    json["boundsConforming"] = boundsConforming.toJson();
    if (srsList.size() == 1) json["srs"] = *srsList.begin();
    json["baseDepth"] = Json::UInt64(baseDepth);

    // if (const auto r = meta.reprojection()) json["reprojection"] = r->toJson();
    if (density) json["density"] = density;
    if (scale != entwine::Scale(1)) json["scale"] = scale.toJson();

    return json;
}

template<typename Req, typename Res>
void Resource::info(Req& req, Res& res)
{
    const auto start(getNow());

    auto h(m_manager.headers());
    h.emplace("Content-Type", "application/json");
    res.write(getInfo().toStyledString(), h);

    std::lock_guard<std::mutex> lock(m);
    std::cout << m_name << "/" << color("info", Color::Green) << ": " <<
        color(std::to_string(msSince(start)), Color::Magenta) << " ms" <<
        std::endl;
}

template<typename Req, typename Res>
void Resource::hierarchy(Req& req, Res& res)
{
    const auto start(getNow());

    if (m_readers.size() != 1)
    {
        throw std::runtime_error("Hierarchy not allowed for multi-resource");
    }

    const Json::Value q(parseQuery(req));
    const Json::Value result(m_readers.front()->get()->hierarchy(q));
    auto h(m_manager.headers());
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

    if (m_readers.size() != 1)
    {
        throw std::runtime_error("Files not allowed for multi-resource");
    }

    SharedReader reader(m_readers.front()->get());
    auto h(m_manager.headers());
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
        const auto paths(reader->metadata().manifest().paths());
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
                        reader->files(
                            bounds,
                            &delta->scale(),
                            &delta->offset()));
            }
            else result = toJson(reader->files(bounds));
        }
        else if (query.isMember("search"))
        {
            auto single([&reader](const Json::Value& v)->Json::Value
            {
                if (v.isIntegral())
                {
                    try { return reader->files(v.asUInt64()).toJson(); }
                    catch (...) { return Json::nullValue; }
                }
                else if (v.isString())
                {
                    try { return reader->files(v.asString()).toJson(); }
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

    SharedReader reader(m_readers.front()->get());
    auto query(reader->getQuery(q));

    using Compressor = pdal::LazPerfCompressor<Stream>;
    Stream stream;
    std::unique_ptr<Compressor> compressor;
    if (q["compress"].asBool())
    {
        const auto dimTypes(query->schema().pdalLayout().dimTypes());
        compressor = entwine::makeUnique<Compressor>(stream, dimTypes);
    }

    Chunker<Res> chunker(res, m_manager.headers());
    auto& data(chunker.data());

    uint32_t points(0);

    {
        query->run();
        data = std::move(query->data());

        if (compressor)
        {
            compressor->compress(data.data(), data.size());
            if (query->done()) compressor->done();
            data = std::move(stream.data());
        }

        points = query->numPoints();
        const char* pos(reinterpret_cast<const char*>(&points));
        data.insert(data.end(), pos, pos + sizeof(uint32_t));

        chunker.write(true);
    }

    /*
    while (!query->done() && !chunker.canceled())
    {
        query->next();
        data = std::move(query->data());

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
    */

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
    if (chunker.canceled()) std::cout << " " << color("canceled", Color::Red);

    std::cout << std::endl;
}
/*
template<typename Req, typename Res>
void Resource::read(Req& req, Res& res)
{
    const auto start(getNow());

    Json::Value q(parseQuery(req));

    if (q.isMember("depthBegin") && q.isMember("depthEnd"))
    {
        if (q["depthBegin"].asUInt64() == 0 && q["depthEnd"].asUInt64() == 30)
        {
            q["depthEnd"] = Json::UInt64(1);
        }
    }

    if (!q.isMember("schema")) q["schema"] = getInfo()["schema"];

    using Compressor = pdal::LazPerfCompressor<Stream>;
    Stream stream;
    std::unique_ptr<Compressor> compressor;
    if (q.isMember("compress") && q["compress"].asBool())
    {
        const entwine::Schema schema(q["schema"]);
        const auto dimTypes(schema.pdalLayout().dimTypes());
        compressor = entwine::makeUnique<Compressor>(stream, dimTypes);
    }

    Chunker<Res> chunker(res, m_manager.headers());
    auto& data(chunker.data());

    bool done(false);
    uint32_t points(0);

    for (std::size_t i(0); i < m_readers.size(); ++i)
    {
        TimedReader* reader(m_readers.at(i));
        auto query(reader->get()->getQuery(q));

        while (!query->done() && !chunker.canceled())
        {
            query->next();
            done = (i == m_readers.size() - 1) && query->done();

            auto& qdata(query->data());
            if (data.empty()) data = std::move(qdata);
            else data.insert(data.end(), qdata.begin(), qdata.end());
            qdata.clear();

            if (compressor)
            {
                compressor->compress(data.data(), data.size());
                if (done) compressor->done();
                data = std::move(stream.data());
            }

            if (query->done()) points += query->numPoints();

            if (done)
            {
                const char* pos(reinterpret_cast<const char*>(&points));
                data.insert(data.end(), pos, pos + sizeof(uint32_t));
            }

            chunker.write(done);
        }

        if (chunker.canceled()) break;
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
    if (chunker.canceled()) std::cout << " " << color("canceled", Color::Red);

    std::cout << std::endl;
}
*/

template<typename Req, typename Res>
void Resource::count(Req& req, Res& res)
{
    const auto start(getNow());

    uint64_t points(0);
    uint64_t chunks(0);

    const Json::Value q(parseQuery(req));

    for (TimedReader* reader : m_readers)
    {
        auto query(reader->get()->getCountQuery(q));
        query->run();
        points += query->numPoints();
        chunks += query->chunks();
    }

    Json::Value result;
    result["points"] = static_cast<Json::UInt64>(points);
    result["chunks"] = static_cast<Json::UInt64>(chunks);

    auto h(m_manager.headers());
    h.emplace("Content-Type", "application/json");
    res.write(dense(result), h);

    std::lock_guard<std::mutex> lock(m);
    std::cout << m_name << "/" << color("count", Color::Cyan) << ": " <<
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

    const std::string name(q["name"].asString());
    SharedReader reader(m_readers.front()->get());

    if (q.isMember("schema"))
    {
        reader->registerAppend(name, entwine::Schema(q["schema"]));
    }

    const std::size_t size(req.content.size());

    std::vector<char> data;
    data.reserve(size);
    data.assign(
            (std::istreambuf_iterator<char>(req.content)),
            std::istreambuf_iterator<char>());
    if (data.size() != size) throw std::runtime_error("Invalid size");

    const std::size_t points(reader->write(name, data, q));

    res.write("", m_manager.headers());

    if (!points) return;

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

template void Resource::info(Http::Request&, Http::Response&);
template void Resource::hierarchy(Http::Request&, Http::Response&);
template void Resource::files(Http::Request&, Http::Response&);
template void Resource::read(Http::Request&, Http::Response&);
template void Resource::count(Http::Request&, Http::Response&);
template void Resource::write(Http::Request&, Http::Response&);

template void Resource::info(Https::Request&, Https::Response&);
template void Resource::hierarchy(Https::Request&, Https::Response&);
template void Resource::files(Https::Request&, Https::Response&);
template void Resource::read(Https::Request&, Https::Response&);
template void Resource::count(Https::Request&, Https::Response&);
template void Resource::write(Https::Request&, Https::Response&);

} // namespace greyhound

