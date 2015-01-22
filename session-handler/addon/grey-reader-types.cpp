#include <pdal/QuadIndex.hpp>
#include <pdal/PointBuffer.hpp>

#include "grey-reader-types.hpp"

namespace
{
    const std::size_t invalidIndex(std::numeric_limits<std::size_t>::max());
}

std::size_t getRasterIndex(
        std::shared_ptr<pdal::PointBuffer> pointBuffer,
        std::size_t index,
        const RasterMeta& rasterMeta)
{
    const double x(
        pointBuffer->getFieldAs<double>(pdal::Dimension::Id::X, index));
    const double y(
        pointBuffer->getFieldAs<double>(pdal::Dimension::Id::Y, index));

    const std::size_t xOffset(pdal::Utils::sround(
            (x - rasterMeta.xBegin) / rasterMeta.xStep));
    const std::size_t yOffset(pdal::Utils::sround(
            (y - rasterMeta.yBegin) / rasterMeta.yStep));

    if (xOffset < rasterMeta.xNum() && yOffset < rasterMeta.yNum())
        return yOffset * rasterMeta.xNum() + xOffset;
    else
        return invalidIndex;
}

NodeInfo::NodeInfo(
        const BBox& bbox,
        std::size_t depth,
        bool isBase,
        bool complete)
    : complete(complete)
    , cluster(new GreyCluster(depth, bbox, isBase))
{ }

QueryIndex::QueryIndex()
    : id(invalidIndex)
    , index(invalidIndex)
{ }

GreyQuery::GreyQuery(
        const NodeInfoMap& nodeInfoMap,
        std::size_t depthBegin,
        std::size_t depthEnd)
    : m_clusters()
    , m_queryIndexList()
{
    for (const auto& node : nodeInfoMap)
    {
        const uint64_t& id(node.first);
        if (node.second.cluster->populated())
        {
            m_clusters[id] = node.second.cluster;
            node.second.cluster->getIndexList(
                    m_queryIndexList,
                    id,
                    depthBegin,
                    depthEnd);
        }
    }
}

GreyQuery::GreyQuery(
        const NodeInfoMap& nodeInfoMap,
        const BBox& bbox,
        std::size_t depthBegin,
        std::size_t depthEnd)
    : m_clusters()
    , m_queryIndexList()
{
    for (const auto& node : nodeInfoMap)
    {
        const uint64_t& id(node.first);
        if (node.second.cluster->populated())
        {
            m_clusters[id] = node.second.cluster;
            node.second.cluster->getIndexList(
                    m_queryIndexList,
                    id,
                    bbox,
                    depthBegin,
                    depthEnd);
        }
    }
}

GreyQuery::GreyQuery(
        const NodeInfoMap& nodeInfoMap,
        std::size_t rasterize,
        const RasterMeta& rasterMeta,
        std::size_t points)
    : m_clusters()
    , m_queryIndexList(points)
{
    for (const auto& node : nodeInfoMap)
    {
        const uint64_t& id(node.first);
        if (node.second.cluster->populated())
        {
            m_clusters[id] = node.second.cluster;
            node.second.cluster->getIndexList(
                    m_queryIndexList,
                    id,
                    rasterize,
                    rasterMeta);
        }
    }
}

pdal::DimTypeList GreyQuery::dimTypes() const
{
    auto it(m_clusters.begin());
    if (it != m_clusters.end())
    {
        return it->second->pointBuffer()->dimTypes();
    }
    else
    {
        return pdal::DimTypeList();
    }
}

std::shared_ptr<pdal::PointBuffer> GreyQuery::pointBuffer(uint64_t id) const
{
    std::shared_ptr<pdal::PointBuffer> result;

    auto cluster(m_clusters.find(id));
    if (cluster != m_clusters.end())
    {
        result = cluster->second->pointBuffer();
    }

    return result;
}

IdTree::IdTree(
        uint64_t id,
        std::size_t currentLevel,
        std::size_t endLevel)
    : m_id(id)
    , nw()
    , ne()
    , sw()
    , se()
{
    if (++currentLevel < endLevel)
    {
        id <<= 2;
        const uint64_t nwId(id | nwFlag);
        const uint64_t neId(id | neFlag);
        const uint64_t swId(id | swFlag);
        const uint64_t seId(id | seFlag);

        nw.reset(new IdTree(nwId, currentLevel, endLevel));
        ne.reset(new IdTree(neId, currentLevel, endLevel));
        sw.reset(new IdTree(swId, currentLevel, endLevel));
        se.reset(new IdTree(seId, currentLevel, endLevel));
    }
}

