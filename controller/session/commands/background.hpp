#pragma once

#include "status.hpp"

class Background
{
public:
    void safe(std::function<void()> f)
    {
        try
        {
            f();
        }
        catch (const std::runtime_error& e)
        {
            status.set(500, e.what());
        }
        catch (const std::bad_alloc& ba)
        {
            status.set(500, "Bad alloc");
        }
        catch (...)
        {
            status.set(500, "Unknown error");
        }
    }

    Status status;
};

