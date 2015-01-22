#pragma once

#include <string>
#include <mutex>

#include <sqlite3.h>

#include <pdal/QuadIndex.hpp>

#include "grey-reader-types.hpp"
#include "grey-common.hpp"

class RasterMeta;

class GreyReader
{
public:
    GreyReader(std::string pipelineId, const SerialPaths& serialPaths);
    ~GreyReader();

    static bool exists(
            const std::string pipelineId,
            const SerialPaths& serialPaths);

    std::size_t getNumPoints() const            { return m_meta.numPoints;  }
    std::string getSchema() const               { return m_meta.schema;     }
    std::string getStats() const                { return m_meta.stats;      }
    std::string getSrs() const                  { return m_meta.srs;        }
    std::vector<std::size_t> getFills() const   { return m_meta.fills;      }

    GreyQuery query(std::size_t depthBegin, std::size_t depthEnd);

    GreyQuery query(
            double xMin,
            double yMin,
            double xMax,
            double yMax,
            std::size_t depthBegin,
            std::size_t depthEnd);

    GreyQuery query(std::size_t rasterize, RasterMeta& rasterMeta);

    const pdal::PointContext& pointContext() const { return m_pointContext; }

private:
    sqlite3* m_db;
    GreyMeta m_meta;
    std::unique_ptr<IdIndex> m_idIndex;

    pdal::PointContext m_pointContext;

    std::mutex m_cacheMutex;
    std::map<uint64_t, std::shared_ptr<GreyCluster>> m_cache;

    void readMeta();

    // Processes a collection of node info.
    //      1 - Identify which nodes are already live in the cache.
    //          1a - Quad-index these nodes if it's necessary for this query.
    //      2 - Return a comma-separated string of the node IDs that are not
    //              yet present in the cache so they may be queried.
    //
    // This function reads from the cache, so it locks the cache mutex.
    std::string processNodeInfo(NodeInfoMap& nodeInfoMap);

    // Query cluster data from the database for node IDs listed in the comma-
    // separated missingIds parameter and store them in the nodeInfoMap
    // reference.  Quad-index clusters if necessary.
    //
    // Does NOT modify the live cache so we do not need mutex protection during
    // querying, aggregation, and indexing.
    void queryClusters(
            NodeInfoMap& nodeInfoMap,
            const std::string& missingIds) const;

    // Write node info to the cache safely (with mutex lock).
    void addToCache(const NodeInfoMap& nodeInfoMap);

    // Not implemented.
    GreyReader(const GreyReader&);
    GreyReader& operator=(const GreyReader&);
};

