#include <greyhound/resource.hpp>

#include <regex>

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

namespace
{
    std::string dense(const Json::Value& json)
    {
        auto s = Json::FastWriter().write(json);
        s.pop_back();
        return s;
    }

    const std::regex onlyNumeric("^\\d+$");
}

namespace greyhound
{

void Resource::info(Req& req, Res& res)
{
    Json::Value json;
    const auto& m(m_reader->metadata());

    json["type"] = "octree";
    json["numPoints"] = Json::UInt64(m.manifest().pointStats().inserts());
    json["schema"] = m.schema().toJson();
    json["bounds"] = m.boundsNativeCubic().toJson();
    json["boundsConforming"] = m.boundsNativeConforming().toJson();
    json["srs"] = m.srs();
    json["baseDepth"] = Json::UInt64(m.structure().baseDepthBegin());

    if (const auto r = m.reprojection()) json["reprojection"] = r->toJson();
    if (m.density()) json["density"] = m.density();
    if (const auto d = m.delta()) d->insertInto(json);

    auto h(m_manager.headers());
    h.emplace("Content-Type", "application/json");
    res.write(json.toStyledString(), h);
}

void Resource::hierarchy(Req& req, Res& res)
{
    const Json::Value result(m_reader->hierarchy(parseQuery(req)));
    auto h(m_manager.headers());
    h.emplace("Content-Type", "application/json");
    res.write(dense(result), h);
}

void Resource::files(Req& req, Res& res)
{
    auto h(m_manager.headers());
    h.emplace("Content-Type", "application/json");

    std::string root(req.path_match[2]);
    Json::Value query(parseQuery(req));

    if (root.size())
    {
        // Transform /files/42 -> /files?search=42
        if (query.isNull())
        {
            if (std::regex_match(root, onlyNumeric))
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
}

void Resource::read(Req& req, Res& res)
{
    const Json::Value params(parseQuery(req));
    auto query(m_reader->getQuery(params));

    using Compressor = pdal::LazPerfCompressor<Stream>;
    Stream stream;
    std::unique_ptr<Compressor> compressor;
    if (params["compress"].asBool())
    {
        compressor = entwine::makeUnique<Compressor>(
                stream,
                query->schema().pdalLayout().dimTypes());
    }

    Chunker chunker(res, m_manager.headers());
    auto& data(chunker.data());

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
            const uint32_t points(query->numPoints());
            const char* pos(reinterpret_cast<const char*>(&points));
            data.insert(data.end(), pos, pos + sizeof(uint32_t));
        }

        chunker.write(query->done());
    }
}

SharedResource Resource::create(const Manager& manager, const std::string name)
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
                return std::make_shared<Resource>(manager, std::move(r));
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

Json::Value Resource::parseQuery(Req& req) const
{
    Json::Value q;
    for (const auto& p : req.parse_query_string())
    {
        q[p.first] = entwine::parse(p.second);
    }
    return q;
}

} // namespace greyhound

