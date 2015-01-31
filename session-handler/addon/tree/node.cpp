#include <pdal/PointBuffer.hpp>
#include <pdal/PointContext.hpp>

#include "node.hpp"

namespace
{
    // Factory method used during construction.
    Node* create(
            const BBox& bbox,
            std::size_t depth,
            std::size_t overflowDepth,
            uint64_t id)
    {
        if (depth < overflowDepth)
        {
            return new StemNode(bbox, depth, overflowDepth, id);
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
    : Node(bbox)
    , data(0)
    , nw(create(bbox.getNw(), depth + 1, overflowDepth, (id << 2) | nwFlag))
    , ne(create(bbox.getNe(), depth + 1, overflowDepth, (id << 2) | neFlag))
    , sw(create(bbox.getSw(), depth + 1, overflowDepth, (id << 2) | swFlag))
    , se(create(bbox.getSe(), depth + 1, overflowDepth, (id << 2) | seFlag))
{ }

StemNode::~StemNode()
{
    if (data.load()) delete data.load();
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
        std::lock_guard<std::mutex> lock(mutex);
        const PointInfo* current(data.load());

        if (!current)
        {
            data.store(toAdd);
        }
        else
        {
            // Someone beat us here, call again to enter the other branch.
            addPoint(toAdd);
        }
    }

    return 0;
}

/*
void Node::getPoints(
        std::vector<std::size_t>& results,
        const std::size_t depthBegin,
        const std::size_t depthEnd,
        std::size_t curDepth) const
{
    if (data && curDepth >= depthBegin)
    {
        results.push_back(data->pbIndex);
    }

    if (++curDepth < depthEnd || depthEnd == 0)
    {
        if (nw) nw->getPoints(results, depthBegin, depthEnd, curDepth);
        if (ne) ne->getPoints(results, depthBegin, depthEnd, curDepth);
        if (se) se->getPoints(results, depthBegin, depthEnd, curDepth);
        if (sw) sw->getPoints(results, depthBegin, depthEnd, curDepth);
    }
}

void Node::getPoints(
        std::vector<std::size_t>& results,
        const std::size_t rasterize,
        const double xBegin,
        const double xEnd,
        const double xStep,
        const double yBegin,
        const double yEnd,
        const double yStep,
        std::size_t curDepth) const
{
    if (curDepth == rasterize)
    {
        const Point mid(bbox.mid());

        if (data)
        {
            const std::size_t xOffset(
                    pdal::Utils::sround((mid.x - xBegin) / xStep));
            const double yOffset(
                    pdal::Utils::sround((mid.y - yBegin) / yStep));

            const std::size_t index(
                pdal::Utils::sround(yOffset * (xEnd - xBegin) / xStep + xOffset));

            results.at(index) = data->pbIndex;
        }
    }
    else if (++curDepth <= rasterize)
    {
        if (nw) nw->getPoints(
                results,
                rasterize,
                xBegin,
                xEnd,
                xStep,
                yBegin,
                yEnd,
                yStep,
                curDepth);

        if (ne) ne->getPoints(
                results,
                rasterize,
                xBegin,
                xEnd,
                xStep,
                yBegin,
                yEnd,
                yStep,
                curDepth);

        if (se) se->getPoints(
                results,
                rasterize,
                xBegin,
                xEnd,
                xStep,
                yBegin,
                yEnd,
                yStep,
                curDepth);

        if (sw) sw->getPoints(
                results,
                rasterize,
                xBegin,
                xEnd,
                xStep,
                yBegin,
                yEnd,
                yStep,
                curDepth);
    }
}

void Node::getPoints(
        std::vector<std::size_t>& results,
        const double xBegin,
        const double xEnd,
        const double xStep,
        const double yBegin,
        const double yEnd,
        const double yStep) const
{
    if (!bbox.overlaps(BBox(Point(xBegin, xEnd), Point(yBegin, yEnd))))
    {
        return;
    }

    if (nw) nw->getPoints(
            results,
            xBegin,
            xEnd,
            xStep,
            yBegin,
            yEnd,
            yStep);

    if (ne) ne->getPoints(
            results,
            xBegin,
            xEnd,
            xStep,
            yBegin,
            yEnd,
            yStep);

    if (se) se->getPoints(
            results,
            xBegin,
            xEnd,
            xStep,
            yBegin,
            yEnd,
            yStep);

    if (sw) sw->getPoints(
            results,
            xBegin,
            xEnd,
            xStep,
            yBegin,
            yEnd,
            yStep);

    // Add data after calling child nodes so we prefer upper levels of the tree.
    if (
            data &&
            data->point.x >= xBegin &&
            data->point.y >= yBegin &&
            data->point.x < xEnd - xStep &&
            data->point.y < yEnd - yStep)
    {
        const std::size_t xOffset(
                pdal::Utils::sround((data->point.x - xBegin) / xStep));
        const std::size_t yOffset(
                pdal::Utils::sround((data->point.y - yBegin) / yStep));

        const std::size_t index(
            pdal::Utils::sround(yOffset * (xEnd - xBegin) / xStep + xOffset));

        if (index < results.size())
        {
            results.at(index) = data->pbIndex;
        }
    }
}

void Node::getPoints(
        std::vector<std::size_t>& results,
        const BBox& query,
        const std::size_t depthBegin,
        const std::size_t depthEnd,
        std::size_t curDepth) const
{
    if (!query.overlaps(bbox))
    {
        return;
    }

    if (data &&
        query.contains(data->point) &&
        curDepth >= depthBegin &&
        (curDepth < depthEnd || depthEnd == 0))
    {
        results.push_back(data->pbIndex);
    }

    if (++curDepth < depthEnd || depthEnd == 0)
    {
        if (nw) nw->getPoints(results, query, depthBegin, depthEnd, curDepth);
        if (ne) ne->getPoints(results, query, depthBegin, depthEnd, curDepth);
        if (se) se->getPoints(results, query, depthBegin, depthEnd, curDepth);
        if (sw) sw->getPoints(results, query, depthBegin, depthEnd, curDepth);
    }
}
*/

LeafNode::LeafNode(const BBox& bbox, uint64_t id)
    : Node(bbox)
    , id(id)
    , overflow()
{ }

LeafNode::~LeafNode()
{ }

LeafNode* LeafNode::addPoint(const PointInfo* toAdd)
{
    std::lock_guard<std::mutex> lock(mutex);
    overflow.emplace_front(toAdd);
    return this;
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

