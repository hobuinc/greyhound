#pragma once

#include <pdal/Dimension.hpp>

#include <json/json.h>

struct RasterMeta
{
    RasterMeta() : xBegin(), xEnd(), xStep(), yBegin(), yEnd(), yStep() { }

    RasterMeta(
            double xBegin,
            double xEnd,
            double xStep,
            double yBegin,
            double yEnd,
            double yStep)
        : xBegin(xBegin)
        , xEnd(xEnd)
        , xStep(xStep)
        , yBegin(yBegin)
        , yEnd(yEnd)
        , yStep(yStep)
    { }

    double xBegin;
    double xEnd;
    double xStep;
    double yBegin;
    double yEnd;
    double yStep;

    std::size_t xNum() const { return std::round((xEnd - xBegin) / xStep); }
    std::size_t yNum() const { return std::round((yEnd - yBegin) / yStep); }
};

struct DimInfo
{
    DimInfo(std::string name, std::string type, std::size_t size)
        : id(pdal::Dimension::id(name)), type(type), size(size)
    { }

    DimInfo(
            const pdal::Dimension::Id::Enum id,
            const pdal::Dimension::Type::Enum type)
        : id(id)
        , type(pdal::Dimension::toName(pdal::Dimension::base(type)))
        , size(pdal::Dimension::size(type))
    { }

    const pdal::Dimension::Id::Enum id;
    const std::string type;
    const std::size_t size;
};

struct Schema
{
    explicit Schema(std::vector<DimInfo> dims) : dims(dims) { }
    const std::vector<DimInfo> dims;

    std::size_t stride(bool rasterize = false) const
    {
        std::size_t stride(0);

        for (const auto& dim : dims)
        {
            if (!rasterize || !Schema::rasterOmit(dim.id))
            {
                stride += dim.size;
            }
        }

        if (rasterize)
        {
            // Clientward rasterization schemas always contain a byte to specify
            // whether a point at this location in the raster exists.
            ++stride;
        }

        return stride;
    }

    bool use(const DimInfo& dim, bool rasterize) const
    {
        return !rasterize || !Schema::rasterOmit(dim.id);
    }

    static bool rasterOmit(pdal::Dimension::Id::Enum id)
    {
        // These Dimensions are not explicitly placed in the output buffer
        // for rasterized requests.
        return id == pdal::Dimension::Id::X || id == pdal::Dimension::Id::Y;
    }

};

struct Point
{
    Point() : x(0), y(0) { }
    Point(double x, double y) : x(x), y(y) { }
    Point(const Point& other) : x(other.x), y(other.y) { }

    // Calculates the distance-squared to another point.
    double sqDist(const Point& other) const
    {
        return (x - other.x) * (x - other.x) + (y - other.y) * (y - other.y);
    }

    double x;
    double y;
};

class BBox
{
public:
    BBox();
    BBox(Point min, Point max);
    BBox(const BBox& other);

    Point min() const;
    Point max() const;
    Point mid() const;

    void min(const Point& p);
    void max(const Point& p);

    // Returns true if this BBox shares any area in common with another.
    bool overlaps(const BBox& other) const;

    // Returns true if the requested point is contained within this BBox.
    bool contains(const Point& p) const;

    // Returns true if the requested BBox is entirely contained within this one.
    bool contains(const BBox& b) const;

    double width() const;
    double height() const;

    BBox getNw() const;
    BBox getNe() const;
    BBox getSw() const;
    BBox getSe() const;

    Json::Value toJson() const;
    static BBox fromJson(const Json::Value& json);

private:
    Point m_min;
    Point m_max;
};

