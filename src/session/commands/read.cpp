#include <limits>

#include <node_buffer.h>

#include <pdal/PointLayout.hpp>

#include <entwine/types/dim-info.hpp>
#include <entwine/types/point.hpp>
#include <entwine/types/schema.hpp>
#include <entwine/util/json.hpp>
#include <entwine/util/unique.hpp>

#include "session.hpp"

#include "commands/read.hpp"

using namespace v8;

namespace
{
    std::size_t isEmpty(v8::Local<v8::Object> object)
    {
        return object->GetOwnPropertyNames()->Length() == 0;
    }

    BufferPool bufferPool(512);

    void release(char* pos, void* hint)
    {
        bufferPool.release(pos);
    }
}

ReadCommand::ReadCommand(
        std::shared_ptr<Session> session,
        BufferPool& bufferPool,
        const bool compress,
        const entwine::Point* scale,
        const entwine::Point* offset,
        const std::string schemaString,
        const std::string filterString,
        uv_loop_t* loop,
        v8::UniquePersistent<v8::Function> initCb,
        v8::UniquePersistent<v8::Function> dataCb)
    : m_session(session)
    , m_compress(compress)
    , m_scale(entwine::maybeClone(scale))
    , m_offset(entwine::maybeClone(offset))
    , m_schema(schemaString.empty() ?
            session->schema() : entwine::Schema(schemaString))
    , m_filter(filterString.empty() ?
            Json::Value() : entwine::parse(filterString))
    , m_loop(loop)
    , m_initAsync(new uv_async_t())
    , m_dataAsync(new uv_async_t())
    , m_initCb(std::move(initCb))
    , m_dataCb(std::move(dataCb))
    , m_wait(false)
    , m_terminate(false)
{
    if (schemaString.empty())
    {
        m_schema = session->schema();
    }
    else
    {
        Json::Reader reader;
        Json::Value jsonSchema;
        reader.parse(schemaString, jsonSchema);

        if (reader.getFormattedErrorMessages().size())
        {
            std::cout << reader.getFormattedErrorMessages() << std::endl;
            throw std::runtime_error("Could not parse requested schema");
        }

        m_schema = entwine::Schema(jsonSchema);
    }

    // This allows us to unwrap our own ReadCommand during async CBs.
    m_initAsync->data = this;
    m_dataAsync->data = this;

    registerInitCb();
    registerDataCb();
}

void ReadCommand::registerInitCb()
{
    uv_async_init(
        m_loop,
        initAsync(),
        ([](uv_async_t* async)->void
        {
            Isolate* isolate(Isolate::GetCurrent());
            HandleScope scope(isolate);
            ReadCommand* self(static_cast<ReadCommand*>(async->data));

            const unsigned argc = 1;
            Local<Value> argv[argc] =
            {
                self->status.ok() ?
                    Local<Value>::New(isolate, Null(isolate)) : // err
                    self->status.toObject(isolate)
            };

            Local<Function> local(Local<Function>::New(
                    isolate,
                    self->initCb()));

            local->Call(isolate->GetCurrentContext()->Global(), argc, argv);
            self->notifyCb();
        })
    );
}

void ReadCommand::registerDataCb()
{
    uv_async_init(
        m_loop,
        dataAsync(),
        ([](uv_async_t* async)->void
        {
            Isolate* isolate(Isolate::GetCurrent());
            HandleScope scope(isolate);
            ReadCommand* self(static_cast<ReadCommand*>(async->data));

            if (self->status.ok())
            {
                bufferPool.capture(self->buffer());

                MaybeLocal<Object> nodeBuffer(
                        node::Buffer::New(
                            isolate,
                            self->buffer().data(),
                            self->buffer().size(),
                            release,
                            nullptr));

                const unsigned argc = 3;
                Local<Value>argv[argc] =
                {
                    Local<Value>::New(isolate, Null(isolate)),
                    Local<Value>::New(isolate, nodeBuffer.ToLocalChecked()),
                    Local<Value>::New(
                            isolate,
                            Number::New(isolate, self->done()))
                };

                Local<Function> local(Local<Function>::New(
                        isolate,
                        self->dataCb()));

                Local<Value> keepGoingVal =
                    local->Call(
                            isolate->GetCurrentContext()->Global(),
                            argc,
                            argv);

                const bool keepGoing(keepGoingVal->BooleanValue());
                if (!keepGoing) self->terminate(true);
            }
            else
            {
                const unsigned argc = 1;
                Local<Value> argv[argc] = { self->status.toObject(isolate) };

                Local<Function> local(Local<Function>::New(
                        isolate,
                        self->dataCb()));

                local->Call(isolate->GetCurrentContext()->Global(), argc, argv);
            }

            self->notifyCb();
        })
    );
}

