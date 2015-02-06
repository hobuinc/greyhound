#include <limits>

#include <pdal/PointBuffer.hpp>
#include <pdal/PointContext.hpp>

#include "node.hpp"

namespace
{
    // Factory method used during construction.
    Node* create(
            const BBox& bbox,
            std::size_t overflowDepth,
            std::size_t depth,
            uint64_t id)
    {
        if (depth < overflowDepth)
        {
            return new StemNode(bbox, overflowDepth, depth, id);
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
        const BBox& bbox,
        std::size_t overflowDepth,
        std::size_t depth,
        const uint64_t id)
    : Node(bbox, id)
    , data(0)
    , index(std::numeric_limits<std::size_t>::max())
    , nw(create(bbox.getNw(), overflowDepth, depth + 1, (id << 2) | nwFlag))
    , ne(create(bbox.getNe(), overflowDepth, depth + 1, (id << 2) | neFlag))
    , sw(create(bbox.getSw(), overflowDepth, depth + 1, (id << 2) | swFlag))
    , se(create(bbox.getSe(), overflowDepth, depth + 1, (id << 2) | seFlag))
{ }

StemNode::~StemNode()
{
    if (data.load()) delete data.load();
}

bool StemNode::hasData() const
{
    return index != std::numeric_limits<std::size_t>::max();
}

LeafNode* StemNode::addPoint(const PointInfo* toAdd)
{
    if (data.load())
    {
        const Point mid(bbox.mid());

        const PointInfo* current(data.load());
        if (toAdd->point.sqDist(mid) < current->point.sqDist(mid))
        {
            std::lock_guard<std::mutex> lock(mutex);

            // Reload the reference point now that we've acquired the lock.
            if (toAdd->point.sqDist(mid) < data.load()->point.sqDist(mid))
            {
                toAdd = data.exchange(toAdd);
            }
        }

        if (toAdd->point.x < mid.x)
        {
            if (toAdd->point.y < mid.y)
            {
                sw->addPoint(toAdd);
            }
            else
            {
                nw->addPoint(toAdd);
            }
        }
        else
        {
            if (toAdd->point.y < mid.y)
            {
                se->addPoint(toAdd);
            }
            else
            {
                ne->addPoint(toAdd);
            }
        }
    }
    else
    {
        std::unique_lock<std::mutex> lock(mutex);
        const PointInfo* current(data.load());

        if (!current)
        {
            data.store(toAdd);
        }
        else
        {
            // Someone beat us here, call again to enter the other branch.
            // Be sure to unlock our mutex first.
            lock.unlock();
            addPoint(toAdd);
        }
    }

    return 0;
}

void StemNode::finalize(
        std::shared_ptr<pdal::PointBuffer> pointBuffer,
        std::map<uint64_t, LeafNode*>& leafNodes)
{
    // Get our data, and zero it out.  From now on we're just maintaining an
    // index into the pointBuffer we're about to populate.
    const PointInfo* pointInfo(data.exchange(0));

    if (pointInfo)
    {
        std::vector<char> bytes(8);
        char* pos(bytes.data());

        try
        {
            // Store the fields we'll need later.
            index = pointBuffer->size();
            point = pointInfo->point;

            const auto dimTypes(pointBuffer->dimTypes());
            //const auto dimIds(pointBuffer->dims());

            //for (std::size_t i(0); i < dimTypes.size(); ++i)
            for (const auto dim : pointBuffer->dimTypes())
            {
                pointInfo->pointBuffer.getRawField(dim.m_id, 0, pos);
                pointBuffer->setField(dim.m_id, dim.m_type, index, pos);
            }

            delete pointInfo;
        }
        catch (...)
        {
            delete pointInfo;
        }

        if (nw) nw->finalize(pointBuffer, leafNodes);
        if (ne) ne->finalize(pointBuffer, leafNodes);
        if (se) se->finalize(pointBuffer, leafNodes);
        if (sw) sw->finalize(pointBuffer, leafNodes);
    }
}

void StemNode::getPoints(
        MultiResults& results,
        const std::size_t depthBegin,
        const std::size_t depthEnd,
        std::size_t curDepth) const
{
    if (hasData() && curDepth >= depthBegin)
    {
        results.push_back(std::make_pair(0, index));

        if (++curDepth < depthEnd || depthEnd == 0)
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
    if (!query.overlaps(bbox))
    {
        return;
    }

    if (hasData() &&
        query.contains(point) &&
        depth >= depthBegin &&
        (depth < depthEnd || depthEnd == 0))
    {
        results.push_back(std::make_pair(0, index));

        if (++depth < depthEnd || depthEnd == 0)
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

LeafNode* LeafNode::addPoint(const PointInfo* toAdd)
{
    delete toAdd;
    //std::lock_guard<std::mutex> lock(mutex);
    //overflow.emplace_front(toAdd);
    return this;
}

void LeafNode::finalize(
        std::shared_ptr<pdal::PointBuffer> pointBuffer,
        std::map<uint64_t, LeafNode*>& leafNodes)
{
    // TODO
}

void LeafNode::save()
{
    // TODO
    std::lock_guard<std::mutex> lock(mutex);
    std::cout << "Saving" << std::endl;
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

