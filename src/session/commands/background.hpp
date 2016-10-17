#pragma once

#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>

#include <entwine/types/bounds.hpp>

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

static inline entwine::Bounds parseBounds(const v8::Local<v8::Value>& jsBounds)
{
    entwine::Bounds bounds;

    try
    {
        std::string boundsStr(std::string(
                    *v8::String::Utf8Value(jsBounds->ToString())));

        Json::Reader reader;
        Json::Value json;

        reader.parse(boundsStr, json, false);
        bounds = entwine::Bounds(json);
    }
    catch (...)
    {
        std::cout << "Invalid Bounds in query." << std::endl;
    }

    return bounds;
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

    virtual ~Background() { }

    Status status;
};

