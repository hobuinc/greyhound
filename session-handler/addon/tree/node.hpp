#pragma once

#include <atomic>
#include <cstdint>
#include <forward_list>
#include <memory>
#include <mutex>
#include <map>

#include "sleepy-tree.hpp"
#include "grey/common.hpp"
#include "types/bbox.hpp"
#include "types/point.hpp"

class LeafNode;

class Node
{
public:
    Node(const BBox& bbox, uint64_t id) : bbox(bbox), id(id) { }
    virtual ~Node() { }

    virtual LeafNode* addPoint(
            pdal::PointBuffer* basePointBuffer,
            PointInfo** toAdd) = 0;

    virtual void getPoints(
            MultiResults& results,
            std::size_t depthBegin,
            std::size_t depthEnd,
            std::size_t curDepth = 0) const = 0;

    virtual void getPoints(
            MultiResults& results,
            const BBox& query,
            std::size_t depthBegin,
            std::size_t depthEnd,
            std::size_t curDepth = 0) const = 0;

    const BBox bbox;
    const uint64_t id;

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
            pdal::PointBuffer* pointBuffer,
            const BBox& bbox,
            std::size_t overflowDepth,
            std::size_t curDepth = 0,
            uint64_t id = baseId);
    ~StemNode();

    virtual LeafNode* addPoint(
            pdal::PointBuffer* basePointBuffer,
            PointInfo** toAdd);

    virtual void getPoints(
            MultiResults& results,
            std::size_t depthBegin,
            std::size_t depthEnd,
            std::size_t curDepth = 0) const;

    virtual void getPoints(
            MultiResults& results,
            const BBox& query,
            std::size_t depthBegin,
            std::size_t depthEnd,
            std::size_t curDepth = 0) const;

private:
    std::atomic<const Point*> point;

    const std::size_t index;

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

    virtual LeafNode* addPoint(
            pdal::PointBuffer* basePointBuffer,
            PointInfo** toAdd);

    virtual void getPoints(
            MultiResults& results,
            std::size_t depthBegin,
            std::size_t depthEnd,
            std::size_t curDepth = 0) const;

    virtual void getPoints(
            MultiResults& results,
            const BBox& query,
            std::size_t depthBegin,
            std::size_t depthEnd,
            std::size_t curDepth = 0) const;

    void save();
    void load();

private:
    std::forward_list<std::unique_ptr<const PointInfo>> overflow;
};

