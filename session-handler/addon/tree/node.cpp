#include <chrono>
#include <fstream>
#include <limits>
#include <thread>

#include <pdal/Charbuf.hpp>
#include <pdal/Compression.hpp>
#include <pdal/PointBuffer.hpp>
#include <pdal/PointContext.hpp>
#include <pdal/QuadIndex.hpp>

#include "compression-stream.hpp"
#include "node.hpp"

namespace
{
    // Factory method used during construction.
    Node* create(
            std::vector<char>* basePoints,
            std::size_t pointSize,
            const BBox& bbox,
            std::size_t overflowDepth,
            std::size_t depth,
            uint64_t id)
    {
        if (depth < overflowDepth)
        {
            return new StemNode(
                    basePoints,
                    pointSize,
                    bbox,
                    overflowDepth,
                    depth,
                    id);
        }
        else if (depth == overflowDepth)
        {
            return new LeafNode(bbox, id);
        }
        else
        {
            throw std::runtime_error("Invalid factory parameters!");
        }
    }

    // TODO
    const std::string diskPath("/var/greyhound/serial");
}

StemNode::StemNode(
        std::vector<char>* base,
        const std::size_t pointSize,
        const BBox& bbox,
        std::size_t overflow,
        std::size_t depth,
        const uint64_t id)
    : Node(bbox, id)
    , point(0)
    , offset(base->size())
{
    base->resize(base->size() + pointSize);
    nw.reset(create(
                base,
                pointSize,
                bbox.getNw(),
                overflow,
                depth + 1,
                (id << 2) | nwFlag));
    ne.reset(create(
                base,
                pointSize,
                bbox.getNe(),
                overflow,
                depth + 1,
                (id << 2) | neFlag));
    sw.reset(create(
                base,
                pointSize,
                bbox.getSw(),
                overflow,
                depth + 1,
                (id << 2) | swFlag));
    se.reset(create(
                base,
                pointSize,
                bbox.getSe(),
                overflow,
                depth + 1,
                (id << 2) | seFlag));
}

StemNode::~StemNode()
{
    if (point.load()) delete point.load();
}

LeafNode* StemNode::addPoint(std::vector<char>* base, PointInfo** toAddPtr)
{
    PointInfo* toAdd(*toAddPtr);
    if (point.load())
    {
        const Point mid(bbox.mid());
        if (toAdd->point->sqDist(mid) < point.load()->sqDist(mid))
        {
            std::lock_guard<std::mutex> lock(mutex);
            const Point* curPoint(point.load());

            // Reload the reference point now that we've acquired the lock.
            if (toAdd->point->sqDist(mid) < curPoint->sqDist(mid))
            {
                // Pull out the old stored value and store the heap-allocated
                // Point that was in our atomic member, so we can overwrite
                // that with the new one.
                PointInfo* old(
                        new PointInfo(
                            curPoint,
                            base->data() + offset,
                            toAdd->bytes.size()));

                // Store this point in the base data store.
                toAdd->write(base->data() + offset);
                point.store(toAdd->point);
                delete toAdd;

                // Send our old stored value downstream.
                toAdd = old;
            }
        }

        if (toAdd->point->x < mid.x)
        {
            if (toAdd->point->y < mid.y)
            {
                return sw->addPoint(base, &toAdd);
            }
            else
            {
                return nw->addPoint(base, &toAdd);
            }
        }
        else
        {
            if (toAdd->point->y < mid.y)
            {
                return se->addPoint(base, &toAdd);
            }
            else
            {
                return ne->addPoint(base, &toAdd);
            }
        }
    }
    else
    {
        std::unique_lock<std::mutex> lock(mutex);
        if (!point.load())
        {
            toAdd->write(base->data() + offset);
            point.store(toAdd->point);
            delete toAdd;
        }
        else
        {
            // Someone beat us here, call again to enter the other branch.
            // Be sure to unlock our mutex first.
            lock.unlock();
            return addPoint(base, &toAdd);
        }
    }

    return 0;
}

void StemNode::getPoints(
        const pdal::PointContextRef pointContext,
        const std::string& pipelineId,
        SleepyCache& cache,
        MultiResults& results,
        const std::size_t depthBegin,
        const std::size_t depthEnd,
        std::size_t curDepth)
{
    if (point.load())
    {
        if (curDepth >= depthBegin && (curDepth < depthEnd || !depthEnd))
        {
            results.push_back(
                    std::make_pair(0, offset / pointContext.pointSize()));
        }

        if (++curDepth < depthEnd || !depthEnd)
        {
            nw->getPoints(
                    pointContext,
                    pipelineId,
                    cache,
                    results,
                    depthBegin,
                    depthEnd,
                    curDepth);
            ne->getPoints(
                    pointContext,
                    pipelineId,
                    cache,
                    results,
                    depthBegin,
                    depthEnd,
                    curDepth);
            se->getPoints(
                    pointContext,
                    pipelineId,
                    cache,
                    results,
                    depthBegin,
                    depthEnd,
                    curDepth);
            sw->getPoints(
                    pointContext,
                    pipelineId,
                    cache,
                    results,
                    depthBegin,
                    depthEnd,
                    curDepth);
        }
    }
}

void StemNode::getPoints(
        const pdal::PointContextRef pointContext,
        const std::string& pipelineId,
        SleepyCache& cache,
        MultiResults& results,
        const BBox& query,
        const std::size_t depthBegin,
        const std::size_t depthEnd,
        std::size_t depth)
{
    const Point* p(point.load());
    if (p && query.overlaps(bbox))
    {
        if (
                query.contains(*p) &&
                depth >= depthBegin &&
                (depth < depthEnd || !depthEnd))
        {
            results.push_back(
                    std::make_pair(0, offset / pointContext.pointSize()));
        }

        if (++depth < depthEnd || !depthEnd)
        {
            nw->getPoints(
                    pointContext,
                    pipelineId,
                    cache,
                    results,
                    query,
                    depthBegin,
                    depthEnd,
                    depth);
            ne->getPoints(
                    pointContext,
                    pipelineId,
                    cache,
                    results,
                    query,
                    depthBegin,
                    depthEnd,
                    depth);
            se->getPoints(
                    pointContext,
                    pipelineId,
                    cache,
                    results,
                    query,
                    depthBegin,
                    depthEnd,
                    depth);
            sw->getPoints(
                    pointContext,
                    pipelineId,
                    cache,
                    results,
                    query,
                    depthBegin,
                    depthEnd,
                    depth);
        }
    }
}

LeafNode::LeafNode(const BBox& bbox, uint64_t id)
    : Node(bbox, id)
    , overflow()
{ }

LeafNode::~LeafNode()
{ }

LeafNode* LeafNode::addPoint(std::vector<char>* base, PointInfo** toAddPtr)
{
    PointInfo* toAdd(*toAddPtr);

    delete toAdd->point;
    delete toAdd;

    return this;
}

void LeafNode::save(Origin origin)
{
    std::lock_guard<std::mutex> lock(mutex);
    overflow.push_back(origin);
}

void LeafNode::getPoints(
        const pdal::PointContextRef pointContext,
        const std::string& pipelineId,
        SleepyCache& cache,
        MultiResults& results,
        std::size_t depthBegin,
        std::size_t depthEnd,
        std::size_t curDepth)
{
    return;
}

void LeafNode::getPoints(
        const pdal::PointContextRef pointContext,
        const std::string& pipelineId,
        SleepyCache& cache,
        MultiResults& results,
        const BBox& query,
        std::size_t depthBegin,
        std::size_t depthEnd,
        std::size_t curDepth)
{
    return;
}