void IdTree::find(
        NodeInfoMap& results,
        const std::size_t queryLevelBegin,
        const std::size_t queryLevelEnd,
        const std::size_t currentLevel,
        const BBox        currentBBox) const
{
    if (currentLevel >= queryLevelBegin && currentLevel < queryLevelEnd)
    {
        results.insert(
                std::make_pair(
                    m_id,
                    NodeInfo(currentBBox, currentLevel, m_id == baseId, true)));
    }

    std::size_t nextLevel(currentLevel + 1);
    if (nextLevel < queryLevelEnd)
    {
        if (nw)
            nw->find(
                    results,
                    queryLevelBegin,
                    queryLevelEnd,
                    nextLevel,
                    currentBBox.getNw());

        if (ne)
            ne->find(
                    results,
                    queryLevelBegin,
                    queryLevelEnd,
                    nextLevel,
                    currentBBox.getNe());

        if (sw)
            sw->find(
                    results,
                    queryLevelBegin,
                    queryLevelEnd,
                    nextLevel,
                    currentBBox.getSw());
        if (se)
            se->find(
                    results,
                    queryLevelBegin,
                    queryLevelEnd,
                    nextLevel,
                    currentBBox.getSe());
    }
}

void IdTree::find(
        NodeInfoMap& results,
        const std::size_t   queryLevelBegin,
        const std::size_t   queryLevelEnd,
        const BBox&         queryBBox,
        const std::size_t   currentLevel,
        const BBox          currentBBox) const
{
    if (queryBBox.overlaps(currentBBox))
    {
        if (currentLevel >= queryLevelBegin && currentLevel < queryLevelEnd)
        {
            results.insert(
                    std::make_pair(
                        m_id,
                        NodeInfo(
                            currentBBox,
                            currentLevel,
                            m_id == baseId,
                            queryBBox.contains(currentBBox))));
        }

        std::size_t nextLevel(currentLevel + 1);

        if (nextLevel < queryLevelEnd)
        {
            if (nw)
                nw->find(
                        results,
                        queryLevelBegin,
                        queryLevelEnd,
                        queryBBox,
                        nextLevel,
                        currentBBox.getNw());

            if (ne)
                ne->find(
                        results,
                        queryLevelBegin,
                        queryLevelEnd,
                        queryBBox,
                        nextLevel,
                        currentBBox.getNe());

            if (sw)
                sw->find(
                        results,
                        queryLevelBegin,
                        queryLevelEnd,
                        queryBBox,
                        nextLevel,
                        currentBBox.getSw());

            if (se)
                se->find(
                        results,
                        queryLevelBegin,
                        queryLevelEnd,
                        queryBBox,
                        nextLevel,
                        currentBBox.getSe());
        }
    }
}

IdIndex::IdIndex(const GreyMeta& meta)
    : m_base(meta.base)
    , m_bbox(meta.bbox)
    , nw((baseId << 2) | nwFlag, meta.base, meta.fills.size() + 1)
    , ne((baseId << 2) | neFlag, meta.base, meta.fills.size() + 1)
    , sw((baseId << 2) | swFlag, meta.base, meta.fills.size() + 1)
    , se((baseId << 2) | seFlag, meta.base, meta.fills.size() + 1)
{ }

void IdIndex::find(
        NodeInfoMap& results,
        std::size_t depthBegin,
        std::size_t depthEnd) const
{
    if (depthBegin >= depthEnd) return;

    if (depthBegin < m_base)
    {
        results.insert(
                std::make_pair(baseId, NodeInfo(m_bbox, 0, true, false)));
    }

    if (depthEnd >= m_base)
    {
        nw.find(
                results,
                depthBegin,
                depthEnd,
                m_base,
                m_bbox.getNw());

        ne.find(
                results,
                depthBegin,
                depthEnd,
                m_base,
                m_bbox.getNe());

        sw.find(
                results,
                depthBegin,
                depthEnd,
                m_base,
                m_bbox.getSw());

        se.find(
                results,
                depthBegin,
                depthEnd,
                m_base,
                m_bbox.getSe());
    }
}

