#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "util/once.hpp"

namespace pdal
{
    class PointContext;
    class StageFactory;
}

namespace entwine
{
    class Bounds;
    class Cache;
    class OuterScope;
    class Reader;
    class Schema;
}

class ReadQuery;

class WrongQueryType : public std::runtime_error
{
public:
    WrongQueryType()
        : std::runtime_error("Invalid query type for this resource")
    { }
};

class Session
{
public:
    Session(pdal::StageFactory& stageFactory, std::mutex& factoryMutex);
    ~Session();

    // Returns true if initialization was successful.  If false, this session
    // should not be used.
    bool initialize(
            const std::string& name,
            std::vector<std::string> paths,
            entwine::OuterScope& outerScope,
            std::shared_ptr<entwine::Cache> cache);

    Json::Value info() const;
    Json::Value hierarchy(
            const entwine::Bounds& bounds,
            std::size_t depthBegin,
            std::size_t depthEnd,
            bool vertical,
            const entwine::Scale* scale,
            const entwine::Offset* offset) const;
    Json::Value files(
            const Json::Value& search,
            const entwine::Scale* scale,
            const entwine::Offset* offset) const;

    // Read quad-tree indexed data with a bounding box query and min/max tree
    // depths to search.
    std::shared_ptr<ReadQuery> query(
            const entwine::Schema& schema,
            const Json::Value& filter,
            bool compress,
            const entwine::Point* scale,
            const entwine::Point* offset,
            const entwine::Bounds* bounds,
            std::size_t depthBegin,
            std::size_t depthEnd);

    const entwine::Schema& schema() const;

private:
    Json::Value filesSingle(
            const Json::Value& search,
            const entwine::Scale* scale,
            const entwine::Offset* offset) const;

    void check() const
    {
        if (!m_entwine)
        {
            throw std::runtime_error("Session was not created");
        }
    }

    bool resolveIndex(
            const std::string& name,
            const std::vector<std::string>& paths,
            entwine::OuterScope& outerScope,
            std::shared_ptr<entwine::Cache> cache);

    bool indexed() const { return m_entwine.get(); }

    Once m_initOnce;
    std::unique_ptr<entwine::Reader> m_entwine;
    Json::Value m_info;

    // Disallow copy/assignment.
    Session(const Session&);
    Session& operator=(const Session&);
};

