#pragma once

#include <vector>
#include <memory>

#include <pdal/PointBuffer.hpp>
#include <pdal/QuadIndex.hpp>

class Schema;
class ItcBuffer;
class DimInfo;
class QueryData;

class QueryData
{
public:
    virtual std::size_t numPoints() const = 0;
    virtual bool done() const = 0;
    virtual bool serial() const { return false; }

    virtual void read(
            std::shared_ptr<ItcBuffer> buffer,
            std::size_t maxNumBytes,
            const Schema& schema,
            bool rasterize = false) = 0;

protected:
    std::size_t readDim(
            uint8_t* buffer,
            const std::shared_ptr<pdal::PointBuffer> pointBuffer,
            const DimInfo& dim,
            const std::size_t index);
};

class UnindexedQueryData : public QueryData
{
public:
    UnindexedQueryData(
            std::shared_ptr<pdal::PointBuffer> pointBuffer,
            std::size_t start,
            std::size_t count);

    virtual std::size_t numPoints() const { return m_end - m_begin; }
    virtual bool done() const { return m_index == m_end; }
    virtual void read(
            std::shared_ptr<ItcBuffer> buffer,
            std::size_t maxNumBytes,
            const Schema& schema,
            bool rasterize = false);

private:
    const std::shared_ptr<pdal::PointBuffer> m_pointBuffer;

    const std::size_t m_begin;
    const std::size_t m_end;

    std::size_t m_index;
};

class LiveQueryData : public QueryData
{
public:
    LiveQueryData(
            std::shared_ptr<pdal::PointBuffer> pointBuffer,
            std::vector<std::size_t> indexList);

    const std::vector<std::size_t>& indexList() { return m_indexList; }
    virtual bool done() const { return m_index == m_indexList.size(); }
    virtual void read(
            std::shared_ptr<ItcBuffer> buffer,
            std::size_t maxNumBytes,
            const Schema& schema,
            bool rasterize = false);

    virtual std::size_t numPoints() const { return m_indexList.size(); }

private:
    const std::shared_ptr<pdal::PointBuffer> m_pointBuffer;

    const std::vector<std::size_t> m_indexList;

    std::size_t m_index;
};

class SerialQueryData : public QueryData
{
public:
    SerialQueryData(GreyQuery greyQuery);

    virtual std::size_t numPoints() const { return m_greyQuery.numPoints(); }
    virtual bool done() const { return m_index == numPoints(); }
    virtual void read(
            std::shared_ptr<ItcBuffer> buffer,
            std::size_t maxNumBytes,
            const Schema& schema,
            bool rasterize = false);

    virtual bool serial() const { return true; }

private:
    GreyQuery m_greyQuery;

    std::size_t m_index;
};

