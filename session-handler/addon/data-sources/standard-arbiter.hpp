#pragma once

#include <mutex>

#include "data-sources/live.hpp"
#include "data-sources/serial.hpp"
#include "types/serial-paths.hpp"
#include "arbiter.hpp"

class StandardArbiter : public Arbiter
{
public:
    // If the constructor throws, then this may not be used.
    StandardArbiter(
            const std::string& pipelineId,
            const std::string& filename,
            bool serialCompress,
            const SerialPaths& serialPaths);
    ~StandardArbiter() { }

    virtual std::size_t getNumPoints() const;
    virtual std::string getSchema() const;
    virtual std::string getStats() const;
    virtual std::string getSrs() const;
    virtual std::vector<std::size_t> getFills() const;

    virtual const pdal::PointContext& pointContext() const;

    virtual std::shared_ptr<ReadQuery> queryUnindexed(
            const Schema& schema,
            bool compress,
            std::size_t start,
            std::size_t count);

    virtual std::shared_ptr<ReadQuery> query(
            const Schema& schema,
            bool compress,
            const BBox& bbox,
            std::size_t depthBegin,
            std::size_t depthEnd);

    virtual std::shared_ptr<ReadQuery> query(
            const Schema& schema,
            bool compress,
            std::size_t depthBegin,
            std::size_t depthEnd);

    virtual std::shared_ptr<ReadQuery> query(
            const Schema& schema,
            bool compress,
            std::size_t rasterize,
            RasterMeta& rasterMeta);

    virtual std::shared_ptr<ReadQuery> query(
            const Schema& schema,
            bool compress,
            const RasterMeta& rasterMeta);

    virtual std::shared_ptr<ReadQuery> query(
            const Schema& schema,
            bool compress,
            bool is3d,
            double radius,
            double x,
            double y,
            double z);

private:
    // Returns true if successfully created.
    bool awaken();      // Try to wake up serialized source.
    bool enliven();     // Try to wake up live source.
    bool serialize();

    const std::string m_pipelineId;
    const std::string m_filename;
    const bool m_serialCompress;
    const SerialPaths m_serialPaths;

    std::mutex m_mutex;     // Guards post-ctor awakening of data sources.

    std::shared_ptr<SerialDataSource> m_serialDataSource;
    std::shared_ptr<LiveDataSource> m_liveDataSource;
};

