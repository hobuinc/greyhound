#pragma once

#include <memory>

namespace pdal
{
    class Options;
    class Reader;
    class StageFactory;
}

namespace entwine
{
    class Schema;
}

class SourceManager
{
public:
    SourceManager(
            const pdal::StageFactory& stageFactory,
            std::string path,
            std::string driver);

    std::unique_ptr<pdal::Reader> createReader() const;

    std::size_t numPoints() const;
    const entwine::Schema& schema() const;

private:
    const pdal::StageFactory& m_stageFactory;
    std::unique_ptr<pdal::Options> m_options;

    std::string m_driver;
    std::unique_ptr<entwine::Schema> m_schema;
    std::size_t m_numPoints;
};