void IdIndex::find(
        NodeInfoMap& results,
        std::size_t depthBegin,
        std::size_t depthEnd,
        BBox queryBBox) const
{
    if (depthBegin >= depthEnd) return;

    if (depthBegin < m_base)
    {
        results.insert(
                std::make_pair(baseId, NodeInfo(m_bbox, 0, true, false)));
    }

    if (depthEnd >= m_base)
    {
        nw.find(
                results,
                depthBegin,
                depthEnd,
                queryBBox,
                m_base,
                m_bbox.getNw());

        ne.find(
                results,
                depthBegin,
                depthEnd,
                queryBBox,
                m_base,
                m_bbox.getNe());

        sw.find(
                results,
                depthBegin,
                depthEnd,
                queryBBox,
                m_base,
                m_bbox.getSw());

        se.find(
                results,
                depthBegin,
                depthEnd,
                queryBBox,
                m_base,
                m_bbox.getSe());
    }
}

GreyCluster::GreyCluster(std::size_t depth, const BBox& bbox, bool isBase)
    : m_pointBuffer(0)
    , m_quadTree(0)
    , m_depth(depth)
    , m_bbox(bbox)
    , m_isBase(isBase)
{ }

void GreyCluster::populate(
        std::shared_ptr<pdal::PointBuffer> pointBuffer,
        bool doIndex)
{
    m_pointBuffer = pointBuffer;

    if (doIndex)
    {
        index();
    }
}

void GreyCluster::index()
{
    if (!m_quadTree)
    {
        m_quadTree.reset(
                new pdal::QuadIndex(*m_pointBuffer.get(), m_depth));

        m_quadTree->build(m_bbox.xMin, m_bbox.yMin, m_bbox.xMax, m_bbox.yMax);
    }
}

void GreyCluster::getIndexList(
        QueryIndexList& queryIndexList,
        uint64_t id,
        const std::size_t depthBegin,
        const std::size_t depthEnd) const
{
    if (!m_isBase)
    {
        // If this is not the base cluster, then this query reads all points
        // from this cluster without caring about indexing.
        for (std::size_t i(0); i < m_pointBuffer->size(); ++i)
        {
            queryIndexList.push_back(QueryIndex(id, i));
        }
    }
    else
    {
        // For the base cluster, query for the specified depth levels.
        if (m_quadTree)
        {
            const std::vector<std::size_t> indexList(
                    m_quadTree->getPoints(depthBegin, depthEnd));

            for (std::size_t i(0); i < indexList.size(); ++i)
            {
                queryIndexList.push_back(QueryIndex(id, indexList[i]));
            }
        }
    }
}

void GreyCluster::getIndexList(
        QueryIndexList& queryIndexList,
        uint64_t id,
        const BBox& bbox,
        std::size_t depthBegin,
        std::size_t depthEnd) const
{
    if (!m_isBase)
    {
        // If this is not the base cluster, then this cluster represents a
        // single depth level, so query the whole tree for this BBox.
        depthBegin = 0;
        depthEnd   = 0;
    }

    if (m_quadTree)
    {
        const std::vector<std::size_t> indexList(
                m_quadTree->getPoints(
                    bbox.xMin,
                    bbox.yMin,
                    bbox.xMax,
                    bbox.yMax,
                    depthBegin,
                    depthEnd));

        for (std::size_t i(0); i < indexList.size(); ++i)
        {
            queryIndexList.push_back(QueryIndex(id, indexList[i]));
        }
    }
}

void GreyCluster::getIndexList(
        QueryIndexList& queryIndexList,
        uint64_t id,
        const std::size_t rasterize,
        const RasterMeta& rasterMeta) const
{
    if (!m_isBase)
    {
        // If this is not the base cluster, then this query reads all points
        // from this cluster without caring about indexing.
        for (std::size_t i(0); i < m_pointBuffer->size(); ++i)
        {
            const std::size_t rasterIndex(getRasterIndex(
                        m_pointBuffer,
                        i,
                        rasterMeta));

            if (rasterIndex != invalidIndex)
            {
                queryIndexList.at(rasterIndex) = QueryIndex(id, i);
            }
        }
    }
    else
    {
        // For the base cluster, query for the specified depth level.
        if (m_quadTree)
        {
            const std::vector<std::size_t> indexList(
                    m_quadTree->getPoints(rasterize, rasterize + 1));

            for (std::size_t i(0); i < indexList.size(); ++i)
            {
                const std::size_t rasterIndex(getRasterIndex(
                            m_pointBuffer,
                            indexList[i],
                            rasterMeta));

                if (rasterIndex != invalidIndex)
                {
                    queryIndexList.at(rasterIndex) =
                        QueryIndex(id, indexList[i]);
                }
            }
        }
    }
}

