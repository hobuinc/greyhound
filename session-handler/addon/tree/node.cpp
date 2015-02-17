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
    const std::size_t dimensions(2);

    std::size_t getOffset(std::size_t depth)
    {
        if (depth == 0) return 0;

        std::size_t offset(1);
        std::size_t factor(1 << dimensions);
        for (std::size_t i(0); i < depth; ++i)
        {
            offset *= factor;
        }

        return offset;
    }
}

Roller::Roller(const BBox& bbox)
    : m_pos(0)
    , m_bbox(bbox)
    , m_depth(0)
{ }

Roller::Roller(const Roller& other)
    : m_pos(other.m_pos)
    , m_bbox(other.m_bbox)
    , m_depth(other.m_depth)
{ }

void Roller::magnify(const Point* point)
{
    const Point mid(m_bbox.mid());

    if (point->x < mid.x)
        if (point->y < mid.y)
            goSw();
        else
            goNw();
    else
        if (point->y < mid.y)
            goSe();
        else
            goNe();
}

std::size_t Roller::depth() const
{
    return m_depth;
}

uint64_t Roller::pos() const
{
    return m_pos;
}

const BBox& Roller::bbox() const
{
    return m_bbox;
}

void Roller::goNw()
{
    levelUp(Dir::nw);
    m_bbox = m_bbox.getNw();
}

void Roller::goNe()
{
    levelUp(Dir::ne);
    m_bbox = m_bbox.getNe();
}

void Roller::goSw()
{
    levelUp(Dir::sw);
    m_bbox = m_bbox.getSw();
}

void Roller::goSe()
{
    levelUp(Dir::se);
    m_bbox = m_bbox.getSe();
}

void Roller::levelUp(const Dir dir)
{
    m_pos = (m_pos << dimensions) + 1 + dir;
    ++m_depth;
}

Roller Roller::getNw() const
{
    Roller roller(*this);
    roller.goNw();
    return roller;
}

Roller Roller::getNe() const
{
    Roller roller(*this);
    roller.goNe();
    return roller;
}

Roller Roller::getSw() const
{
    Roller roller(*this);
    roller.goSw();
    return roller;
}

Roller Roller::getSe() const
{
    Roller roller(*this);
    roller.goSe();
    return roller;
}




Registry::Registry(
        std::size_t pointSize,
        std::size_t baseDepth,
        std::size_t flatDepth,
        std::size_t deadDepth)
    : m_pointSize(pointSize)
    , m_baseDepth(baseDepth)
    , m_flatDepth(flatDepth)
    , m_deadDepth(deadDepth)
    , m_baseOffset(getOffset(baseDepth))
    , m_flatOffset(getOffset(flatDepth))
    , m_deadOffset(getOffset(deadDepth))
    , m_basePoints(m_baseOffset, std::atomic<const Point*>(0))
    , m_baseData(new std::vector<char>(m_baseOffset * pointSize, 0))
    , m_baseLocks(m_baseOffset)
{
    if (baseDepth > flatDepth || flatDepth > deadDepth)
    {
        throw std::runtime_error("Invalid registry params");
    }
}

Registry::Registry(
        std::size_t pointSize,
        std::shared_ptr<std::vector<char>> data,
        std::size_t baseDepth,
        std::size_t flatDepth,
        std::size_t deadDepth)
    : m_pointSize(pointSize)
    , m_baseDepth(baseDepth)
    , m_flatDepth(flatDepth)
    , m_deadDepth(deadDepth)
    , m_baseOffset(getOffset(baseDepth))
    , m_flatOffset(getOffset(flatDepth))
    , m_deadOffset(getOffset(deadDepth))
    , m_basePoints(m_baseOffset, std::atomic<const Point*>(0))
    , m_baseData(data)
    , m_baseLocks(m_baseOffset)
{
    if (baseDepth > flatDepth || flatDepth > deadDepth)
    {
        throw std::runtime_error("Invalid registry params");
    }

    double x(0);
    double y(0);

    for (std::size_t i(0); i < m_baseData->size() / m_pointSize; ++i)
    {
        std::memcpy(
                reinterpret_cast<char*>(&x),
                m_baseData->data() + m_pointSize * i,
                sizeof(double));
        std::memcpy(
                reinterpret_cast<char*>(&y),
                m_baseData->data() + m_pointSize * i,
                sizeof(double));

        if (x != 0 && y != 0)
        {
            m_basePoints[i].atom.store(new Point(x, y));
        }
    }
}

Registry::~Registry()
{
    for (auto& p : m_basePoints)
    {
        if (p.atom.load()) delete p.atom.load();
    }
}

