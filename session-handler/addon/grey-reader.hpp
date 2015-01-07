#pragma once

#include <string>
#include <mutex>

#include <sqlite3.h>

#include <pdal/QuadIndex.hpp>

#include "grey-common.hpp"
#include "read-command.hpp"

class GreyCluster;

struct NodeInfo
{
    NodeInfo(const BBox& bbox, std::size_t depth, bool isBase, bool complete);

    const bool complete;
    std::shared_ptr<GreyCluster> cluster;
};

typedef std::map<uint64_t, NodeInfo> NodeInfoMap;

struct QueryIndex
{
    QueryIndex();
    QueryIndex(uint64_t id, std::size_t index) : id(id), index(index) { }
    uint64_t id;
    std::size_t index;
};

class GreyQuery
{
public:
    GreyQuery(
            const NodeInfoMap& nodeInfoMap,
            std::size_t depthBegin,
            std::size_t depthEnd);
    GreyQuery(
            const NodeInfoMap& nodeInfoMap,
            const BBox& bbox,
            std::size_t depthBegin,
            std::size_t depthEnd);
    GreyQuery(
            const NodeInfoMap& nodeInfoMap,
            std::size_t rasterize,
            const RasterMeta& rasterMeta,
            std::size_t points);

    std::size_t numPoints() const { return m_queryIndexList.size(); }

    QueryIndex queryIndex(std::size_t index) const
    {
        return m_queryIndexList.at(index);
    }

    std::shared_ptr<pdal::PointBuffer> pointBuffer(uint64_t id);

private:
    std::map<uint64_t, std::shared_ptr<GreyCluster>> m_clusters;
    std::vector<QueryIndex> m_queryIndexList;
};

typedef std::vector<QueryIndex> QueryIndexList;

class GreyCluster
{
public:
    GreyCluster(std::size_t depth, const BBox& bbox, bool isBase);

    void populate(std::shared_ptr<pdal::PointBuffer> pointBuffer, bool doIndex);
    bool populated() const { return m_pointBuffer.get() != 0; }
    void index();
    bool indexed() const { return m_quadTree.get() != 0; }

    std::size_t depth() const { return m_depth; }
    BBox bbox() const { return m_bbox; }

    // Read the indexed dataset within the specified depths.  If this cluster
    // is not the base cluster, then this cluster represents a single depth
    // level, in which case the entire buffer is read.
    void getIndexList(
            QueryIndexList& queryIndexList,
            uint64_t id,
            std::size_t depthBegin,
            std::size_t depthEnd) const;

    // Read the indexed dataset within the specified bounds and depths.
    void getIndexList(
            QueryIndexList& queryIndexList,
            uint64_t id,
            const BBox& bbox,
            std::size_t depthBegin = 0,
            std::size_t depthEnd = 0) const;

    // Read the indexed dataset into a rasterized output.
    void getIndexList(
            QueryIndexList& queryIndexList,
            uint64_t id,
            std::size_t rasterize,
            const RasterMeta& rasterMeta) const;

    std::shared_ptr<pdal::PointBuffer> pointBuffer() { return m_pointBuffer; }

private:
    std::shared_ptr<pdal::PointBuffer> m_pointBuffer;
    std::shared_ptr<pdal::QuadIndex> m_quadTree;
    const std::size_t m_depth;
    const BBox m_bbox;
    const bool m_isBase;
};

class IdTree
{
public:
    IdTree(
            uint64_t id,
            std::size_t currentLevel,
            std::size_t endLevel);

    void find(
            NodeInfoMap& results,
            std::size_t queryLevelBegin,
            std::size_t queryLevelEnd,
            std::size_t currentLevel,
            BBox        currentBBox) const;

    void find(
            NodeInfoMap& results,
            std::size_t queryLevelBegin,
            std::size_t queryLevelEnd,
            const BBox& queryBBox,
            std::size_t currentLevel,
            BBox        currentBBox) const;

private:
    const uint64_t m_id;
    std::unique_ptr<IdTree> nw;
    std::unique_ptr<IdTree> ne;
    std::unique_ptr<IdTree> sw;
    std::unique_ptr<IdTree> se;
};

class IdIndex
{
public:
    IdIndex(const GreyMeta& meta);

    void find(
            NodeInfoMap& results,
            std::size_t depthLevelBegin,
            std::size_t depthLevelEnd) const;

    void find(
            NodeInfoMap& results,
            std::size_t depthLevelBegin,
            std::size_t depthLevelEnd,
            BBox queryBBox) const;

private:
    const std::size_t m_base;
    const BBox m_bbox;

    const IdTree nw;
    const IdTree ne;
    const IdTree sw;
    const IdTree se;
};

class GreyReader
{
public:
    GreyReader(
            std::string pipelineId,
            const std::vector<std::string>& serialPaths);

    ~GreyReader();

    static bool exists(
            const std::string pipelineId,
            const std::vector<std::string>& serialPaths);

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

