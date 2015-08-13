#pragma once

#include <memory>
#include <vector>
#include <mutex>

#include <node.h>
#include <uv.h>
#include <v8.h>

#include <pdal/Dimension.hpp>
#include <entwine/types/bbox.hpp>
#include <entwine/types/schema.hpp>

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
            v8::UniquePersistent<v8::Function> initCb,
            v8::UniquePersistent<v8::Function> dataCb);
    virtual ~ReadCommand();

    void registerInitCb();
    void registerDataCb();

    virtual void read(std::size_t maxNumBytes);

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
    v8::UniquePersistent<v8::Function>& initCb();
    v8::UniquePersistent<v8::Function>& dataCb();

    uv_async_t* initAsync() { return m_initAsync; }
    uv_async_t* dataAsync() { return m_dataAsync; }

    void doCb(uv_async_t* async)
    {
        m_wait = true;
        uv_async_send(async);
        std::unique_lock<std::mutex> lock(m_mutex);
        m_cv.wait(lock, [this]()->bool { return !m_wait; });
    }


    void notifyCb() {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_wait = false;
        lock.unlock();
        m_cv.notify_all();
    }

protected:
    virtual void query() = 0;

    std::shared_ptr<Session> m_session;

    ItcBufferPool& m_itcBufferPool;
    std::shared_ptr<ItcBuffer> m_itcBuffer;
    const std::string m_readId;
    const bool m_compress;
    const entwine::Schema m_schema;
    std::size_t m_numSent;
    std::shared_ptr<ReadQuery> m_readQuery;

    uv_async_t* m_initAsync;
    uv_async_t* m_dataAsync;
    v8::UniquePersistent<v8::Function> m_initCb;
    v8::UniquePersistent<v8::Function> m_dataCb;

    std::mutex m_mutex;
    std::condition_variable m_cv;
    bool m_wait;

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
            v8::UniquePersistent<v8::Function> initCb,
            v8::UniquePersistent<v8::Function> dataCb);

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
            v8::UniquePersistent<v8::Function> initCb,
            v8::UniquePersistent<v8::Function> dataCb);

protected:
    virtual void query();

    const entwine::BBox m_bbox;
    const std::size_t m_depthBegin;
    const std::size_t m_depthEnd;
};

class ReadCommandFactory
{
public:
    static ReadCommand* create(
            v8::Isolate* isolate,
            std::shared_ptr<Session> session,
            ItcBufferPool& itcBufferPool,
            std::string readId,
            entwine::DimList dims,
            bool compress,
            v8::Local<v8::Object> query,
            v8::UniquePersistent<v8::Function> initCb,
            v8::UniquePersistent<v8::Function> dataCb);
};

