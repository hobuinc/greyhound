#include <fstream>
#include <iostream>

#include <glob.h>   // TODO

#include <pdal/StageFactory.hpp>

#include <entwine/third/arbiter/arbiter.hpp>
#include <entwine/reader/query.hpp>
#include <entwine/reader/reader.hpp>
#include <entwine/third/json/json.hpp>
#include <entwine/tree/clipper.hpp>
#include <entwine/types/bbox.hpp>
#include <entwine/types/schema.hpp>
#include <entwine/types/structure.hpp>
#include <entwine/util/executor.hpp>

#include "read-queries/entwine.hpp"
#include "read-queries/unindexed.hpp"
#include "util/buffer-pool.hpp"

#include "session.hpp"

namespace
{
    // TODO Put this somewhere else - platform dependent code.
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
        std::shared_ptr<arbiter::Arbiter> arbiter,
        std::shared_ptr<entwine::Cache> cache)
{
    m_initOnce.ensure([this, &name, &paths, cache, arbiter]()
    {
        paths.push_back("s3://");

        std::cout << "Discovering " << name << std::endl;

        if (resolveIndex(name, paths, *arbiter, cache))
        {
            std::cout << "\tIndex for " << name << " found" << std::endl;

            Json::Value json;
            const std::size_t numPoints(m_entwine->numPoints());
            const std::size_t numDimensions(
                    m_entwine->structure().dimensions());

            json["type"] = (numDimensions == 2 ? "quadtree" : "octree");
            json["numPoints"] = static_cast<Json::UInt64>(numPoints);
            json["schema"] = m_entwine->schema().toJson();
            json["bounds"] = m_entwine->bbox().toJson()["bounds"];
            json["srs"] = m_entwine->srs();

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
            json["bounds"] = m_source->bbox().toJson()["bounds"];
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
        const bool compress,
        const entwine::BBox& bbox,
        const std::size_t depthBegin,
        const std::size_t depthEnd)
{
    if (indexed())
    {
        return std::shared_ptr<ReadQuery>(
                new EntwineReadQuery(
                    schema,
                    compress,
                    m_entwine->query(
                        schema,
                        bbox.exists() ? bbox : m_entwine->bbox(),
                        depthBegin,
                        depthEnd)));
    }
    else
    {
        throw WrongQueryType();
    }
}

const entwine::Schema& Session::schema() const
{
    check();

    if (indexed()) return m_entwine->schema();
    else return m_source->schema();
}

bool Session::resolveIndex(
        const std::string& name,
        const std::vector<std::string>& paths,
        arbiter::Arbiter& arbiter,
        std::shared_ptr<entwine::Cache> cache)
{
    for (std::string path : paths)
    {
        try
        {
            if (path.size() && path.back() != '/') path.push_back('/');
            arbiter::Endpoint endpoint(arbiter.getEndpoint(path + name));
            m_entwine.reset(new entwine::Reader(endpoint, arbiter, *cache));
        }
        catch (...)
        {
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
            std::cout << "fail" << std::endl;
        }
    }

    return indexed();
}

bool Session::resolveSource(
        const std::string& name,
        const std::vector<std::string>& paths)
{
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
}

