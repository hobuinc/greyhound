#pragma once

#include <memory>

#include <pdal/QuadIndex.hpp>
#include <pdal/Dimension.hpp>

#include "grey/common.hpp"

class GreyCluster;
class RasterMeta;

std::size_t getRasterIndex(
        std::shared_ptr<pdal::PointBuffer> pointBuffer,
        std::size_t index,
        const RasterMeta& rasterMeta);

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

    pdal::DimTypeList dimTypes() const;

    std::shared_ptr<pdal::PointBuffer> pointBuffer(uint64_t id) const;

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
    void index();

    bool populated() const;
    bool indexed() const;

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

    std::shared_ptr<pdal::PointBuffer> pointBuffer();

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

