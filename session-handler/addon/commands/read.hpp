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

void errorCallback(
        v8::Persistent<v8::Function> callback,
        std::string errMsg);

class ReadQuery;
class ItcBufferPool;
class ItcBuffer;
class PdalSession;

class ReadCommand : public Background
{
public:
    ReadCommand(
            std::shared_ptr<PdalSession> pdalSession,
            ItcBufferPool& itcBufferPool,
            std::string readId,
            std::string host,
            std::size_t port,
            bool compress,
            const entwine::Schema& schema,
            v8::Persistent<v8::Function> queryCallback,
            v8::Persistent<v8::Function> dataCallback);
    virtual ~ReadCommand();

    virtual void read(std::size_t maxNumBytes);
    virtual bool rasterize() const { return false; }

    void run();

    void cancel(bool cancel);
    std::string& errMsg();
    std::shared_ptr<ItcBuffer> getBuffer();
    ItcBufferPool& getBufferPool();

    bool done() const;
    void acquire();

    std::size_t numPoints() const;
    std::size_t numBytes() const;

    std::string readId()    const;
    bool        cancel()    const;
    v8::Persistent<v8::Function> queryCallback() const;
    v8::Persistent<v8::Function> dataCallback() const;

    uv_async_t* async() { return m_async; }

protected:
    virtual void query() = 0;

    std::shared_ptr<PdalSession> m_pdalSession;

    ItcBufferPool& m_itcBufferPool;
    std::shared_ptr<ItcBuffer> m_itcBuffer;
    uv_async_t* m_async;

    const std::string m_readId;
    const std::string m_host;
    const std::size_t m_port;
    const bool m_compress;
    const entwine::Schema m_schema;
    std::size_t m_numSent;
    std::shared_ptr<ReadQuery> m_readQuery;

    v8::Persistent<v8::Function> m_queryCallback;
    v8::Persistent<v8::Function> m_dataCallback;
    bool m_cancel;

    std::string m_errMsg;

private:
    entwine::Schema schemaOrDefault(entwine::Schema reqSchema);
};

class ReadCommandUnindexed : public ReadCommand
{
public:
    ReadCommandUnindexed(
            std::shared_ptr<PdalSession> pdalSession,
            ItcBufferPool& itcBufferPool,
            std::string readId,
            std::string host,
            std::size_t port,
            bool compress,
            entwine::Schema schema,
            std::size_t start,
            std::size_t count,
            v8::Persistent<v8::Function> queryCallback,
            v8::Persistent<v8::Function> dataCallback);

private:
    virtual void query();

    const std::size_t m_start;
    const std::size_t m_count;
};

class ReadCommandQuadIndex : public ReadCommand
{
public:
    ReadCommandQuadIndex(
            std::shared_ptr<PdalSession> pdalSession,
            ItcBufferPool& itcBufferPool,
            std::string readId,
            std::string host,
            std::size_t port,
            bool compress,
            entwine::Schema schema,
            std::size_t depthBegin,
            std::size_t depthEnd,
            v8::Persistent<v8::Function> queryCallback,
            v8::Persistent<v8::Function> dataCallback);

protected:
    virtual void query();

    const std::size_t m_depthBegin;
    const std::size_t m_depthEnd;
};

class ReadCommandBoundedQuadIndex : public ReadCommandQuadIndex
{
public:
    ReadCommandBoundedQuadIndex(
            std::shared_ptr<PdalSession> pdalSession,
            ItcBufferPool& itcBufferPool,
            std::string readId,
            std::string host,
            std::size_t port,
            bool compress,
            entwine::Schema schema,
            entwine::BBox bbox,
            std::size_t depthBegin,
            std::size_t depthEnd,
            v8::Persistent<v8::Function> queryCallback,
            v8::Persistent<v8::Function> dataCallback);

private:
    virtual void query();

    const entwine::BBox m_bbox;
};

class ReadCommandRastered : public ReadCommand
{
public:
    ReadCommandRastered(
            std::shared_ptr<PdalSession> pdalSession,
            ItcBufferPool& itcBufferPool,
            std::string readId,
            std::string host,
            std::size_t port,
            bool compress,
            entwine::Schema schema,
            v8::Persistent<v8::Function> queryCallback,
            v8::Persistent<v8::Function> dataCallback);

    ReadCommandRastered(
            std::shared_ptr<PdalSession> pdalSession,
            ItcBufferPool& itcBufferPool,
            std::string readId,
            std::string host,
            std::size_t port,
            bool compress,
            entwine::Schema schema,
            RasterMeta rasterMeta,
            v8::Persistent<v8::Function> queryCallback,
            v8::Persistent<v8::Function> dataCallback);

    virtual void read(std::size_t maxNumBytes);

    virtual bool rasterize() const { return true; }
    RasterMeta rasterMeta() const { return m_rasterMeta; }

protected:
    virtual void query();

    RasterMeta m_rasterMeta;
};

class ReadCommandQuadLevel : public ReadCommandRastered
{
public:
    ReadCommandQuadLevel(
            std::shared_ptr<PdalSession> pdalSession,
            ItcBufferPool& itcBufferPool,
            std::string readId,
            std::string host,
            std::size_t port,
            bool compress,
            entwine::Schema schema,
            std::size_t level,
            v8::Persistent<v8::Function> queryCallback,
            v8::Persistent<v8::Function> dataCallback);

private:
    virtual void query();

    const std::size_t m_level;
};

class ReadCommandFactory
{
public:
    static ReadCommand* create(
            std::shared_ptr<PdalSession> pdalSession,
            ItcBufferPool& itcBufferPool,
            std::string readId,
            const v8::Arguments& args);
};

