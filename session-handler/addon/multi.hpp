#pragma once

#include <vector>
#include <string>

#include "data-sources/base.hpp"

namespace entwine
{
    class BBox;
    class S3Info;
    class Schema;
    class SleepyTree;
}

class ReadQuery;
class SerialPaths;

class MultiDataSource : public DataSource
{
public:
    MultiDataSource(
            const std::string& pipelineId,
            const std::vector<std::string>& paths,
            const entwine::Schema& schema,
            const entwine::BBox& bbox,
            const entwine::S3Info& s3Info);

    virtual std::size_t getNumPoints() const;
    virtual std::string getSchema() const;
    virtual std::string getStats() const;
    virtual std::string getSrs() const;
    virtual std::vector<std::size_t> getFills() const;

    virtual std::shared_ptr<ReadQuery> query(
            const entwine::Schema& schema,
            bool compressed,
            const entwine::BBox& bbox,
            std::size_t depthBegin,
            std::size_t depthEnd);

    virtual std::shared_ptr<ReadQuery> query(
            const entwine::Schema& schema,
            bool compressed,
            std::size_t depthBegin,
            std::size_t depthEnd);

private:
    std::shared_ptr<entwine::SleepyTree> m_sleepyTree;
};

