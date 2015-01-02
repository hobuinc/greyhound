#include "grey-common.hpp"

BBox::BBox()
    : xMin(0)
    , yMin(0)
    , xMax(0)
    , yMax(0)
{ }

BBox::BBox(const BBox& other)
    : xMin(other.xMin)
    , yMin(other.yMin)
    , xMax(other.xMax)
    , yMax(other.yMax)
{ }

BBox::BBox(double xMin, double yMin, double xMax, double yMax)
    : xMin(xMin)
    , yMin(yMin)
    , xMax(xMax)
    , yMax(yMax)
{ }

BBox::BBox(pdal::BOX3D bbox)
    : xMin(bbox.minx)
    , yMin(bbox.miny)
    , xMax(bbox.maxx)
    , yMax(bbox.maxy)
{ }

double BBox::xMid() const { return xMin + (xMax - xMin) / 2.0; }
double BBox::yMid() const { return yMin + (yMax - yMin) / 2.0; }

bool BBox::overlaps(const BBox& other) const
{
    return
        std::abs(xMid() - other.xMid()) < width()  / 2 + other.width()  / 2 &&
        std::abs(yMid() - other.yMid()) < height() / 2 + other.height() / 2;
}

bool BBox::contains(const BBox& other) const
{
    return
        xMin <= other.xMin && xMax >= other.xMax &&
        yMin <= other.yMin && yMax >= other.yMax;
}

double BBox::width() const  { return xMax - xMin; }
double BBox::height() const { return yMax - yMin; }

BBox BBox::getNw() const { return BBox(xMin, yMid(), xMid(), yMax); }
BBox BBox::getNe() const { return BBox(xMid(), yMid(), xMax, yMax); }
BBox BBox::getSw() const { return BBox(xMin, yMin, xMid(), yMid()); }
BBox BBox::getSe() const { return BBox(xMid(), yMin, xMax, yMid()); }

