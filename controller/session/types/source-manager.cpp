#include "source-manager.hpp"

#include <pdal/Options.hpp>
#include <pdal/Reader.hpp>
#include <pdal/StageFactory.hpp>

#include <entwine/types/schema.hpp>
#include <entwine/types/simple-point-table.hpp>

SourceManager::SourceManager(
        pdal::StageFactory& stageFactory,
        std::mutex& factoryMutex,
        std::string path,
        std::string driver)
    : m_stageFactory(stageFactory)
    , m_factoryMutex(factoryMutex)
    , m_options(new pdal::Options())
    , m_driver(driver)
    , m_schema(new entwine::Schema())
    , m_numPoints(0)
{
    m_options->add(pdal::Option("filename", path));

    // Determine the number of points in this resource.
    {
        auto reader(createReader());

        entwine::DimList emptyDims;
        entwine::Schema emptySchema(emptyDims);
        entwine::SimplePointTable pointTable(emptySchema);

        reader->setReadCb(
                [this, &pointTable]
                (pdal::PointView& view, pdal::PointId index)
        {
            ++m_numPoints;
            pointTable.clear();
        });

        reader->prepare(pointTable);
        reader->execute(pointTable);
    }

    // Determine the native schema of the resource.
    {
        auto reader(createReader());
        entwine::SimplePointTable pointTable(*m_schema);
        reader->prepare(pointTable);
        m_schema->finalize();
    }
}

std::unique_ptr<pdal::Reader> SourceManager::createReader()
{
    std::unique_lock<std::mutex> lock(m_factoryMutex);
    std::unique_ptr<pdal::Reader> reader(
            static_cast<pdal::Reader*>(
                m_stageFactory.createStage(m_driver)));
    lock.unlock();

    reader->setOptions(*m_options);
    return reader;
}

std::size_t SourceManager::numPoints() const
{
    return m_numPoints;
}

const entwine::Schema& SourceManager::schema() const
{
    return *m_schema;
}

