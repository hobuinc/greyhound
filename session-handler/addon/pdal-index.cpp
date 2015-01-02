#include "pdal-index.hpp"

void PdalIndex::ensureIndex(
        const IndexType indexType,
        const pdal::PointBufferPtr pointBufferPtr)
{
    const pdal::PointBuffer& pointBuffer(*pointBufferPtr.get());

    switch (indexType)
    {
        case KdIndex2d:
            m_kd2dOnce.ensure([this, &pointBuffer]() {
                m_kdIndex2d.reset(new pdal::KDIndex(pointBuffer));
                m_kdIndex2d->build(false);
            });
            break;
        case KdIndex3d:
            m_kd3dOnce.ensure([this, &pointBuffer]() {
                m_kdIndex3d.reset(new pdal::KDIndex(pointBuffer));
                m_kdIndex3d->build(true);
            });
            break;
        case QuadIndex:
            m_quadOnce.ensure([this, &pointBuffer]() {
                m_quadIndex.reset(new pdal::QuadIndex(pointBuffer));
                m_quadIndex->build();
            });
            break;
        default:
            throw std::runtime_error("Invalid PDAL index type");
    }
}

