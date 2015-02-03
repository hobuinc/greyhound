#pragma once

#include <vector>
#include <string>

#include <pdal/PipelineManager.hpp>

#include "once.hpp"
#include "pdal-index.hpp"
#include "data-sources/base.hpp"

class BBox;
class PdalIndex;
class RasterMeta;
class ReadQuery;
class SerialPaths;
class Schema;

class LiveDataSource : DataSource
{
public:
    LiveDataSource(
            const std::string& pipelineId,
            const std::string& filename);
    ~LiveDataSource() { }

    void serialize(bool compress, const SerialPaths& serialPaths);

    virtual std::size_t getNumPoints() const;
    virtual std::string getSchema() const;
    virtual std::string getStats() const;
    virtual std::string getSrs() const;
    virtual std::vector<std::size_t> getFills() const;

    virtual std::shared_ptr<ReadQuery> queryUnindexed(
            const Schema& schema,
            bool compressed,
            std::size_t start,
            std::size_t count);

    virtual std::shared_ptr<ReadQuery> query(
            const Schema& schema,
            bool compressed,
            const BBox& bbox,
            std::size_t depthBegin,
            std::size_t depthEnd);

    virtual std::shared_ptr<ReadQuery> query(
            const Schema& schema,
            bool compressed,
            std::size_t depthBegin,
            std::size_t depthEnd);

    virtual std::shared_ptr<ReadQuery> query(
            const Schema& schema,
            bool compressed,
            std::size_t rasterize,
            RasterMeta& rasterMeta);

    virtual std::shared_ptr<ReadQuery> query(
            const Schema& schema,
            bool compressed,
            const RasterMeta& rasterMeta);

    virtual std::shared_ptr<ReadQuery> query(
            const Schema& schema,
            bool compressed,
            bool is3d,
            double radius,
            double x,
            double y,
            double z);

    const pdal::PointContext& pointContext() const
    {
        return m_pointContext;
    }

    std::shared_ptr<pdal::PointBuffer> pointBuffer() const
    {
        return m_pointBuffer;
    }

private:
    void exec(std::string filename);
    void ensureIndex(PdalIndex::IndexType indexType);

    const std::string m_pipelineId;

    pdal::PipelineManager m_pipelineManager;
    std::shared_ptr<pdal::PointBuffer> m_pointBuffer;
    pdal::PointContext m_pointContext;

    Once m_serializeOnce;

    std::shared_ptr<PdalIndex> m_pdalIndex;
};

