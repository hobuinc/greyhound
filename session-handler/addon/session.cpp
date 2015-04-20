#include <fstream>
#include <iostream>

#include <glob.h>   // TODO

#include <entwine/types/bbox.hpp>
#include <entwine/types/schema.hpp>
#include <entwine/tree/branches/clipper.hpp>
#include <entwine/tree/reader.hpp>

#include "read-queries/entwine.hpp"
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

Session::Session()
    : m_initOnce()
    , m_source()
    , m_entwine()
    , m_name()
    , m_paths()
{ }

Session::~Session()
{ }

bool Session::initialize(const std::string& name, const Paths& paths)
{
    m_initOnce.ensure([this, &name, &paths]()
    {
        std::cout << "Discovering " << name << std::endl;

        m_name = name;
        m_paths.reset(new Paths(paths));

        resolveSource();
        resolveIndex();
    });

    return (sourced() || indexed());
}

std::size_t Session::getNumPoints()
{
    return m_entwine->numPoints();
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

std::shared_ptr<ReadQuery> Session::query(
        const entwine::Schema& schema,
        const bool compress)
{
    throw std::runtime_error("TODO - Session::queryUnindexed");

    if (resolveSource())
    {
        // TODO Contents.
        return std::shared_ptr<ReadQuery>();
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
        std::vector<std::size_t> results(
                m_entwine->query(
                    bbox.exists() ? bbox : m_entwine->bbox(),
                    depthBegin,
                    depthEnd));

        return std::shared_ptr<ReadQuery>(
                new EntwineReadQuery(
                    schema,
                    compress,
                    false,
                    *m_entwine,
                    results));
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

const entwine::Schema& Session::schema() const
{
    return m_entwine->schema();
}

bool Session::resolveSource()
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

bool Session::resolveIndex()
{
    if (!indexed())
    {
        try
        {
            entwine::S3Info s3Info(
                m_paths->s3Info.baseAwsUrl,
                m_name,
                m_paths->s3Info.awsAccessKeyId,
                m_paths->s3Info.awsSecretAccessKey);

            std::cout << "Making reader: " << m_name << std::endl;
            m_entwine.reset(new entwine::Reader(s3Info, 128));
        }
        catch (...)
        {
            std::cout << "Couldn't find index: " << m_name << std::endl;
            m_entwine.reset();
        }
    }

    return indexed();
}

