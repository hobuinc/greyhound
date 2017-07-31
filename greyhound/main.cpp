#include <map>
#include <mutex>

#include <pdal/Dimension.hpp>
#include <pdal/Compression.hpp>

#include <entwine/reader/cache.hpp>
#include <entwine/reader/reader.hpp>
#include <entwine/types/manifest.hpp>
#include <entwine/types/reprojection.hpp>
#include <entwine/types/schema.hpp>
#include <entwine/util/compression.hpp>
#include <entwine/util/unique.hpp>

#include <greyhound/configuration.hpp>
#include <greyhound/defs.hpp>
#include <greyhound/manager.hpp>
#include <greyhound/resource.hpp>
#include <greyhound/router.hpp>

using namespace greyhound;

int main(int argv, char** argc)
{
    Configuration config(argv, argc);
    Router router(config);

    router.get(routes::info, [&](Resource& resource, Req& req, Res& res)
    {
        resource.info(req, res);
    });

    router.get(routes::hierarchy, [&](Resource& resource, Req& req, Res& res)
    {
        resource.hierarchy(req, res);
    });

    router.get(routes::read, [&](Resource& resource, Req& req, Res& res)
    {
        resource.read(req, res);
    });

    router.get(routes::filesRoot, [&](Resource& resource, Req& req, Res& res)
    {
        resource.files(req, res);
    });

    router.get(routes::files, [&](Resource& resource, Req& req, Res& res)
    {
        resource.files(req, res);
    });

    router.start();
}

