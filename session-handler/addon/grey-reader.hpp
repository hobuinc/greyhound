#pragma once

#include <string>
#include <mutex>

#include <sqlite3.h>

#include "grey-common.hpp"
#include "read-command.hpp"

class GreyCluster
{
public:
    GreyCluster(std::size_t depth, const BBox& bbox);

    void populate(std::shared_ptr<pdal::PointBuffer> pointBuffer, bool doIndex);
    void index();

    std::size_t depth() const { return m_depth; }
    BBox bbox() const { return m_bbox; }

    // Read entire dataset.
    std::size_t read(
            std::vector<uint8_t>& buffer,
            const Schema& schema) const;

    // Read the indexed dataset within the specified bounds and depths.  Note
    // that for the base cluster, the depths represent actual quad-tree depths.
    // For clusters other than the base, always use [0, 0] because these
    // clusters represent a single quad-tree level.
    std::size_t read(
            std::vector<uint8_t>& buffer,
            const Schema& schema,
            const BBox& bbox,
            std::size_t depthBegin = 0,
            std::size_t depthEnd = 0) const;

    // Read indexed dataset.  Note that aside from the cluster with an ID of
    // baseId, all clusters represent a single level of the aggregated tree.
    // This function is ONLY useful for the base cluster.
    std::size_t readBase(
            std::vector<uint8_t>& buffer,
            const Schema& schema,
            std::size_t depthBegin,
            std::size_t depthEnd) const;

private:
    std::shared_ptr<pdal::PointBuffer> m_pointBuffer;
    std::shared_ptr<pdal::QuadIndex> m_quadTree;
    const std::size_t m_depth;
    const BBox m_bbox;

    std::size_t readDim(
            unsigned char* buffer,
            const DimInfo& dim,
            const pdal::PointBuffer& pointBuffer,
            const std::size_t index) const;
};

struct NodeInfo
{
    NodeInfo(const BBox& bbox, std::size_t depth, bool complete)
        : complete(complete)
        , cluster(new GreyCluster(depth, bbox))
    { }

    const bool complete;
    std::shared_ptr<GreyCluster> cluster;
};

typedef std::map<uint64_t, NodeInfo> NodeInfoMap;

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
    GreyReader(std::string pipelineId);
    ~GreyReader();

    static bool exists(const std::string pipelineId);

    std::size_t getNumPoints() const            { return m_meta.numPoints;  }
    std::string getSchema() const               { return m_meta.schema;     }
    std::string getStats() const                { return m_meta.stats;      }
    std::string getSrs() const                  { return m_meta.srs;        }
    std::vector<std::size_t> getFills() const   { return m_meta.fills;      }

    std::size_t read(
            std::vector<uint8_t>& buffer,
            const Schema& schema,
            std::size_t depthBegin,
            std::size_t depthEnd);

    std::size_t read(
            std::vector<uint8_t>& buffer,
            const Schema& schema,
            double xMin,
            double yMin,
            double xMax,
            double yMax,
            std::size_t depthBegin,
            std::size_t depthEnd);

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

    // Not implemented.
    GreyReader(const GreyReader&);
    GreyReader& operator=(const GreyReader&);
};

