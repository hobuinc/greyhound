#pragma once

#include <string>
#include <mutex>

#include <sqlite3.h>

#include <pdal/PointContext.hpp>
#include <pdal/QuadIndex.hpp>

#include "http/s3.hpp"
#include "grey/reader-types.hpp"
#include "grey/common.hpp"

class RasterMeta;
class SerialPaths;

class GreyReader
{
public:
    GreyReader();
    virtual ~GreyReader() { }

    std::size_t getNumPoints() const            { return m_meta.numPoints;  }
    std::string getSchema() const               { return m_meta.schema;     }
    std::string getStats() const                { return m_meta.stats;      }
    std::string getSrs() const                  { return m_meta.srs;        }
    std::vector<std::size_t> getFills() const   { return m_meta.fills;      }

    GreyQuery query(std::size_t depthBegin, std::size_t depthEnd);

    GreyQuery query(
            const BBox& bbox,
            std::size_t depthBegin,
            std::size_t depthEnd);

    GreyQuery query(std::size_t rasterize, RasterMeta& rasterMeta);

    const GreyMeta& meta() const { return m_meta; }
    const pdal::PointContext& pointContext() const { return m_pointContext; }

    static bool exists(
            std::string pipelineId,
            const SerialPaths& serialPaths);
protected:
    // Query cluster data from the database for the supplied node IDs and store
    // them in the nodeInfoMap reference.  Quad-index clusters if necessary.
    //
    // Does NOT modify the live cache so we do not need mutex protection during
    // querying, aggregation, and indexing.
    virtual void queryClusters(
            NodeInfoMap& nodeInfoMap,
            const std::vector<std::size_t>& missingIds) = 0;

    // Derived constructors should acquire metadata and call this function.
    void init(GreyMeta meta);

private:
    bool m_initialized;
    GreyMeta m_meta;
    std::unique_ptr<IdIndex> m_idIndex;

    pdal::PointContext m_pointContext;

    std::mutex m_cacheMutex;
    std::map<uint64_t, std::shared_ptr<GreyCluster>> m_cache;

    // Processes a collection of node info.
    //      1 - Identify which nodes are already live in the cache.
    //          1a - Quad-index these nodes if it's necessary for this query.
    //      2 - Return a vector of the node IDs that are not yet present in the
    //          cache so they may be queried.
    //
    // This function reads from the cache, so it locks the cache mutex.
    std::vector<std::size_t> processNodeInfo(NodeInfoMap& nodeInfoMap);

    // Write node info to the cache safely (with mutex lock).
    void addToCache(const NodeInfoMap& nodeInfoMap);

    // Not implemented.
    GreyReader(const GreyReader&);
    GreyReader& operator=(const GreyReader&);
};

class GreyReaderSqlite : public GreyReader
{
public:
    GreyReaderSqlite(std::string pipelineId, const SerialPaths& serialPaths);
    ~GreyReaderSqlite() { if (m_db) sqlite3_close_v2(m_db); }

    static bool exists(
            const std::string pipelineId,
            const std::vector<std::string>& diskPaths);

private:
    sqlite3* m_db;

    virtual void queryClusters(
            NodeInfoMap& nodeInfoMap,
            const std::vector<std::size_t>& missingIds);
};

class GreyReaderS3 : public GreyReader
{
public:
    GreyReaderS3(std::string pipelineId, const SerialPaths& serialPaths);
    ~GreyReaderS3() { }

    static bool exists(const std::string pipelineId, const S3Info& s3Info);

private:
    S3 m_s3;
    const std::string m_pipelineId;

    virtual void queryClusters(
            NodeInfoMap& nodeInfoMap,
            const std::vector<std::size_t>& missingIds);
};

class GreyReaderFactory
{
public:
    // Shall not throw.
    static GreyReader* create(
            std::string pipelineId,
            const SerialPaths& serialPaths);
};