void Registry::put(
        PointInfo** toAddPtr,
        Roller& roller)
{
    bool done(false);

    PointInfo* toAdd(*toAddPtr);

    if (roller.depth() < m_baseDepth)
    {
        const std::size_t index(roller.pos());
        auto& myPoint(m_basePoints[index].atom);

        if (myPoint.load())
        {
            const Point mid(roller.bbox().mid());
            if (toAdd->point->sqDist(mid) < myPoint.load()->sqDist(mid))
            {
                std::lock_guard<std::mutex> lock(m_baseLocks[index]);
                const Point* curPoint(myPoint.load());

                // Reload the reference point now that we've acquired the lock.
                if (toAdd->point->sqDist(mid) < curPoint->sqDist(mid))
                {
                    // Pull out the old stored value and store the Point that
                    // was in our atomic member, so we can overwrite that with
                    // the new one.
                    PointInfo* old(
                            new PointInfo(
                                curPoint,
                                m_baseData->data() + index * m_pointSize,
                                toAdd->bytes.size()));

                    // Store this point in the base data store.
                    toAdd->write(m_baseData->data() + index * m_pointSize);
                    myPoint.store(toAdd->point);
                    delete toAdd;

                    // Send our old stored value downstream.
                    toAdd = old;
                }
            }
        }
        else
        {
            std::unique_lock<std::mutex> lock(m_baseLocks[index]);
            if (!myPoint.load())
            {
                toAdd->write(m_baseData->data() + index * m_pointSize);
                myPoint.store(toAdd->point);
                delete toAdd;
                done = true;
            }
            else
            {
                std::cout << "Averting race condition." << std::endl;

                // Someone beat us here, call again to enter the other branch.
                // Be sure to unlock our mutex first.
                lock.unlock();
                put(&toAdd, roller);
            }
        }
    }
    else if (roller.depth() < m_flatDepth) { /* TODO */ }
    else if (roller.depth() < m_deadDepth) { /* TODO */ }
    else
    {
        delete toAdd->point;
        delete toAdd;

        done = true;
    }

    if (!done)
    {
        roller.magnify(toAdd->point);
        put(&toAdd, roller);
    }
}

void Registry::getPoints(
        const Roller& roller,
        MultiResults& results,
        std::size_t depthBegin,
        std::size_t depthEnd)
{
    auto& myPoint(m_basePoints[roller.pos()].atom);

    if (myPoint.load())
    {
        if (
                (roller.depth() >= depthBegin) &&
                (roller.depth() < depthEnd || !depthEnd))
        {
            results.push_back(std::make_pair(0, roller.pos() * m_pointSize));
        }

        if (roller.depth() + 1 < depthEnd || !depthEnd)
        {
            getPoints(roller.getNw(), results, depthBegin, depthEnd);
            getPoints(roller.getNe(), results, depthBegin, depthEnd);
            getPoints(roller.getSw(), results, depthBegin, depthEnd);
            getPoints(roller.getSe(), results, depthBegin, depthEnd);
        }
    }
}

void Registry::getPoints(
        const Roller& roller,
        MultiResults& results,
        const BBox& query,
        std::size_t depthBegin,
        std::size_t depthEnd)
{
    if (!roller.bbox().overlaps(query)) return;

    auto& myPoint(m_basePoints[roller.pos()].atom);
    if (myPoint.load())
    {
        if (
                (roller.depth() >= depthBegin) &&
                (roller.depth() < depthEnd || !depthEnd))
        {
            results.push_back(std::make_pair(0, roller.pos() * m_pointSize));
        }

        if (roller.depth() + 1 < depthEnd || !depthEnd)
        {
            getPoints(roller.getNw(), results, depthBegin, depthEnd);
            getPoints(roller.getNe(), results, depthBegin, depthEnd);
            getPoints(roller.getSw(), results, depthBegin, depthEnd);
            getPoints(roller.getSe(), results, depthBegin, depthEnd);
        }
    }
}





Sleeper::Sleeper(const BBox& bbox, const std::size_t pointSize)
    : m_bbox(bbox)
    , m_registry(pointSize)
{ }

Sleeper::Sleeper(
        const BBox& bbox,
        const std::size_t pointSize,
        std::shared_ptr<std::vector<char>> data)
    : m_bbox(bbox)
    , m_registry(pointSize, data)
{ }

void Sleeper::addPoint(PointInfo** toAddPtr)
{
    Roller roller(m_bbox);
    m_registry.put(toAddPtr, roller);
}

void Sleeper::getPoints(
        MultiResults& results,
        std::size_t depthBegin,
        std::size_t depthEnd)
{
    Roller roller(m_bbox);
    m_registry.getPoints(roller, results, depthBegin, depthEnd);
}

void Sleeper::getPoints(
        MultiResults& results,
        const BBox& query,
        std::size_t depthBegin,
        std::size_t depthEnd)
{
    Roller roller(m_bbox);
    m_registry.getPoints(roller, results, query, depthBegin, depthEnd);
}

BBox Sleeper::bbox() const
{
    return m_bbox;
}

