#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "types/source-manager.hpp"
#include "util/once.hpp"

namespace pdal
{
    class PointContext;
    class StageFactory;
}

namespace entwine
{
    class BBox;
    class Schema;
    class Reader;
}

class RasterMeta;
class ReadQuery;
class Paths;

class Session
{
public:
    Session(const pdal::StageFactory& stageFactory);
    ~Session();

    // Returns true if initialization was successful.  If false, this session
    // should not be used.
    bool initialize(const std::string& name, const Paths& paths);

    std::size_t getNumPoints();
    std::string getStats();
    std::string getSrs();

    // Read a full unindexed data set.
    std::shared_ptr<ReadQuery> query(
            const entwine::Schema& schema,
            bool compress);

    // Read quad-tree indexed data with a bounding box query and min/max tree
    // depths to search.
    std::shared_ptr<ReadQuery> query(
            const entwine::Schema& schema,
            bool compress,
            const entwine::BBox& bbox,
            std::size_t depthBegin,
            std::size_t depthEnd);

    // Read quad-tree indexed data with depth level for rasterization.
    std::shared_ptr<ReadQuery> query(
            const entwine::Schema& schema,
            bool compress,
            const entwine::BBox& bbox,
            std::size_t rasterize,
            RasterMeta& rasterMeta);

    const entwine::Schema& schema();

private:
    bool sourced();
    bool indexed();

    bool resolveSource();
    bool resolveIndex();

    const pdal::StageFactory& m_stageFactory;

    Once m_initOnce;
    std::unique_ptr<SourceManager> m_source;
    std::unique_ptr<entwine::Reader> m_entwine;

    std::mutex m_sourceMutex;
    std::mutex m_indexMutex;

    std::string m_name;
    std::unique_ptr<Paths> m_paths;

    // Disallow copy/assignment.
    Session(const Session&);
    Session& operator=(const Session&);
};

