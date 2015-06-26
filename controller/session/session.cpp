#include <fstream>
#include <iostream>

#include <glob.h>   // TODO

#include <pdal/StageFactory.hpp>

#include <entwine/drivers/arbiter.hpp>
#include <entwine/reader/query.hpp>
#include <entwine/reader/reader.hpp>
#include <entwine/types/bbox.hpp>
#include <entwine/types/schema.hpp>
#include <entwine/tree/clipper.hpp>

#include "read-queries/entwine.hpp"
#include "read-queries/unindexed.hpp"
#include "types/paths.hpp"
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
        const Paths& paths,
        std::shared_ptr<entwine::Arbiter> arbiter,
        std::shared_ptr<entwine::Cache> cache)
{
    m_initOnce.ensure(
            [this, &name, &paths, cache, arbiter]()
    {
        std::cout << "Discovering " << name << std::endl;

        m_name = name;
        m_paths.reset(new Paths(paths));
        m_arbiter = arbiter;
        m_cache = cache;

        resolveSource();
        resolveIndex();

        std::cout << "Source for " << name << ": " <<
            (sourced() ? "FOUND" : "NOT found") << std::endl;

        std::cout << "Index for " << name << ": " <<
            (indexed() ? "FOUND" : "NOT found") << std::endl;
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
                    false,
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

std::shared_ptr<ReadQuery> Session::query(
        const entwine::Schema& schema,
        bool compress,
        const entwine::BBox& bbox,
        std::size_t rasterize,
        RasterMeta& rasterMeta)
{
    throw std::runtime_error("TODO - Session::query (rastered)");
    return std::shared_ptr<ReadQuery>();
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
        const auto sources(resolve(m_paths->inputs, m_name));

        if (sources.size() > 1)
        {
            std::cout << "Found competing sources for " << m_name << std::endl;
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
                        std::cout << "\tNo success with: " << path << std::endl;
                        m_source.reset(0);
                    }
                }
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
        std::vector<std::string> searchPaths(m_paths->inputs);
        searchPaths.push_back(m_paths->output);
        searchPaths.push_back("s3://");

        for (std::string path : searchPaths)
        {
            try
            {
                if (path.size() && path.back() != '/') path.push_back('/');

                entwine::Source source(m_arbiter->getSource(path + m_name));

                m_entwine.reset(
                        new entwine::Reader(source, *m_arbiter, m_cache));
            }
            catch (std::runtime_error& e)
            {
                std::cout << "\tTried " << path << ": " <<
                    e.what() << "\n" << std::endl;
            }
            catch (...)
            {
                m_entwine.reset(0);
            }

            if (m_entwine) break;
        }
    }

    return m_entwine.get() != 0;
}

