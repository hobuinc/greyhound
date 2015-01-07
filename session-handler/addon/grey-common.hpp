#pragma once

#include <sqlite3.h>

#include <pdal/Bounds.hpp>

static const std::string greyVersion("1.0");

static const uint64_t nwFlag(0);
static const uint64_t neFlag(1);
static const uint64_t swFlag(2);
static const uint64_t seFlag(3);

// Must be non-zero to distinguish between various levels of NW-only paths.
static const uint64_t baseId(3);

struct BBox
{
    BBox();
    BBox(const BBox& other);
    BBox(double xMin, double yMin, double xMax, double yMax);
    BBox(pdal::BOX3D bbox);

    double xMin;
    double yMin;
    double xMax;
    double yMax;

    double xMid() const;
    double yMid() const;

    bool overlaps(const BBox& other) const;
    bool contains(const BBox& other) const;

    double width() const;
    double height() const;

    BBox getNw() const;
    BBox getNe() const;
    BBox getSw() const;
    BBox getSe() const;
};

struct GreyMeta
{
    std::string version;
    std::size_t base;
    std::string pointContextXml;
    BBox bbox;
    std::size_t numPoints;
    std::string schema;
    bool compressed;
    std::string stats;
    std::string srs;
    std::vector<std::size_t> fills;
};

