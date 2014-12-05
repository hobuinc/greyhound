#pragma once

#include <string>

#include <sqlite3.h>

#include <pdal/QuadIndex.hpp>
#include <pdal/Bounds.hpp>

class GreyTree
{
public:
    struct GreyMeta
    {
        std::string pointContextXml;
        double xMin;
        double yMin;
        double xMax;
        double yMax;
        std::size_t numPoints;
        std::string schema;
        std::string stats;
        std::string srs;
        std::vector<std::size_t> fills;
    };

    GreyTree(const pdal::QuadIndex& quadIndex, GreyMeta meta);

    void write(std::string filename, std::size_t baseLevel) const;

private:
    struct BBox
    {
        BBox(double xMin, double yMin, double xMax, double yMax);
        BBox(pdal::BOX3D bbox);

        const double xMin;
        const double yMin;
        const double xMax;
        const double yMax;
        const double xMid;
        const double yMid;
    };

    const GreyMeta m_meta;
    const pdal::QuadIndex& m_quadIndex;

    void dbWriteMeta(sqlite3* db) const;
    void dbWriteData(
            sqlite3* db,
            const std::map<uint64_t, std::vector<std::size_t>>& clusters) const;

    void build(
            std::map<uint64_t, std::vector<std::size_t>>& results,
            const BBox& bbox,
            std::size_t level,
            uint64_t id) const;

    std::vector<std::size_t> getPoints(
            const BBox& bbox,
            std::size_t level) const;
};

