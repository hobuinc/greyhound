#pragma once

#include <pdal/Dimension.hpp>

#include <entwine/types/schema.hpp>

class Util
{
public:
    static bool rasterOmit(pdal::Dimension::Id::Enum id)
    {
        // These Dimensions are not explicitly placed in the output buffer
        // for rasterized requests.
        return id == pdal::Dimension::Id::X || id == pdal::Dimension::Id::Y;
    }

    static std::size_t stride(const entwine::Schema& schema, bool rasterize)
    {
        std::size_t stride(0);

        for (const auto& dim : schema.dims())
        {
            if (!rasterize || !rasterOmit(dim.id()))
            {
                stride += dim.size();
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

    static bool use(const entwine::DimInfo& dim, bool rasterize)
    {
        return !rasterize || !rasterOmit(dim.id());
    }
};

