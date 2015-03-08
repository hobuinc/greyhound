#include <pdal/PointContext.hpp>
#include <entwine/types/schema.hpp>

#include "util/schema.hpp"
#include "read-queries/multi.hpp"

MultiReadQuery::MultiReadQuery(
        const entwine::Schema& schema,
        bool compress,
        bool rasterize,
        std::shared_ptr<SleepyTree> sleepyTree,
        MultiResults multiResults)
    : ReadQuery(
            sleepyTree->pointContext().dimTypes(),
            schema,
            compress,
            rasterize)
    , m_sleepyTree(sleepyTree)
    , m_multiResults(multiResults)
    , m_offset(0)
{
    std::cout << "TODO: read-queries/multi.cpp - readPoint\n\t" <<
        "Dimension request casting" << std::endl;
}

void MultiReadQuery::readPoint(
        uint8_t* pos,
        const entwine::Schema& schema,
        bool rasterize) const
{
    std::shared_ptr<std::vector<char>> data(
            m_sleepyTree->data(m_multiResults[m_index].first));

    const pdal::PointContextRef pointContext(m_sleepyTree->pointContext());

    // TODO
    if (rasterize) throw std::runtime_error("No raster for multi yet.");

    const std::size_t pointOffset(m_multiResults[m_index].second);

    std::vector<char> bytes(8);

    for (const auto& dim : schema.dims())
    {
        if (Util::use(dim, rasterize) && pointContext.hasDim(dim.id()))
        {
            const std::size_t dimOffset(pointContext.dimOffset(dim.id()));
            const std::size_t nativeSize(pointContext.dimSize(dim.id()));

            std::memcpy(
                    bytes.data(),
                    data->data() + pointOffset + dimOffset,
                    nativeSize);

            using namespace pdal::Dimension;
            const BaseType::Enum baseType(base(dim.type()));
            const std::size_t dimSize(dim.size());

            if (baseType == BaseType::Floating)
            {
                if (dimSize != nativeSize)
                {
                    if (nativeSize == 8)
                    {
                        double d(0);
                        std::memcpy(&d, bytes.data(), 8);
                        float f(static_cast<float>(d));
                        std::memcpy(pos, &f, 4);
                    }
                    else
                    {
                        float f(0);
                        std::memcpy(&f, bytes.data() + 4, 4);
                        double d(static_cast<double>(f));
                        std::memcpy(pos, &d, 8);
                    }
                }
            }
            else
            {
                if (nativeSize != dimSize) throw std::runtime_error("TODO");
                std::memcpy(pos, bytes.data(), dimSize);
            }

            pos += dimSize;
        }
    }
}

bool MultiReadQuery::eof() const
{
    return m_index == numPoints();
}

std::size_t MultiReadQuery::numPoints() const
{
    return m_multiResults.size();
}

