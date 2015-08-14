#include <fstream>
#include <iostream>

#include <glob.h>   // TODO

#include <pdal/StageFactory.hpp>

#include <entwine/third/arbiter/arbiter.hpp>
#include <entwine/reader/query.hpp>
#include <entwine/reader/reader.hpp>
#include <entwine/types/bbox.hpp>
#include <entwine/types/schema.hpp>
#include <entwine/types/structure.hpp>
#include <entwine/tree/clipper.hpp>

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

            glob((dirs[i] + "/" + name + "*").c_str(), flags, 0, &buffer);
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
    , m_name()
    , m_paths()
    , m_arbiter()
    , m_cache()
{ }

Session::~Session()
{ }

bool Session::initialize(
        const std::string& name,
        const std::vector<std::string>& paths,
        std::shared_ptr<arbiter::Arbiter> arbiter,
        std::shared_ptr<entwine::Cache> cache)
{
    m_initOnce.ensure(
            [this, &name, &paths, cache, arbiter]()
    {
        std::cout << "Discovering " << name << std::endl;

        m_name = name;
        m_paths = paths;
        m_arbiter = arbiter;
        m_cache = cache;

        m_paths.push_back("s3://");

        resolveSource();
        resolveIndex();

        std::cout << "\n\tSource for " << name << ": " <<
            (sourced() ? "FOUND" : "NOT found") << std::endl;

        std::cout << "\tIndex for " << name << ": " <<
            (indexed() ? "FOUND" : "NOT found") << "\n" << std::endl;
    });

    return sourced() || indexed();
}

std::size_t Session::getNumPoints()
{
    if (indexed()) return m_entwine->numPoints();
    else return m_source->numPoints();
}

std::string Session::getStats()
{
    // TODO
    return "{ }";
}

std::string Session::getSrs()
{
    return "";
}

std::string Session::getType()
{
    if (resolveIndex())
    {
        return m_entwine->structure().dimensions() == 2 ? "quadtree" : "octree";
    }
    else
    {
        throw std::runtime_error("No index found for " + m_name);
    }
}

entwine::BBox Session::getBounds()
{
    if (resolveIndex())
    {
        return m_entwine->bbox();
    }
    else
    {
        throw std::runtime_error("No index found for " + m_name);
    }
}

std::shared_ptr<ReadQuery> Session::query(
        const entwine::Schema& schema,
        const bool compress)
{
    if (resolveSource())
    {
        return std::shared_ptr<ReadQuery>(
                new UnindexedReadQuery(
                    schema,
                    compress,
                    *m_source));
    }
    else
    {
        return std::shared_ptr<ReadQuery>();
    }
}

std::shared_ptr<ReadQuery> Session::query(
        const entwine::Schema& schema,
        bool compress,
        const entwine::BBox& bbox,
        std::size_t depthBegin,
        std::size_t depthEnd)
{
    if (resolveIndex())
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
        return std::shared_ptr<ReadQuery>();
    }
}

const entwine::Schema& Session::schema()
{
    if (indexed()) return m_entwine->schema();
    else return m_source->schema();
}

bool Session::sourced()
{
    std::lock_guard<std::mutex> lock(m_sourceMutex);
    return m_source.get() != 0;
}

bool Session::indexed()
{
    std::lock_guard<std::mutex> lock(m_indexMutex);
    return m_entwine.get() != 0;
}

bool Session::resolveSource()
{
    // TODO For now only works for local paths.  Support any Source the Arbiter
    // contains.
    std::lock_guard<std::mutex> lock(m_sourceMutex);
    if (!m_source)
    {
        const auto sources(resolve(m_paths, m_name));

        if (sources.size() > 1)
        {
            std::cout << "\tFound competing unindexed sources for " << m_name <<
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
    }

    return m_source.get() != 0;
}

bool Session::resolveIndex()
{
    std::lock_guard<std::mutex> lock(m_indexMutex);
    if (!m_entwine)
    {
        for (std::string path : m_paths)
        {
            try
            {
                if (path.size() && path.back() != '/') path.push_back('/');

                arbiter::Endpoint endpoint(
                        m_arbiter->getEndpoint(path + m_name));

                m_entwine.reset(
                        new entwine::Reader(endpoint, *m_arbiter, m_cache));
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
    }

    return m_entwine.get() != 0;
}

