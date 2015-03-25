#include <fstream>
#include <iostream>

#include <glob.h>   // TODO

#include <entwine/types/bbox.hpp>
#include <entwine/types/schema.hpp>
#include <entwine/tree/multi-batcher.hpp>
#include <entwine/tree/sleepy-tree.hpp>

#include "buffer-pool.hpp"
#include "read-queries/entwine.hpp"
#include "types/paths.hpp"
#include "pdal-session.hpp"

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

PdalSession::PdalSession()
    : m_initOnce()
    , m_source()
    , m_tree()
    , m_name()
    , m_paths()
{ }

PdalSession::~PdalSession()
{ }

bool PdalSession::initialize(const std::string& name, const Paths& paths)
{
    std::cout << "Discovering " << name << std::endl;

    m_initOnce.ensure([this, &name, &paths]()
    {
        m_name = name;
        m_paths.reset(new Paths(paths));

        resolveSource();
        resolveIndex();
    });

    return (sourced() || indexed());
}

std::size_t PdalSession::getNumPoints()
{
    return m_tree->numPoints();
}

std::string PdalSession::getSchemaString()
{
    // TODO Return the JSON, let the bindings turn it into a JS object.
    return m_tree->schema().toJson().toStyledString();
}

std::string PdalSession::getStats()
{
    // TODO
    return "{ }";
}

std::string PdalSession::getSrs()
{
    return "";
}

std::shared_ptr<ReadQuery> PdalSession::queryUnindexed(
        const entwine::Schema& schema,
        bool compress,
        std::size_t start,
        std::size_t count)
{
    // TODO If sourced() is not true here, attempt to re-resolve a path for
    // this resource.  Perhaps it was added later.
    throw std::runtime_error("TODO - PdalSession::queryUnindexed");
    return std::shared_ptr<ReadQuery>();
}

std::shared_ptr<ReadQuery> PdalSession::query(
        const entwine::Schema& schema,
        bool compress,
        const entwine::BBox& bbox,
        std::size_t depthBegin,
        std::size_t depthEnd)
{
    std::vector<std::size_t> results(m_tree->query(bbox, depthBegin, depthEnd));
    return std::shared_ptr<ReadQuery>(
            new EntwineReadQuery(
                schema,
                compress,
                false,
                *m_tree.get(),
                results));
}

std::shared_ptr<ReadQuery> PdalSession::query(
        const entwine::Schema& schema,
        bool compress,
        std::size_t depthBegin,
        std::size_t depthEnd)
{
    std::vector<std::size_t> results(m_tree->query(depthBegin, depthEnd));
    return std::shared_ptr<ReadQuery>(
            new EntwineReadQuery(
                schema,
                compress,
                false,
                *m_tree.get(),
                results));
}

std::shared_ptr<ReadQuery> PdalSession::query(
        const entwine::Schema& schema,
        bool compress,
        std::size_t rasterize,
        RasterMeta& rasterMeta)
{
    throw std::runtime_error("TODO - PdalSession::query (rastered)");
    return std::shared_ptr<ReadQuery>();
}

std::shared_ptr<ReadQuery> PdalSession::query(
        const entwine::Schema& schema,
        bool compress,
        const RasterMeta& rasterMeta)
{
    throw std::runtime_error("TODO - PdalSession::query (rastered)");
    return std::shared_ptr<ReadQuery>();
}

const entwine::Schema& PdalSession::schema() const
{
    return m_tree->schema();
}

bool PdalSession::resolveSource()
{
    if (!sourced())
    {
        const auto sources(resolve(m_paths->inputs, m_name));

        if (sources.size() > 1)
        {
            std::cout << "Found competing sources for " << m_name << std::endl;
            std::cout << "\tUsing: " << sources[0] << std::endl;
        }

        if (sources.size())
        {
            m_source = sources[0];
        }
    }

    return sourced();
}

bool PdalSession::resolveIndex()
{
    if (!indexed())
    {
        try
        {
            m_tree.reset(
                    new entwine::SleepyTree(m_paths->output + "/" + m_name));
        }
        catch (...)
        {
            m_tree.reset();
        }
    }

    return indexed();
}

