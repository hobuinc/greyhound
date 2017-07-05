#include <fstream>
#include <iostream>

#include <json/json.h>

#include <entwine/third/arbiter/arbiter.hpp>
#include <entwine/reader/query.hpp>
#include <entwine/reader/reader.hpp>
#include <entwine/tree/clipper.hpp>
#include <entwine/types/bounds.hpp>
#include <entwine/types/delta.hpp>
#include <entwine/types/metadata.hpp>
#include <entwine/types/reprojection.hpp>
#include <entwine/types/schema.hpp>
#include <entwine/types/structure.hpp>
#include <entwine/util/executor.hpp>

#include "read-queries/entwine.hpp"
#include "types/buffer-pool.hpp"

#include "session.hpp"

namespace
{
    std::string getTypeString(const entwine::Structure& structure)
    {
        if (structure.dimensions() == 2)
        {
            if (structure.tubular()) return "octree";
            else return "quadtree";
        }
        else if (structure.dimensions() == 3 && !structure.tubular())
        {
            return "octree";
        }
        else
        {
            throw std::runtime_error("Invalid structure");
        }
    }
}

Session::Session(
        const std::string name,
        const std::vector<std::string>& paths,
        entwine::OuterScope& outerScope,
        entwine::Cache& cache)
    : m_name(name)
    , m_paths(paths)
    , m_outerScope(outerScope)
    , m_cache(cache)
{ }

Session::~Session()
{ }

bool Session::initialize()
{
    std::call_once(m_initOnce, [this]()
    {
        std::cout << "Discovering " << m_name << std::endl;

        if (resolveIndex())
        {
            std::cout << "\tIndex for " << m_name << " found" << std::endl;

            Json::Value json;
            const entwine::Metadata& metadata(m_entwine->metadata());

            const std::size_t numPoints(
                    metadata.manifest().pointStats().inserts());

            json["type"] = getTypeString(metadata.structure());
            json["numPoints"] = static_cast<Json::UInt64>(numPoints);
            json["schema"] = metadata.schema().toJson();
            json["bounds"] = metadata.boundsNativeCubic().toJson();
            json["boundsConforming"] =
                metadata.boundsNativeConforming().toJson();
            json["srs"] = metadata.srs();
            json["baseDepth"] = static_cast<Json::UInt64>(
                    metadata.structure().nullDepthEnd());

            if (const entwine::Reprojection* r = metadata.reprojection())
            {
                json["reprojection"] = r->toJson();
            }

            if (const entwine::Delta* delta = metadata.delta())
            {
                const entwine::Point& scale(delta->scale());

                if (scale.x == scale.y && scale.x == scale.z)
                {
                    json["scale"] = scale.x;
                }
                else
                {
                    json["scale"] = scale.toJsonArray();
                }

                json["offset"] = delta->offset().toJsonArray();
            }

            m_info = json;
        }
        else
        {
            std::cout << "\tBacking for " << m_name << " NOT found" <<
                std::endl;
        }
    });

    return !!m_entwine;
}

Json::Value Session::info() const
{
    check();
    return m_info;
}

Json::Value Session::hierarchy(
        const entwine::Bounds* inBounds,
        const std::size_t depthBegin,
        const std::size_t depthEnd,
        const bool vertical,
        const entwine::Scale* scale,
        const entwine::Offset* offset) const
{
    check();
    const auto& nativeBounds(m_entwine->metadata().boundsCubic());
    const entwine::Delta delta(scale, offset);
    const entwine::Bounds bounds(
            inBounds ? *inBounds : nativeBounds.deltify(delta));

    return m_entwine->hierarchy(
            bounds,
            depthBegin,
            depthEnd,
            vertical,
            scale,
            offset);
}

Json::Value Session::filesSingle(const Json::Value& in) const
{
    Json::Value result;
    if (in.isNumeric())
    {
        try { return m_entwine->files(in.asUInt64()).toJson(); }
        catch (...) { return Json::nullValue; }
    }
    else if (in.isString())
    {
        try { return m_entwine->files(in.asString()).toJson(); }
        catch (...) { return Json::nullValue; }
    }
    else if (in.isNull())
    {
        return Json::UInt64(m_entwine->metadata().manifest().size());
    }
    else
    {
        throw std::runtime_error("Invalid file query: " + in.toStyledString());
    }
}

Json::Value Session::files(const Json::Value& search) const
{
    if (search.isArray())
    {
        Json::Value result;
        for (const auto& f : search) result.append(filesSingle(f));
        return result;
    }
    else
    {
        return filesSingle(search);
    }
}

Json::Value Session::files(
        const entwine::Bounds& bounds,
        const entwine::Scale* scale,
        const entwine::Offset* offset) const
{
    const auto fileInfo(m_entwine->files(bounds, scale, offset));
    if (fileInfo.empty()) return Json::nullValue;

    const auto json(entwine::toJsonArrayOfObjects(fileInfo));
    return json.size() == 1 ? json[0] : json;
}

std::shared_ptr<ReadQuery> Session::query(
        const entwine::Schema& schema,
        const Json::Value& filter,
        const bool compress,
        const entwine::Scale* scale,
        const entwine::Offset* offset,
        const entwine::Bounds* inBounds,
        const std::size_t depthBegin,
        const std::size_t depthEnd)
{
    check();
    std::unique_ptr<entwine::Query> q;

    if (inBounds)
    {
        q = m_entwine->getQuery(
                schema,
                filter,
                *inBounds,
                depthBegin,
                depthEnd,
                scale,
                offset);
    }
    else
    {
        q = m_entwine->getQuery(
                schema,
                filter,
                depthBegin,
                depthEnd,
                scale,
                offset);
    }

    return std::shared_ptr<ReadQuery>(
            new EntwineReadQuery(compress, std::move(q)));
}

std::unique_ptr<ReadQuery> Session::getQuery(
        const entwine::Bounds* bounds,
        const std::size_t depthBegin,
        const std::size_t depthEnd,
        const entwine::Scale* scale,
        const entwine::Offset* offset,
        const entwine::Schema* inSchema,
        const Json::Value& filter,
        const bool compress) const
{
    check();
    std::unique_ptr<entwine::Query> q;

    const entwine::Schema& schema(
            inSchema ? *inSchema : m_entwine->metadata().schema());

    if (bounds)
    {
        q = m_entwine->getQuery(
                schema,
                filter,
                *bounds,
                depthBegin,
                depthEnd,
                scale,
                offset);
    }
    else
    {
        q = m_entwine->getQuery(
                schema,
                filter,
                depthBegin,
                depthEnd,
                scale,
                offset);
    }

    return entwine::makeUnique<EntwineReadQuery>(compress, std::move(q));
}

const entwine::Schema& Session::schema() const
{
    check();
    return m_entwine->metadata().schema();
}

bool Session::resolveIndex()
{
    for (std::string path : m_paths)
    {
        std::string err;

        try
        {
            if (path.size() && path.back() != '/') path.push_back('/');
            entwine::arbiter::Endpoint endpoint(
                    m_outerScope.getArbiterPtr()->getEndpoint(path + m_name));
            m_entwine.reset(new entwine::Reader(endpoint, m_cache));
        }
        catch (const std::exception& e)
        {
            err = e.what();
        }
        catch (...)
        {
            err = "unknown error";
            m_entwine.reset();
        }

        std::cout << "\tTried resolving index at " << path << ": ";
        if (m_entwine)
        {
            std::cout << "SUCCESS" << std::endl;
            break;
        }
        else
        {
            std::cout << "fail - " << err << std::endl;
        }
    }

    return !!m_entwine;
}

