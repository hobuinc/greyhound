#pragma once

#include <pdal/KDIndex.hpp>
#include <pdal/QuadIndex.hpp>

#include "util/once.hpp"

// This is a convenience class that maintains the various index types that
// exist in a PDAL session.
class PdalIndex
{
public:
    PdalIndex()
        : m_kdIndex2d()
        , m_kdIndex3d()
        , m_quadIndex()
        , m_kd2dOnce()
        , m_kd3dOnce()
        , m_quadOnce()
    { }

    enum IndexType
    {
        KdIndex2d,
        KdIndex3d,
        QuadIndex
    };

    void ensureIndex(
            IndexType indexType,
            const pdal::PointBufferPtr pointBufferPtr);

    const pdal::KDIndex& kdIndex2d()    { return *m_kdIndex2d.get(); }
    const pdal::KDIndex& kdIndex3d()    { return *m_kdIndex3d.get(); }
    const pdal::QuadIndex& quadIndex()  { return *m_quadIndex.get(); }

private:
    std::unique_ptr<pdal::KDIndex>      m_kdIndex2d;
    std::unique_ptr<pdal::KDIndex>      m_kdIndex3d;
    std::unique_ptr<pdal::QuadIndex>    m_quadIndex;

    Once m_kd2dOnce;
    Once m_kd3dOnce;
    Once m_quadOnce;

    // Disallow copy/assignment.
    PdalIndex(const PdalIndex&);
    PdalIndex& operator=(const PdalIndex&);
};

