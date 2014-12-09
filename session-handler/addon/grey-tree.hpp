#pragma once

#include <string>
#include <mutex>

#include <sqlite3.h>

#include <pdal/QuadIndex.hpp>
#include <pdal/Bounds.hpp>

#include "read-command.hpp"

struct BBox
{
    BBox();
    BBox(const BBox& other);
    BBox(double xMin, double yMin, double xMax, double yMax);
    BBox(pdal::BOX3D bbox);

    double xMin;
    double yMin;
    double xMax;
    double yMax;

    double xMid() const { return xMin + (xMax - xMin) / 2.0; }
    double yMid() const { return yMin + (yMax - yMin) / 2.0; }

    bool overlaps(const BBox& other) const;
    bool contains(const BBox& other) const;

    double width() const  { return xMax - xMin; }
    double height() const { return yMax - yMin; }

    BBox getNw() const { return BBox(xMin, yMid(), xMid(), yMax); }
    BBox getNe() const { return BBox(xMid(), yMid(), xMax, yMax); }
    BBox getSw() const { return BBox(xMin, yMin, xMid(), yMid()); }
    BBox getSe() const { return BBox(xMid(), yMin, xMax, yMid()); }
};

struct GreyMeta
{
    std::string version;
    std::size_t base;
    std::string pointContextXml;
    BBox bbox;
    std::size_t numPoints;
    std::string schema;
    std::string stats;
    std::string srs;
    std::vector<std::size_t> fills;
};

class GreyWriter
{
public:
    GreyWriter(const pdal::QuadIndex& quadIndex, GreyMeta meta);

    void write(std::string filename) const;

private:
    const GreyMeta m_meta;
    const pdal::QuadIndex& m_quadIndex;

    void writeMeta(sqlite3* db) const;
    void writeData(
            sqlite3* db,
            const std::map<uint64_t, std::vector<std::size_t>>& clusters) const;

    void build(
            std::map<uint64_t, std::vector<std::size_t>>& results,
            const BBox& bbox,
            std::size_t level,
            uint64_t id) const;

    std::vector<std::size_t> getPoints(
            const BBox& bbox,
            std::size_t level) const;
};

class IdTree
{
public:
    IdTree(
            uint64_t id,
            std::size_t currentLevel,
            std::size_t endLevel);

    void find(
            std::vector<uint64_t>& results,
            std::size_t queryLevelBegin,
            std::size_t queryLevelEnd,
            std::size_t currentLevel) const;

    void find(
            std::vector<uint64_t>& completeResults,
            std::vector<uint64_t>& partialResults,
            std::size_t queryLevelBegin,
            std::size_t queryLevelEnd,
            const BBox& queryBBox,
            std::size_t currentLevel,
            BBox        currentBBox) const;

    std::size_t info(
            uint64_t id,
            std::size_t offset,
            BBox&       resultBBox,
            std::size_t currentLevel,
            BBox        currentBBox);

private:
    const uint64_t id;
    std::unique_ptr<IdTree> nw;
    std::unique_ptr<IdTree> ne;
    std::unique_ptr<IdTree> sw;
    std::unique_ptr<IdTree> se;
};

class IdIndex
{
public:
    IdIndex(const GreyMeta& meta);

    void find(
            std::vector<uint64_t>& completeResults,
            std::vector<uint64_t>& partialResults,
            std::size_t depthLevelBegin,
            std::size_t depthLevelEnd) const;

    void find(
            std::vector<uint64_t>& completeResults,
            std::vector<uint64_t>& partialResults,
            std::size_t depthLevelBegin,
            std::size_t depthLevelEnd,
            BBox queryBBox) const;

private:
    const std::size_t m_base;
    const IdTree m_idTree;
    const BBox m_bbox;
};

class GreyCluster
{
public:
    GreyCluster(
            std::shared_ptr<pdal::PointBuffer> pointBuffer,
            std::size_t depthBegin);

    bool indexed() const { return m_quadTree.get() != 0; }

private:
    std::shared_ptr<pdal::PointBuffer> m_pointBuffer;
    std::shared_ptr<pdal::QuadIndex> m_quadTree;
};

class GreyReader
{
public:
    GreyReader(std::string pipelineId);
    ~GreyReader();

    std::size_t getNumPoints() const            { return m_meta.numPoints;  }
    std::string getSchema() const               { return m_meta.schema;     }
    std::string getStats() const                { return m_meta.stats;      }
    std::string getSrs() const                  { return m_meta.srs;        }
    std::vector<std::size_t> getFills() const   { return m_meta.fills;      }

    void read(
            std::vector<uint8_t>& buffer,
            const Schema& schema,
            std::size_t depthBegin,
            std::size_t depthEnd);

private:
    sqlite3* m_db;
    GreyMeta m_meta;
    std::unique_ptr<IdIndex> m_idIndex;

    pdal::PointContext m_pointContext;

    std::mutex m_cacheMutex;
    std::map<uint64_t, std::shared_ptr<GreyCluster>> m_cache;

    void readMeta();

    // Not implemented.
    GreyReader(const GreyReader&);
    GreyReader& operator=(const GreyReader&);
};

