#pragma once

#include <cmath>

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

