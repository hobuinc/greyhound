#pragma once

#include <vector>
#include <string>

#include <entwine/http/s3.hpp>

struct Paths
{
    Paths(
            entwine::S3Info s3Info,
            std::vector<std::string> inputs,
            std::string output)
        : s3Info(s3Info)
        , inputs(inputs)
        , output(output)
    { }

    Paths(const Paths& other)
        : s3Info(other.s3Info)
        , inputs(other.inputs)
        , output(other.output)
    { }

    const entwine::S3Info s3Info;
    const std::vector<std::string> inputs;
    const std::string output;
};

