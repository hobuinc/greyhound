#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "types/source-manager.hpp"
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

    // Returns stringified JSON response.
    std::string info() const;
    std::string hierarchy(
            const entwine::Bounds& bounds,
            std::size_t depthBegin,
            std::size_t depthEnd,
            bool vertical,
            const entwine::Scale* scale,
            const entwine::Offset* offset) const;

    // Read a full unindexed data set.
    std::shared_ptr<ReadQuery> query(
            const entwine::Schema& schema,
            bool compress);

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
    bool resolveIndex(
            const std::string& name,
            const std::vector<std::string>& paths,
            entwine::OuterScope& outerScope,
            std::shared_ptr<entwine::Cache> cache);

    bool resolveSource(
            const std::string& name,
            const std::vector<std::string>& paths);

    void resolveInfo();

    bool indexed() const { return m_entwine.get(); }
    bool sourced() const { return m_source.get(); }

    void check() const
    {
        if (!sourced() && !indexed())
        {
            throw std::runtime_error("Session has no backing data.");
        }
    }

    pdal::StageFactory& m_stageFactory;
    std::mutex& m_factoryMutex;

    Once m_initOnce;
    std::unique_ptr<SourceManager> m_source;
    std::unique_ptr<entwine::Reader> m_entwine;
    std::string m_info;

    // Disallow copy/assignment.
    Session(const Session&);
    Session& operator=(const Session&);
};

