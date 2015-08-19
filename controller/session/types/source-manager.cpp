#include "source-manager.hpp"

#include <pdal/Options.hpp>
#include <pdal/Reader.hpp>
#include <pdal/StageFactory.hpp>

#include <entwine/types/bbox.hpp>
#include <entwine/types/schema.hpp>
#include <entwine/types/simple-point-table.hpp>
#include <entwine/util/executor.hpp>

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
    , m_bbox(new entwine::BBox())
    , m_numPoints(0)
{
    m_options->add(pdal::Option("filename", path));

    // Use BBox::set() to avoid malformed BBox warning.
    m_bbox->set(
            entwine::Point(
                std::numeric_limits<double>::max(),
                std::numeric_limits<double>::max(),
                std::numeric_limits<double>::max()),
            entwine::Point(
                std::numeric_limits<double>::lowest(),
                std::numeric_limits<double>::lowest(),
                std::numeric_limits<double>::lowest()),
            true);

    // Determine the number of points and the bounds for this resource.
    {
        entwine::DimList dims;
        const auto type(pdal::Dimension::Type::Double);

        dims.push_back(entwine::DimInfo("X", pdal::Dimension::Id::X, type));
        dims.push_back(entwine::DimInfo("Y", pdal::Dimension::Id::Y, type));
        dims.push_back(entwine::DimInfo("Z", pdal::Dimension::Id::Z, type));

        entwine::Schema xyzSchema(dims);
        entwine::Executor executor(xyzSchema, true);

        auto bounder([this](pdal::PointView& view)
        {
            entwine::Point p;
            m_numPoints += view.size();

            for (std::size_t i = 0; i < view.size(); ++i)
            {
                p.x = view.getFieldAs<double>(pdal::Dimension::Id::X, i);
                p.y = view.getFieldAs<double>(pdal::Dimension::Id::Y, i);
                p.z = view.getFieldAs<double>(pdal::Dimension::Id::Z, i);

                m_bbox->grow(p);
            }
        });

        if (!executor.run(path, nullptr, bounder))
        {
            throw std::runtime_error("Could not execute resource: " + path);
        }
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

