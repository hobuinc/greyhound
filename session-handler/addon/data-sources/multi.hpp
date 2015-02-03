#pragma once

#include <vector>
#include <string>

#include "types.hpp"
#include "grey-common.hpp"

class SerialPaths;
class Schema;

class MultiDataSource
{
public:
    MultiDataSource(
            const std::string& pipelineId,
            const std::vector<std::string>& paths,
            const Schema& schema,
            const BBox& bbox);

    std::size_t getNumPoints() const;
    std::string getSchema() const;
    std::string getStats() const;
    std::string getSrs() const;

    void serialize(const SerialPaths& serialPaths);



private:

    std::shared_ptr<SleepyTree> m_sleepyIndex;
};

