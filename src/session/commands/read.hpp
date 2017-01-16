#pragma once

#include <memory>
#include <vector>
#include <mutex>

#include <node.h>
#include <uv.h>
#include <v8.h>

#include <pdal/Dimension.hpp>
#include <entwine/types/bounds.hpp>
#include <entwine/types/schema.hpp>

#include "commands/background.hpp"
#include "read-queries/base.hpp"
#include "util/buffer-pool.hpp"

class Session;

struct AsyncDeleter
{
    void operator()(uv_async_t* async)
    {
        auto handle(reinterpret_cast<uv_handle_t*>(async));
        uv_close(handle, (uv_close_cb)([](uv_handle_t* handle)
        {
            delete handle;
        }));
    }
};

class ReadCommand : public Background
{
public:
    ReadCommand(
            std::shared_ptr<Session> session,
            BufferPool& bufferPool,
            bool compress,
            const entwine::Point* scale,
            const entwine::Point* offset,
            std::string schemaString,
            std::string filterString,
            uv_loop_t* loop,
            v8::UniquePersistent<v8::Function> initCb,
            v8::UniquePersistent<v8::Function> dataCb);

    virtual ~ReadCommand() { }

    static ReadCommand* create(
            v8::Isolate* isolate,
            std::shared_ptr<Session> session,
            BufferPool& bufferPool,
            std::string schemaString,
            std::string filterString,
            bool compress,
            const entwine::Point* scale,
            const entwine::Point* offset,
            v8::Local<v8::Object> query,
            uv_loop_t* loop,
            v8::UniquePersistent<v8::Function> initCb,
            v8::UniquePersistent<v8::Function> dataCb);

    void registerInitCb();
    void registerDataCb();

    void init() { query(); }
    void read();

    std::vector<char>& buffer() { return *m_buffer; }

    bool done() const { return terminate() || m_readQuery->done(); }
    bool terminate() const { return m_terminate; }
    void terminate(bool val) { m_terminate = val; }

    v8::UniquePersistent<v8::Function>& initCb() { return m_initCb; }
    v8::UniquePersistent<v8::Function>& dataCb() { return m_dataCb; }

    uv_async_t* initAsync() { return m_initAsync.get(); }
    uv_async_t* dataAsync() { return m_dataAsync.get(); }

    void doCb(uv_async_t* async)
    {
        m_wait = true;
        uv_async_send(async);
        std::unique_lock<std::mutex> lock(m_mutex);
        m_cv.wait(lock, [this]()->bool { return !m_wait; });
    }

    void notifyCb()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_wait = false;
        lock.unlock();
        m_cv.notify_all();
    }

protected:
    virtual void query() = 0;

    std::shared_ptr<Session> m_session;

    std::vector<char>* m_buffer = nullptr;

    const bool m_compress;
    std::unique_ptr<entwine::Point> m_scale;
    std::unique_ptr<entwine::Point> m_offset;
    entwine::Schema m_schema;
    Json::Value m_filter;
    std::size_t m_numSent = 0;
    std::shared_ptr<ReadQuery> m_readQuery;

    uv_loop_t* m_loop;
    std::unique_ptr<uv_async_t, AsyncDeleter> m_initAsync;
    std::unique_ptr<uv_async_t, AsyncDeleter> m_dataAsync;
    v8::UniquePersistent<v8::Function> m_initCb;
    v8::UniquePersistent<v8::Function> m_dataCb;

    std::mutex m_mutex;
    std::condition_variable m_cv;
    bool m_wait;
    bool m_terminate;
};

class ReadCommandQuadIndex : public ReadCommand
{
public:
    ReadCommandQuadIndex(
            std::shared_ptr<Session> session,
            BufferPool& bufferPool,
            bool compress,
            const entwine::Point* scale,
            const entwine::Point* offset,
            std::string schemaString,
            std::string filterString,
            std::unique_ptr<entwine::Bounds> bounds,
            std::size_t depthBegin,
            std::size_t depthEnd,
            uv_loop_t* loop,
            v8::UniquePersistent<v8::Function> initCb,
            v8::UniquePersistent<v8::Function> dataCb);

protected:
    virtual void query();

    const std::unique_ptr<entwine::Bounds> m_bounds;
    const std::size_t m_depthBegin;
    const std::size_t m_depthEnd;
};

