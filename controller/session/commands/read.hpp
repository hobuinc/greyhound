#pragma once

#include <memory>
#include <vector>
#include <mutex>

#include <v8.h>
#include <node.h>

#include <pdal/Dimension.hpp>
#include <entwine/types/bbox.hpp>
#include <entwine/types/schema.hpp>

#include "types/raster-meta.hpp"
#include "commands/background.hpp"

void errorCb(
        v8::Persistent<v8::Function> callback,
        std::string errMsg);

class ReadQuery;
class ItcBufferPool;
class ItcBuffer;
class Session;

class ReadCommand : public Background
{
public:
    ReadCommand(
            std::shared_ptr<Session> session,
            ItcBufferPool& itcBufferPool,
            std::string readId,
            bool compress,
            entwine::DimList dims,
            v8::Persistent<v8::Function> readCb,
            v8::Persistent<v8::Function> dataCb);
    virtual ~ReadCommand();

    virtual void read(std::size_t maxNumBytes);
    virtual bool rasterize() const { return false; }

    void run();

    void cancel(bool cancel);
    std::shared_ptr<ItcBuffer> getBuffer();
    ItcBufferPool& getBufferPool();

    bool done() const;
    void acquire();

    std::size_t numPoints() const;
    std::size_t numBytes() const;

    std::string readId()    const;
    bool        cancel()    const;
    v8::Persistent<v8::Function> readCb() const;
    v8::Persistent<v8::Function> dataCb() const;

    uv_async_t* async() { return m_async; }

protected:
    virtual void query() = 0;

    std::shared_ptr<Session> m_session;

    ItcBufferPool& m_itcBufferPool;
    std::shared_ptr<ItcBuffer> m_itcBuffer;
    uv_async_t* m_async;

    const std::string m_readId;
    const bool m_compress;
    const entwine::Schema m_schema;
    std::size_t m_numSent;
    std::shared_ptr<ReadQuery> m_readQuery;

    v8::Persistent<v8::Function> m_readCb;
    v8::Persistent<v8::Function> m_dataCb;
    bool m_cancel;
};

class ReadCommandUnindexed : public ReadCommand
{
public:
    ReadCommandUnindexed(
            std::shared_ptr<Session> session,
            ItcBufferPool& itcBufferPool,
            std::string readId,
            bool compress,
            entwine::DimList dims,
            v8::Persistent<v8::Function> readCb,
            v8::Persistent<v8::Function> dataCb);

private:
    virtual void query();
};

class ReadCommandQuadIndex : public ReadCommand
{
public:
    ReadCommandQuadIndex(
            std::shared_ptr<Session> session,
            ItcBufferPool& itcBufferPool,
            std::string readId,
            bool compress,
            entwine::DimList dims,
            entwine::BBox bbox,
            std::size_t depthBegin,
            std::size_t depthEnd,
            v8::Persistent<v8::Function> readCb,
            v8::Persistent<v8::Function> dataCb);

protected:
    virtual void query();

    const entwine::BBox m_bbox;
    const std::size_t m_depthBegin;
    const std::size_t m_depthEnd;
};

class ReadCommandRastered : public ReadCommand
{
public:
    ReadCommandRastered(
            std::shared_ptr<Session> session,
            ItcBufferPool& itcBufferPool,
            std::string readId,
            bool compress,
            entwine::DimList dims,
            entwine::BBox bbox,
            std::size_t level,
            v8::Persistent<v8::Function> readCb,
            v8::Persistent<v8::Function> dataCb);

    virtual void read(std::size_t maxNumBytes);

    virtual bool rasterize() const { return true; }
    RasterMeta rasterMeta() const { return m_rasterMeta; }

protected:
    virtual void query();

    const entwine::BBox m_bbox;
    const std::size_t m_level;
    RasterMeta m_rasterMeta;
};

class ReadCommandFactory
{
public:
    static ReadCommand* create(
            std::shared_ptr<Session> session,
            ItcBufferPool& itcBufferPool,
            std::string readId,
            entwine::DimList dims,
            bool compress,
            v8::Local<v8::Object> query,
            v8::Persistent<v8::Function> readCb,
            v8::Persistent<v8::Function> dataCb);
};

