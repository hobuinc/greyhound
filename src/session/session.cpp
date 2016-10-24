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
#include <entwine/types/format.hpp>
#include <entwine/types/metadata.hpp>
#include <entwine/types/schema.hpp>
#include <entwine/types/structure.hpp>
#include <entwine/util/executor.hpp>

#include "read-queries/entwine.hpp"
#include "read-queries/unindexed.hpp"
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
    : m_stageFactory(stageFactory)
    , m_factoryMutex(factoryMutex)
    , m_initOnce()
    , m_source()
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
            json["bounds"] = metadata.bounds().toJson();
            json["boundsConforming"] = metadata.boundsConforming().toJson();
            json["boundsNative"] = metadata.boundsNative().toJson();
            json["srs"] = metadata.format().srs();
            json["baseDepth"] = static_cast<Json::UInt64>(
                    metadata.structure().nullDepthEnd());

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

            m_info = json.toStyledString();
        }
        else if (resolveSource(name, paths))
        {
            std::cout << "\tSource for " << name << " found" << std::endl;
            const std::size_t numPoints(m_source->numPoints());

            Json::Value json;
            json["type"] = "unindexed";
            json["numPoints"] = static_cast<Json::UInt64>(numPoints);
            json["schema"] = m_source->schema().toJson();
            json["bounds"] = m_source->bounds().toJson();
            json["srs"] = m_source->srs();

            m_info = json.toStyledString();
        }
        else
        {
            std::cout << "\tBacking for " << name << " NOT found" << std::endl;
        }
    });

    return sourced() || indexed();
}

std::string Session::info() const
{
    check();
    return m_info;
}

std::string Session::hierarchy(
        const entwine::Bounds& bounds,
        const std::size_t depthBegin,
        const std::size_t depthEnd,
        const bool vertical,
        const entwine::Scale* scale,
        const entwine::Offset* offset) const
{
    if (indexed())
    {
        Json::FastWriter writer;
        return writer.write(
                m_entwine->hierarchy(
                    bounds,
                    depthBegin,
                    depthEnd,
                    vertical,
                    scale,
                    offset));
    }
    else
    {
        throw std::runtime_error("Cannot get hierarchy from unindexed dataset");
    }
}

std::shared_ptr<ReadQuery> Session::query(
        const entwine::Schema& schema,
        const bool compress)
{
    if (sourced())
    {
        return std::shared_ptr<ReadQuery>(
                new UnindexedReadQuery(
                    schema,
                    compress,
                    *m_source));
    }
    else
    {
        throw WrongQueryType();
    }
}

std::shared_ptr<ReadQuery> Session::query(
        const entwine::Schema& schema,
        const Json::Value& filter,
        const bool compress,
        const entwine::Point* scale,
        const entwine::Point* offset,
        const entwine::Bounds* inBounds,
        const std::size_t depthBegin,
        const std::size_t depthEnd)
{
    if (indexed())
    {
        std::unique_ptr<entwine::Query> q;

        if (inBounds)
        {
            q = m_entwine->query(
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
            q = m_entwine->query(
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
    else
    {
        throw WrongQueryType();
    }
}

const entwine::Schema& Session::schema() const
{
    check();

    if (indexed()) return m_entwine->metadata().schema();
    else return m_source->schema();
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
        catch (const std::runtime_error& e)
        {
            err = e.what();
        }
        catch (...)
        {
            err = "unknown error";
            m_entwine.reset(0);
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

    return indexed();
}

bool Session::resolveSource(
        const std::string& name,
        const std::vector<std::string>& paths)
{
    return false;
    /*
    const auto sources(resolve(paths, name));

    if (sources.size() > 1)
    {
        std::cout << "\tFound competing unindexed sources for " << name <<
            std::endl;
    }

    if (sources.size())
    {
        std::size_t i(0);

        while (!m_source && i < sources.size())
        {
            const std::string path(sources[i++]);

            std::unique_lock<std::mutex> lock(m_factoryMutex);
            const std::string driver(
                    m_stageFactory.inferReaderDriver(path));
            lock.unlock();

            if (driver.size())
            {
                try
                {
                    m_source.reset(
                            new SourceManager(
                                m_stageFactory,
                                m_factoryMutex,
                                path,
                                driver));
                }
                catch (...)
                {
                    m_source.reset(0);
                }
            }

            std::cout << "\tTried resolving unindexed at " << path << ": ";
            if (m_source) std::cout << "SUCCESS" << std::endl;
            else std::cout << "failed" << std::endl;
        }
    }

    return sourced();
    */
}

