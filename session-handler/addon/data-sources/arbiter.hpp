#pragma once

#include <cstdint>
#include <mutex>
#include <vector>

#include "data-sources/live.hpp"
#include "data-sources/serial.hpp"
#include "types/serial-paths.hpp"

namespace pdal
{
    class PointContext;
    class PointBuffer;
}

class BBox;
class DataSource;
class RasterMeta;
class ReadQuery;
class Schema;

class Arbiter
{
public:
    // If the constructor throws, then this may not be used.
    Arbiter(
            const std::string& pipelineId,
            const std::string& filename,
            bool serialCompress,
            const SerialPaths& serialPaths);
    ~Arbiter() { }

    std::size_t getNumPoints() const;
    std::string getSchema() const;
    std::string getStats() const;
    std::string getSrs() const;
    std::vector<std::size_t> getFills() const;

    std::shared_ptr<ReadQuery> queryUnindexed(
            const Schema& schema,
            bool compress,
            std::size_t start,
            std::size_t count);

    std::shared_ptr<ReadQuery> query(
            const Schema& schema,
            bool compress,
            const BBox& bbox,
            std::size_t depthBegin,
            std::size_t depthEnd);

    std::shared_ptr<ReadQuery> query(
            const Schema& schema,
            bool compress,
            std::size_t depthBegin,
            std::size_t depthEnd);

    std::shared_ptr<ReadQuery> query(
            const Schema& schema,
            bool compress,
            std::size_t rasterize,
            RasterMeta& rasterMeta);

    std::shared_ptr<ReadQuery> query(
            const Schema& schema,
            bool compress,
            const RasterMeta& rasterMeta);

    std::shared_ptr<ReadQuery> query(
            const Schema& schema,
            bool compress,
            bool is3d,
            double radius,
            double x,
            double y,
            double z);

    const pdal::PointContext& pointContext() const;

    // Returns true if successfully created.
    bool serialize();   // Try to serialize live source.

private:
    // Returns true if successfully created.
    bool awaken();      // Try to wake up serialized source.
    bool enliven();     // Try to wake up live source.

    const std::string m_pipelineId;
    const std::string m_filename;
    const bool m_serialCompress;
    const SerialPaths m_serialPaths;

    std::mutex m_mutex;     // Guards post-ctor awakening of data sources.

    std::shared_ptr<SerialDataSource> m_serialDataSource;
    std::shared_ptr<LiveDataSource> m_liveDataSource;
};