void ReadCommand::read()
{
    do
    {
        m_buffer = &bufferPool.acquire();
        m_readQuery->read(*m_buffer);
        doCb(dataAsync());
    }
    while (!done() && status.ok());
}

ReadCommandQuadIndex::ReadCommandQuadIndex(
        std::shared_ptr<Session> session,
        BufferPool& bufferPool,
        bool compress,
        const entwine::Point* scale,
        const entwine::Point* offset,
        const std::string schemaString,
        const std::string filterString,
        std::unique_ptr<entwine::Bounds> bounds,
        std::size_t depthBegin,
        std::size_t depthEnd,
        uv_loop_t* loop,
        v8::UniquePersistent<v8::Function> initCb,
        v8::UniquePersistent<v8::Function> dataCb)
    : ReadCommand(
            session,
            bufferPool,
            compress,
            scale,
            offset,
            schemaString,
            filterString,
            loop,
            std::move(initCb),
            std::move(dataCb))
    , m_bounds(std::move(bounds))
    , m_depthBegin(depthBegin)
    , m_depthEnd(depthEnd)
{ }

void ReadCommandQuadIndex::query()
{
    m_readQuery = m_session->query(
            m_schema,
            m_filter,
            m_compress,
            m_scale.get(),
            m_offset.get(),
            m_bounds.get(),
            m_depthBegin,
            m_depthEnd);
}

ReadCommand* ReadCommand::create(
        Isolate* isolate,
        std::shared_ptr<Session> session,
        BufferPool& bufferPool,
        const std::string schemaString,
        const std::string filterString,
        bool compress,
        const entwine::Point* scale,
        const entwine::Point* offset,
        v8::Local<v8::Object> query,
        uv_loop_t* loop,
        v8::UniquePersistent<v8::Function> initCb,
        v8::UniquePersistent<v8::Function> dataCb)
{
    ReadCommand* readCommand(nullptr);

    const auto depthSymbol(toSymbol(isolate, "depth"));
    const auto depthBeginSymbol(toSymbol(isolate, "depthBegin"));
    const auto depthEndSymbol(toSymbol(isolate, "depthEnd"));
    const auto boundsSymbol(toSymbol(isolate, "bounds"));
    const auto filterSymbol(toSymbol(isolate, "filter"));

    std::size_t depthBegin(
            query->HasOwnProperty(depthBeginSymbol) ?
                query->Get(depthBeginSymbol)->Uint32Value() : 0);

    std::size_t depthEnd(
            query->HasOwnProperty(depthEndSymbol) ?
                query->Get(depthEndSymbol)->Uint32Value() : 0);

    if (depthBegin || depthEnd)
    {
        query->Delete(depthBeginSymbol);
        query->Delete(depthEndSymbol);
    }
    else if (query->HasOwnProperty(depthSymbol))
    {
        depthBegin = query->Get(depthSymbol)->Uint32Value();
        depthEnd = depthBegin + 1;

        query->Delete(depthSymbol);
    }

    if (!depthEnd) depthEnd = std::numeric_limits<uint32_t>::max();

    std::unique_ptr<entwine::Bounds> bounds;

    if (query->HasOwnProperty(boundsSymbol))
    {
        bounds.reset(
                new entwine::Bounds(parseBounds(query->Get(boundsSymbol))));
    }

    query->Delete(boundsSymbol);

    if (query->HasOwnProperty(filterSymbol))
    {
        std::cout <<
            std::string(
                    *v8::String::Utf8Value(
                        query->Get(filterSymbol)->ToString())) <<
            std::endl;
    }

    if (isEmpty(query))
    {
        readCommand = new ReadCommandQuadIndex(
                session,
                bufferPool,
                compress,
                scale,
                offset,
                schemaString,
                filterString,
                std::move(bounds),
                depthBegin,
                depthEnd,
                loop,
                std::move(initCb),
                std::move(dataCb));
    }

    if (!readCommand)
    {
        std::cout << "Bad read command" << std::endl;
        Status status(400, std::string("Invalid read query parameters"));
        const unsigned argc = 1;
        Local<Value> argv[argc] = { status.toObject(isolate) };

        Local<Function> local(Local<Function>::New(isolate, initCb));
        local->Call(isolate->GetCurrentContext()->Global(), argc, argv);
    }

    return readCommand;
}

