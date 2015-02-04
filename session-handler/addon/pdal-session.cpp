#include <iostream>

#include <pdal/PointBuffer.hpp>
#include <pdal/PointContext.hpp>

#include "buffer-pool.hpp"
#include "read-queries/base.hpp"
#include "pdal-session.hpp"

PdalSession::PdalSession()
    : m_initOnce()
    , m_arbiter()
{ }

void PdalSession::initialize(
        const std::string& pipelineId,
        const std::string& filename,
        const bool serialCompress,
        const SerialPaths& serialPaths)
{
    m_initOnce.ensure([
            this,
            &pipelineId,
            &filename,
            serialCompress,
            &serialPaths]()
    {
        try
        {
            m_arbiter.reset(new Arbiter(
                    pipelineId,
                    filename,
                    serialCompress,
                    serialPaths));
        }
        catch (...)
        {
            m_arbiter.reset();

            // Throw here so that Once::await() will return an error
            // indication so we won't use this arbiter.
            throw std::runtime_error(
                    "Caught exception in init - " + pipelineId);
        }
    });
}

void PdalSession::initialize(
        const std::string& pipelineId,
        const std::vector<std::string>& paths,
        const BBox& bbox,
        const bool serialCompress,
        const SerialPaths& serialPaths)
{
    m_initOnce.ensure([
            this,
            &pipelineId,
            &paths,
            &bbox,
            serialCompress,
            &serialPaths]()
    {
        // TODO Initialize multi-arbiter.
    });
}

std::size_t PdalSession::getNumPoints()
{
    check();
    return m_arbiter->getNumPoints();
}

std::string PdalSession::getSchema()
{
    check();
    return m_arbiter->getSchema();
}

std::string PdalSession::getStats()
{
    check();
    return m_arbiter->getStats();
}

std::string PdalSession::getSrs()
{
    check();
    return m_arbiter->getSrs();
}

std::vector<std::size_t> PdalSession::getFills()
{
    check();
    return m_arbiter->getFills();
}

void PdalSession::serialize(const SerialPaths& serialPaths)
{
    check();
    m_arbiter->serialize();
}

std::shared_ptr<ReadQuery> PdalSession::queryUnindexed(
        const Schema& schema,
        bool compress,
        std::size_t start,
        std::size_t count)
{
    check();
    return m_arbiter->queryUnindexed(schema, compress, start, count);
}

std::shared_ptr<ReadQuery> PdalSession::query(
        const Schema& schema,
        bool compress,
        const BBox& bbox,
        std::size_t depthBegin,
        std::size_t depthEnd)
{
    check();
    return m_arbiter->query(schema, compress, bbox, depthBegin, depthEnd);
}

std::shared_ptr<ReadQuery> PdalSession::query(
        const Schema& schema,
        bool compress,
        std::size_t depthBegin,
        std::size_t depthEnd)
{
    check();
    return m_arbiter->query(schema, compress, depthBegin, depthEnd);
}

std::shared_ptr<ReadQuery> PdalSession::query(
        const Schema& schema,
        bool compress,
        std::size_t rasterize,
        RasterMeta& rasterMeta)
{
    check();
    return m_arbiter->query(schema, compress, rasterize, rasterMeta);
}

std::shared_ptr<ReadQuery> PdalSession::query(
        const Schema& schema,
        bool compress,
        const RasterMeta& rasterMeta)
{
    check();
    return m_arbiter->query(schema, compress, rasterMeta);
}

std::shared_ptr<ReadQuery> PdalSession::query(
        const Schema& schema,
        bool compress,
        bool is3d,
        double radius,
        double x,
        double y,
        double z)
{
    check();
    return m_arbiter->query(schema, compress, is3d, radius, x, y, z);
}

const pdal::PointContext& PdalSession::pointContext()
{
    check();
    return m_arbiter->pointContext();
}

void PdalSession::check()
{
    if (m_initOnce.await())
    {
        throw std::runtime_error("Not initialized!");
    }
}

