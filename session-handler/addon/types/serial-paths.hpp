#pragma once

#include <vector>
#include <string>

#include <entwine/http/s3.hpp>

struct SerialPaths
{
    SerialPaths(entwine::S3Info s3Info, std::vector<std::string> diskPaths)
        : s3Info(s3Info)
        , diskPaths(diskPaths)
    { }

    SerialPaths(const SerialPaths& other)
        : s3Info(other.s3Info)
        , diskPaths(other.diskPaths)
    { }

    const entwine::S3Info s3Info;
    const std::vector<std::string> diskPaths;
};

