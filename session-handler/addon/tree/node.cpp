#include <limits>

#include <pdal/PointBuffer.hpp>
#include <pdal/PointContext.hpp>

#include "node.hpp"

namespace
{
    // Factory method used during construction.
    Node* create(
            pdal::PointBuffer* pointBuffer,
            const BBox& bbox,
            std::size_t overflowDepth,
            std::size_t depth,
            uint64_t id)
    {
        if (depth < overflowDepth)
        {
            return new StemNode(pointBuffer, bbox, overflowDepth, depth, id);
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
}

StemNode::StemNode(
        pdal::PointBuffer* pb,
        const BBox& bbox,
        std::size_t overflowDepth,
        std::size_t depth,
        const uint64_t id)
    : Node(bbox, id)
    , point(0)
    , index(pb->append())
    , nw(create(pb, bbox.getNw(), overflowDepth, depth + 1, (id << 2) | nwFlag))
    , ne(create(pb, bbox.getNe(), overflowDepth, depth + 1, (id << 2) | neFlag))
    , sw(create(pb, bbox.getSw(), overflowDepth, depth + 1, (id << 2) | swFlag))
    , se(create(pb, bbox.getSe(), overflowDepth, depth + 1, (id << 2) | seFlag))
{ }

StemNode::~StemNode()
{
    if (point.load()) delete point.load();
}

LeafNode* StemNode::addPoint(
        pdal::PointBuffer* basePointBuffer,
        PointInfo** toAddPtr)
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
                        new PointInfoPulled(
                            curPoint,
                            basePointBuffer,
                            index));

                // Store this point in the basePointBuffer.
                toAdd->write(basePointBuffer, index);
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
                return sw->addPoint(basePointBuffer, &toAdd);
            }
            else
            {
                return nw->addPoint(basePointBuffer, &toAdd);
            }
        }
        else
        {
            if (toAdd->point->y < mid.y)
            {
                return se->addPoint(basePointBuffer, &toAdd);
            }
            else
            {
                return ne->addPoint(basePointBuffer, &toAdd);
            }
        }
    }
    else
    {
        std::unique_lock<std::mutex> lock(mutex);
        if (!point.load())
        {
            toAdd->write(basePointBuffer, index);
            point.store(toAdd->point);
            delete toAdd;
        }
        else
        {
            // Someone beat us here, call again to enter the other branch.
            // Be sure to unlock our mutex first.
            lock.unlock();
            return addPoint(basePointBuffer, &toAdd);
        }
    }

    return 0;
}

void StemNode::getPoints(
        MultiResults& results,
        const std::size_t depthBegin,
        const std::size_t depthEnd,
        std::size_t curDepth) const
{
    if (point.load())
    {
        if (curDepth >= depthBegin && (curDepth < depthEnd || !depthEnd))
        {
            results.push_back(std::make_pair(0, index));
        }

        if (++curDepth < depthEnd || !depthEnd)
        {
            if (nw) nw->getPoints(results, depthBegin, depthEnd, curDepth);
            if (ne) ne->getPoints(results, depthBegin, depthEnd, curDepth);
            if (se) se->getPoints(results, depthBegin, depthEnd, curDepth);
            if (sw) sw->getPoints(results, depthBegin, depthEnd, curDepth);
        }
    }
}

void StemNode::getPoints(
        MultiResults& results,
        const BBox& query,
        const std::size_t depthBegin,
        const std::size_t depthEnd,
        std::size_t depth) const
{
    const Point* p(point.load());
    if (p && query.overlaps(bbox))
    {
        if (
                query.contains(*p) &&
                depth >= depthBegin &&
                (depth < depthEnd || !depthEnd))
        {
            results.push_back(std::make_pair(0, index));
        }

        if (++depth < depthEnd || !depthEnd)
        {
            if (nw) nw->getPoints(results, query, depthBegin, depthEnd, depth);
            if (ne) ne->getPoints(results, query, depthBegin, depthEnd, depth);
            if (se) se->getPoints(results, query, depthBegin, depthEnd, depth);
            if (sw) sw->getPoints(results, query, depthBegin, depthEnd, depth);
        }
    }
}

LeafNode::LeafNode(const BBox& bbox, uint64_t id)
    : Node(bbox, id)
    , overflow()
{ }

LeafNode::~LeafNode()
{ }

LeafNode* LeafNode::addPoint(
        pdal::PointBuffer* basePointBuffer,
        PointInfo** toAddPtr)
{
    PointInfo* toAdd(*toAddPtr);
    delete toAdd->point;
    delete toAdd;
    //std::lock_guard<std::mutex> lock(mutex);
    //overflow.emplace_front(toAdd);
    return this;
}

void LeafNode::save()
{
    // TODO
    std::lock_guard<std::mutex> lock(mutex);
}

void LeafNode::load()
{
    // TODO
    std::lock_guard<std::mutex> lock(mutex);
    std::cout << "Loading" << std::endl;
}

void LeafNode::getPoints(
        MultiResults& results,
        std::size_t depthBegin,
        std::size_t depthEnd,
        std::size_t curDepth) const
{
    // TODO
}

void LeafNode::getPoints(
        MultiResults& results,
        const BBox& query,
        std::size_t depthBegin,
        std::size_t depthEnd,
        std::size_t curDepth) const
{
    // TODO
}

