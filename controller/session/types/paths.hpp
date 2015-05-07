#pragma once

#include <vector>
#include <string>

#include <entwine/drivers/s3.hpp>

struct Paths
{
    Paths(std::vector<std::string> inputs, std::string output)
        : inputs(inputs)
        , output(output)
    { }

    Paths(const Paths& other)
        : inputs(other.inputs)
        , output(other.output)
    { }

    const std::vector<std::string> inputs;
    const std::string output;
};

