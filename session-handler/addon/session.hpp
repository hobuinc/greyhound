#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "util/once.hpp"

namespace pdal
{
    class PointContext;
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
    Session();
    ~Session();

    // Returns true if initialization was successful.  If false, this session
    // should not be used.
    bool initialize(const std::string& name, const Paths& paths);

    // Queries.
    std::size_t getNumPoints();
    std::string getSchemaString();
    std::string getStats();
    std::string getSrs();

    // Read un-indexed data with an offset and a count.
    std::shared_ptr<ReadQuery> queryUnindexed(
            const entwine::Schema& schema,
            bool compress,
            std::size_t start,
            std::size_t count);

    // Read quad-tree indexed data with a bounding box query and min/max tree
    // depths to search.
    std::shared_ptr<ReadQuery> query(
            const entwine::Schema& schema,
            bool compress,
            const entwine::BBox& bbox,
            std::size_t depthBegin,
            std::size_t depthEnd);

    // Read quad-tree indexed data with min/max tree depths to search.
    std::shared_ptr<ReadQuery> query(
            const entwine::Schema& schema,
            bool compress,
            std::size_t depthBegin,
            std::size_t depthEnd);

    // Read quad-tree indexed data with depth level for rasterization.
    std::shared_ptr<ReadQuery> query(
            const entwine::Schema& schema,
            bool compress,
            std::size_t rasterize,
            RasterMeta& rasterMeta);

    // Read a bounded set of points into a raster of pre-determined resolution.
    std::shared_ptr<ReadQuery> query(
            const entwine::Schema& schema,
            bool compress,
            const RasterMeta& rasterMeta);

    const entwine::Schema& schema() const;

private:
    bool sourced() const { return m_source.size(); }    // Have data source?
    bool indexed() const { return !!m_entwine; }        // Have index?

    bool resolveSource();
    bool resolveIndex();

    Once m_initOnce;
    std::string m_source;
    std::unique_ptr<entwine::Reader> m_entwine;

    std::string m_name;
    std::unique_ptr<Paths> m_paths;

    // Disallow copy/assignment.
    Session(const Session&);
    Session& operator=(const Session&);
};

