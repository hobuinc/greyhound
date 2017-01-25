#include <fstream>
#include <iostream>

#include <glob.h>

#include <json/json.h>

#include <pdal/StageFactory.hpp>

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
#include "util/buffer-pool.hpp"

#include "session.hpp"

namespace
{
    /*
    std::vector<std::string> resolve(
            const std::vector<std::string>& dirs,
            const std::string& name)
    {
        glob_t buffer;
        for (std::size_t i(0); i < dirs.size(); ++i)
        {
            auto flags(GLOB_NOSORT | GLOB_TILDE);
            if (i) flags |= GLOB_APPEND;

            glob((dirs[i] + "/" + name + ".*").c_str(), flags, 0, &buffer);
        }

        std::vector<std::string> results;
        for (std::size_t i(0); i < buffer.gl_pathc; ++i)
        {
            results.push_back(buffer.gl_pathv[i]);
        }

        globfree(&buffer);

        return results;
    }
    */

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
        pdal::StageFactory& stageFactory,
        std::mutex& factoryMutex)
    : m_initOnce()
    , m_entwine()
{ }

Session::~Session()
{ }

bool Session::initialize(
        const std::string& name,
        std::vector<std::string> paths,
        entwine::OuterScope& outerScope,
        std::shared_ptr<entwine::Cache> cache)
{
    m_initOnce.ensure([this, &name, &paths, &cache, &outerScope]()
    {
        std::cout << "Discovering " << name << std::endl;

        if (resolveIndex(name, paths, outerScope, cache))
        {
            std::cout << "\tIndex for " << name << " found" << std::endl;

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
            std::cout << "\tBacking for " << name << " NOT found" << std::endl;
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
        const entwine::Bounds& bounds,
        const std::size_t depthBegin,
        const std::size_t depthEnd,
        const bool vertical,
        const entwine::Scale* scale,
        const entwine::Offset* offset) const
{
    check();
    return m_entwine->hierarchy(
            bounds,
            depthBegin,
            depthEnd,
            vertical,
            scale,
            offset);
}

Json::Value Session::filesSingle(
        const Json::Value& in,
        const entwine::Scale* scale,
        const entwine::Offset* offset) const
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
    else
    {
        throw std::runtime_error("Invalid file query: " + in.toStyledString());
    }
}

Json::Value Session::files(
        const Json::Value& in,
        const entwine::Scale* scale,
        const entwine::Offset* offset) const
{
    Json::Value result;
    if (in.isArray())
    {
        for (const auto& f : in)
        {
            const auto current(filesSingle(f, scale, offset));
            result.append(current);
        }

        return result.size() == 1 ? result[0] : result;
    }
    else if (in.isObject() && in.isMember("bounds"))
    {
        const entwine::Bounds b(in["bounds"]);
        const auto fileInfo(m_entwine->files(b, scale, offset));
        if (fileInfo.size() == 1)
        {
            result = fileInfo.front().toJson();
        }
        else if (fileInfo.size() > 1)
        {
            for (const auto& f : fileInfo)
            {
                result.append(f.toJson());
            }
        }
        else return Json::nullValue;
    }
    else
    {
        return filesSingle(in, scale, offset);
    }

    return result;
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
            new EntwineReadQuery(schema, compress, std::move(q)));
}

const entwine::Schema& Session::schema() const
{
    check();
    return m_entwine->metadata().schema();
}

bool Session::resolveIndex(
        const std::string& name,
        const std::vector<std::string>& paths,
        entwine::OuterScope& outerScope,
        std::shared_ptr<entwine::Cache> cache)
{
    for (std::string path : paths)
    {
        std::string err;

        try
        {
            if (path.size() && path.back() != '/') path.push_back('/');
            entwine::arbiter::Endpoint endpoint(
                    outerScope.getArbiterPtr()->getEndpoint(path + name));
            m_entwine.reset(new entwine::Reader(endpoint, *cache));
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

