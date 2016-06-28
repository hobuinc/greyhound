#include "source-manager.hpp"

#include <pdal/Options.hpp>
#include <pdal/Reader.hpp>
#include <pdal/StageFactory.hpp>

#include <entwine/types/bounds.hpp>
#include <entwine/types/pooled-point-table.hpp>
#include <entwine/types/schema.hpp>
#include <entwine/types/structure.hpp>
#include <entwine/util/executor.hpp>

namespace
{
    entwine::Schema xyzSchema(([]()
    {
        entwine::DimList dims;
        const auto type(pdal::Dimension::Type::Double);

        dims.push_back(entwine::DimInfo("X", pdal::Dimension::Id::X, type));
        dims.push_back(entwine::DimInfo("Y", pdal::Dimension::Id::Y, type));
        dims.push_back(entwine::DimInfo("Z", pdal::Dimension::Id::Z, type));

        return entwine::Schema(dims);
    })());
}

SourceManager::SourceManager(
        pdal::StageFactory& stageFactory,
        std::mutex& factoryMutex,
        std::string path,
        std::string driver)
    : m_stageFactory(stageFactory)
    , m_factoryMutex(factoryMutex)
    , m_options(new pdal::Options())
    , m_driver(driver)
    , m_schema()
    , m_bounds()
    , m_numPoints(0)
    , m_srs()
{
    m_options->add(pdal::Option("filename", path));

    entwine::Executor executor;

    auto preview(executor.preview(path, nullptr));
    if (preview)
    {
        m_numPoints = preview->numPoints;
        m_srs = preview->srs;
        m_bounds.reset(new entwine::Bounds(preview->bounds));

        entwine::DimList dims;
        for (const auto& name : preview->dimNames)
        {
            const pdal::Dimension::Id::Enum id(pdal::Dimension::id(name));
            dims.emplace_back(name, id, pdal::Dimension::defaultType(id));
        }

        m_schema.reset(new entwine::Schema(dims));
    }
    else
    {
        throw std::runtime_error("Could not create source");
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

