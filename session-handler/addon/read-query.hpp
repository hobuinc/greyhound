#pragma once

#include <vector>
#include <memory>

#include <pdal/PointBuffer.hpp>
#include <pdal/QuadIndex.hpp>
#include <pdal/Compression.hpp>

#include "compression-stream.hpp"

class Schema;
class ItcBuffer;
class DimInfo;
class QueryData;

class QueryData
{
public:
    QueryData(
            pdal::DimTypeList dimTypes,
            const Schema& schema,
            bool compress,
            bool rasterize,
            std::size_t index = 0);

    virtual std::size_t numPoints() const = 0;

    bool done() const;
    bool compress() const { return m_compressor.get() != 0; }
    virtual bool serial() const { return false; }

    void read(
            std::shared_ptr<ItcBuffer> buffer,
            std::size_t maxNumBytes,
            bool rasterize = false);

protected:
    std::size_t readDim(
            uint8_t* buffer,
            const std::shared_ptr<pdal::PointBuffer> pointBuffer,
            const DimInfo& dim,
            const std::size_t index) const;

    CompressionStream m_compressionStream;
    std::shared_ptr<pdal::LazPerfCompressor<CompressionStream>> m_compressor;
    std::size_t m_compressionOffset;

    const Schema& m_schema;
    const bool m_rasterize;
    std::size_t m_index;

private:
    virtual void readPoint(
            uint8_t* pos,
            const Schema& schema,
            bool rasterize) const = 0;

    virtual bool eof() const = 0;
};

class UnindexedQueryData : public QueryData
{
public:
    UnindexedQueryData(
            const Schema& schema,
            bool compress,
            std::shared_ptr<pdal::PointBuffer> pointBuffer,
            std::size_t start,
            std::size_t count);

    virtual std::size_t numPoints() const { return m_end - m_begin; }
    virtual bool eof() const { return m_index == m_end; }

private:
    const std::shared_ptr<pdal::PointBuffer> m_pointBuffer;

    const std::size_t m_begin;
    const std::size_t m_end;

    virtual void readPoint(
            uint8_t* pos,
            const Schema& schema,
            bool rasterize) const;
};

class LiveQueryData : public QueryData
{
public:
    LiveQueryData(
            const Schema& schema,
            bool compress,
            bool rasterize,
            std::shared_ptr<pdal::PointBuffer> pointBuffer,
            std::vector<std::size_t> indexList);

    const std::vector<std::size_t>& indexList() { return m_indexList; }
    virtual bool eof() const { return m_index == m_indexList.size(); }

    virtual std::size_t numPoints() const { return m_indexList.size(); }

private:
    const std::shared_ptr<pdal::PointBuffer> m_pointBuffer;

    const std::vector<std::size_t> m_indexList;

    virtual void readPoint(
            uint8_t* pos,
            const Schema& schema,
            bool rasterize) const;
};

class SerialQueryData : public QueryData
{
public:
    SerialQueryData(
            const Schema& schema,
            bool compress,
            bool rasterize,
            GreyQuery greyQuery);

    virtual std::size_t numPoints() const { return m_greyQuery.numPoints(); }
    virtual bool eof() const { return m_index == numPoints(); }

    virtual bool serial() const { return true; }

private:
    GreyQuery m_greyQuery;

    virtual void readPoint(
            uint8_t* pos,
            const Schema& schema,
            bool rasterize) const;
};

