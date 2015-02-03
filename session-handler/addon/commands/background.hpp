#pragma once

class Background
{
public:
    void safe(std::string& err, std::function<void()> f)
    {
        try
        {
            f();
        }
        catch (const std::runtime_error& e)
        {
            err = e.what();
        }
        catch (const std::bad_alloc& ba)
        {
            err = "Caught bad alloc";
        }
        catch (...)
        {
            err = "Unknown error";
        }
    }
};

