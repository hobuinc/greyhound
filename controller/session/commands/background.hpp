#pragma once

#include <functional>
#include <stdexcept>
#include <string>

#include <entwine/types/bbox.hpp>

#include "status.hpp"

static inline v8::Local<v8::String> toSymbol(
        v8::Isolate* isolate,
        const std::string& str)
{
    return v8::String::NewFromUtf8(isolate, str.c_str());
}

static inline entwine::Point parsePoint(const v8::Local<v8::Value>& jsPoint)
{
    entwine::Point p(0, 0, 0);
    if (!jsPoint->IsNull())
    {
        try
        {
            std::string pointStr(std::string(
                        *v8::String::Utf8Value(jsPoint->ToString())));

            Json::Reader reader;
            Json::Value rawPoint;

            reader.parse(pointStr, rawPoint, false);

            if (rawPoint.isArray() && rawPoint.size() == 3)
            {
                p = entwine::Point(
                        rawPoint[0].asDouble(),
                        rawPoint[1].asDouble(),
                        rawPoint[2].asDouble());
            }
            else
            {
                throw std::runtime_error("Invalid point");
            }
        }
        catch (...)
        {
            std::cout << "Invalid point in query" << std::endl;
        }
    }

    return p;
}

static inline entwine::BBox parseBBox(const v8::Local<v8::Value>& jsBBox)
{
    entwine::BBox bbox;

    try
    {
        std::string bboxStr(std::string(
                    *v8::String::Utf8Value(jsBBox->ToString())));

        Json::Reader reader;
        Json::Value bounds;

        reader.parse(bboxStr, bounds, false);
        bbox = entwine::BBox(bounds);
    }
    catch (...)
    {
        std::cout << "Invalid BBox in query." << std::endl;
    }

    return bbox;
}

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

