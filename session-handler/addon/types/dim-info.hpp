#pragma once

#include <cstdint>

#include <pdal/Dimension.hpp>

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

