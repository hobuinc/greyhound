#pragma once

#include <sqlite3.h>

#include <pdal/util/Bounds.hpp>

#include "types.hpp"
#include "http/s3.hpp"

static const std::string greyVersion("0.0.1a");

static const uint64_t nwFlag(0);
static const uint64_t neFlag(1);
static const uint64_t swFlag(2);
static const uint64_t seFlag(3);

// Must be non-zero to distinguish between various levels of NW-only paths.
static const uint64_t baseId(3);

struct GreyMeta
{
    std::string version;
    std::size_t base;
    std::size_t cutoff;
    std::string pointContextXml;
    BBox bbox;
    std::size_t numPoints;
    std::string schema;
    bool compressed;
    std::string stats;
    std::string srs;
    std::vector<std::size_t> fills;

    GreyMeta()
        : version()
        , base()
        , cutoff()
        , pointContextXml()
        , bbox()
        , numPoints()
        , schema()
        , compressed()
        , stats()
        , srs()
        , fills()
    { }

    explicit GreyMeta(const Json::Value& json)
        : version           (json["version"].asString())
        , base              (json["base"].asUInt64())
        , pointContextXml   (json["pointContextXml"].asString())
        , bbox              (BBox::fromJson(json["bbox"]))
        , numPoints         (json["numPoints"].asUInt64())
        , schema            (json["schema"].asString())
        , compressed        (json["compressed"].asBool())
        , stats             (json["stats"].asString())
        , srs               (json["srs"].asString())
        , fills             (parseFills(json["fills"]))
    { }

    Json::Value toJson() const
    {
        Json::Value json;
        json["version"] =           version;
        json["base"] =              static_cast<Json::Value::UInt64>(base);
        json["pointContextXml"] =   pointContextXml;
        json["bbox"] =              bbox.toJson();
        json["numPoints"] =         static_cast<Json::Value::UInt64>(numPoints);
        json["schema"] =            schema;
        json["compressed"] =        compressed;
        json["stats"] =             stats;
        json["srs"] =               srs;

        Json::Value jsonFills;
        for (std::size_t i(0); i < fills.size(); ++i)
        {
            jsonFills.append(static_cast<Json::Value::UInt64>(fills[i]));
        }

        json["fills"] = jsonFills;
        return json;
    }

private:
    std::vector<std::size_t> parseFills(const Json::Value& json)
    {
        std::vector<std::size_t> ret;

        if (json.isArray())
        {
            for (Json::ArrayIndex i(0); i < json.size(); ++i)
            {
                ret.push_back(json[i].asUInt64());
            }
        }

        return ret;
    }
};

struct SerialPaths
{
    SerialPaths(S3Info s3Info, std::vector<std::string> diskPaths)
        : s3Info(s3Info)
        , diskPaths(diskPaths)
    { }

    SerialPaths(const SerialPaths& other)
        : s3Info(other.s3Info)
        , diskPaths(other.diskPaths)
    { }

    const S3Info s3Info;
    const std::vector<std::string> diskPaths;
};

