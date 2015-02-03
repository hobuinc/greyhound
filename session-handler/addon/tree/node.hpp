#pragma once

#include <atomic>
#include <cstdint>
#include <forward_list>
#include <memory>
#include <mutex>
#include <vector>

#include "sleepy-tree.hpp"
#include "grey/common.hpp"
#include "types/bbox.hpp"
#include "types/point.hpp"

class LeafNode;

class Node
{
public:
    explicit Node(const BBox& bbox) : bbox(bbox) { }
    virtual ~Node() { }

    virtual LeafNode* addPoint(const PointInfo* toAdd) = 0;

    /*
    virtual void getPoints(
            std::vector<std::size_t>& results,
            std::size_t depthBegin,
            std::size_t depthEnd,
            std::size_t curDepth = 0) const;

    virtual void getPoints(
            std::vector<std::size_t>& results,
            std::size_t rasterize,
            const RasterMeta& rasterMeta,
            std::size_t curDepth = 0) const;

    virtual void getPoints(
            std::vector<std::size_t>& results,
            const RasterMeta& rasterMeta) const;

    virtual void getPoints(
            std::vector<std::size_t>& results,
            const BBox& query,
            std::size_t depthBegin,
            std::size_t depthEnd,
            std::size_t curDepth = 0) const;
    */

    const BBox bbox;
protected:
    std::mutex mutex;

private:
    // Non-copyable.
    Node(const Node&);
    Node& operator=(const Node&);
};

class StemNode : public Node
{
public:
    StemNode(
            const BBox& bbox,
            std::size_t overflowDepth,
            std::size_t curDepth = 0,
            uint64_t id = baseId);
    ~StemNode();

    virtual LeafNode* addPoint(const PointInfo* toAdd);

private:
    std::atomic<const PointInfo*> data;
    std::unique_ptr<Node> nw;
    std::unique_ptr<Node> ne;
    std::unique_ptr<Node> sw;
    std::unique_ptr<Node> se;
};

class LeafNode : public Node
{
public:
    LeafNode(const BBox& bbox, uint64_t id);
    ~LeafNode();

    virtual LeafNode* addPoint(const PointInfo* toAdd);

    void save();
    void load();

private:
    const uint64_t id;
    std::forward_list<std::unique_ptr<const PointInfo>> overflow;
};

