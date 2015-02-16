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

namespace pdal
{
    class QuadIndex;
}

class LeafNode;
class PutCollector;
class S3;

class Node
{
public:
    Node() { }
    virtual ~Node() { }

    virtual LeafNode* addPoint(
            BBox bbox,
            uint64_t id,
            std::vector<char>* base,
            PointInfo** toAdd) = 0;

    virtual void getPoints(
            BBox bbox,
            uint64_t id,
            const pdal::PointContextRef pointContext,
            const std::string& pipelineId,
            SleepyCache& cache,
            MultiResults& results,
            std::size_t depthBegin,
            std::size_t depthEnd,
            std::size_t curDepth = 0) = 0;

    virtual void getPoints(
            BBox bbox,
            uint64_t id,
            const pdal::PointContextRef pointContext,
            const std::string& pipelineId,
            SleepyCache& cache,
            MultiResults& results,
            const BBox& query,
            std::size_t depthBegin,
            std::size_t depthEnd,
            std::size_t curDepth = 0) = 0;

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
            std::vector<char>* basePoints,
            std::size_t pointSize,
            std::size_t overflowDepth,
            std::size_t curDepth = 0);
    ~StemNode();

    virtual LeafNode* addPoint(
            BBox bbox,
            uint64_t id,
            std::vector<char>* base,
            PointInfo** toAdd);

    virtual void getPoints(
            BBox bbox,
            uint64_t id,
            const pdal::PointContextRef pointContext,
            const std::string& pipelineId,
            SleepyCache& cache,
            MultiResults& results,
            std::size_t depthBegin,
            std::size_t depthEnd,
            std::size_t curDepth = 0);

    virtual void getPoints(
            BBox bbox,
            uint64_t id,
            const pdal::PointContextRef pointContext,
            const std::string& pipelineId,
            SleepyCache& cache,
            MultiResults& results,
            const BBox& query,
            std::size_t depthBegin,
            std::size_t depthEnd,
            std::size_t curDepth = 0);

private:
    std::atomic<const Point*> point;

    const std::size_t offset;

    std::unique_ptr<Node> nw;
    std::unique_ptr<Node> ne;
    std::unique_ptr<Node> sw;
    std::unique_ptr<Node> se;
};

class LeafNode : public Node
{
public:
    LeafNode();
    ~LeafNode();

    virtual LeafNode* addPoint(
            BBox bbox,
            uint64_t id,
            std::vector<char>* base,
            PointInfo** toAdd);

    virtual void getPoints(
            BBox bbox,
            uint64_t id,
            const pdal::PointContextRef pointContext,
            const std::string& pipelineId,
            SleepyCache& cache,
            MultiResults& results,
            std::size_t depthBegin,
            std::size_t depthEnd,
            std::size_t curDepth = 0);

    virtual void getPoints(
            BBox bbox,
            uint64_t id,
            const pdal::PointContextRef pointContext,
            const std::string& pipelineId,
            SleepyCache& cache,
            MultiResults& results,
            const BBox& query,
            std::size_t depthBegin,
            std::size_t depthEnd,
            std::size_t curDepth = 0);

    void save(Origin origin);

private:
    std::vector<Origin> overflow;
};

