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

// A copy-constructible and assignable atomic wrapper for use in a vector.
template <typename T>
struct ElasticAtomic
{
    ElasticAtomic()
        : atom()
    { }

    ElasticAtomic(const std::atomic<T>& other)
        : atom(other.load())
    { }

    ElasticAtomic(const ElasticAtomic& other)
        : atom(other.atom.load())
    { }

    ElasticAtomic& operator=(const ElasticAtomic& other)
    {
        atom.store(other.atom.load());
        return *this;
    }

    std::atomic<T> atom;
};

// Maintains the state of the current point as it traverses the virtual tree.
class Roller
{
public:
    Roller(const BBox& bbox);
    Roller(const Roller& other);

    void magnify(const Point* point);
    std::size_t depth() const;
    uint64_t pos() const;
    const BBox& bbox() const;

    void goNw();
    void goNe();
    void goSw();
    void goSe();

    Roller getNw() const;
    Roller getNe() const;
    Roller getSw() const;
    Roller getSe() const;

    enum Dir
    {
        nw = 0,
        ne = 1,
        sw = 2,
        se = 3
    };

private:
    uint64_t m_pos;
    BBox m_bbox;

    std::size_t m_depth;

    void levelUp(Dir dir);
};

// Maintains mapping to house the data belonging to each virtual node.
class Registry
{
public:
    Registry(
            std::size_t pointSize,
            std::size_t baseDepth = 12, // TODO - Set.
            std::size_t flatDepth = 12,
            std::size_t deadDepth = 12);
    Registry(
            std::size_t pointSize,
            std::shared_ptr<std::vector<char>> data,
            std::size_t baseDepth = 12, // TODO - Set.
            std::size_t flatDepth = 12,
            std::size_t deadDepth = 12);
    ~Registry();

    void put(PointInfo** toAddPtr, Roller& roller);

    void getPoints(
            const Roller& roller,
            MultiResults& results,
            std::size_t depthBegin,
            std::size_t depthEnd);

    void getPoints(
            const Roller& roller,
            MultiResults& results,
            const BBox& query,
            std::size_t depthBegin,
            std::size_t depthEnd);

    std::shared_ptr<std::vector<char>> baseData() { return m_baseData; }

private:
    const std::size_t m_pointSize;

    const std::size_t m_baseDepth;
    const std::size_t m_flatDepth;
    const std::size_t m_deadDepth;

    const std::size_t m_baseOffset;
    const std::size_t m_flatOffset;
    const std::size_t m_deadOffset;

    // TODO Real structures.  These are testing only.
    std::vector<ElasticAtomic<const Point*>> m_basePoints;
    std::shared_ptr<std::vector<char>> m_baseData;
    std::vector<std::mutex> m_baseLocks;
};

class Sleeper
{
public:
    Sleeper(const BBox& bbox, std::size_t pointSize);
    Sleeper(
            const BBox& bbox,
            std::size_t pointSize,
            std::shared_ptr<std::vector<char>> data);

    void addPoint(PointInfo** toAddPtr);

    void getPoints(
            MultiResults& results,
            std::size_t depthBegin,
            std::size_t depthEnd);

    void getPoints(
            MultiResults& results,
            const BBox& query,
            std::size_t depthBegin,
            std::size_t depthEnd);

    std::shared_ptr<std::vector<char>> baseData()
    {
        return m_registry.baseData();
    }

    BBox bbox() const;

private:
    const BBox m_bbox;

    Registry m_registry;
};

