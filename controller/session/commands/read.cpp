#include <node_buffer.h>

#include <pdal/PointLayout.hpp>

#include <entwine/types/dim-info.hpp>
#include <entwine/types/point.hpp>
#include <entwine/types/schema.hpp>

#include "session.hpp"
#include "util/buffer-pool.hpp"

#include "commands/read.hpp"

using namespace v8;

namespace
{
    void freeCb(char* data, void* hint) { }

    v8::Local<v8::String> toSymbol(Isolate* isolate, const std::string& str)
    {
        return v8::String::NewFromUtf8(isolate, str.c_str());
    }

    std::size_t isEmpty(v8::Local<v8::Object> object)
    {
        return object->GetOwnPropertyNames()->Length() == 0;
    }

    entwine::BBox parseBBox(const v8::Local<v8::Value>& jsBBox)
    {
        entwine::BBox bbox;

        try
        {
            std::string bboxStr(std::string(
                        *v8::String::Utf8Value(jsBBox->ToString())));

            Json::Reader reader;
            Json::Value rawBounds;

            reader.parse(bboxStr, rawBounds, false);

            Json::Value json;
            Json::Value& bounds(json["bounds"]);

            if (rawBounds.size() == 4)
            {
                bounds.append(rawBounds[0].asDouble());
                bounds.append(rawBounds[1].asDouble());
                bounds.append(0);
                bounds.append(rawBounds[2].asDouble());
                bounds.append(rawBounds[3].asDouble());
                bounds.append(0);

                json["is3d"] = false;
            }
            else if (rawBounds.size() == 6)
            {
                bounds.append(rawBounds[0].asDouble());
                bounds.append(rawBounds[1].asDouble());
                bounds.append(rawBounds[2].asDouble());
                bounds.append(rawBounds[3].asDouble());
                bounds.append(rawBounds[4].asDouble());
                bounds.append(rawBounds[5].asDouble());

                json["is3d"] = true;
            }
            else
            {
                throw std::runtime_error("Invalid");
            }

            bbox = entwine::BBox(json);
        }
        catch (...)
        {
            std::cout << "Invalid BBox in query." << std::endl;
        }

        return bbox;
    }
}

ReadCommand::ReadCommand(
        std::shared_ptr<Session> session,
        ItcBufferPool& itcBufferPool,
        const bool compress,
        const std::string schemaString,
        v8::UniquePersistent<v8::Function> initCb,
        v8::UniquePersistent<v8::Function> dataCb)
    : m_session(session)
    , m_itcBufferPool(itcBufferPool)
    , m_itcBuffer()
    , m_compress(compress)
    , m_schema(schemaString.empty() ?
            session->schema() : entwine::Schema(schemaString))
    , m_numSent(0)
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
        reader.parse("{\"schema\":" + schemaString + "}", jsonSchema);

        if (reader.getFormattedErrorMessages().size())
        {
            std::cout << reader.getFormattedErrorMessages() << std::endl;
            throw std::runtime_error("Could not parse requested schema");
        }

        m_schema = entwine::Schema(jsonSchema["schema"]);
    }

    // This allows us to unwrap our own ReadCommand during async CBs.
    m_initAsync->data = this;
    m_dataAsync->data = this;
}

ReadCommand::~ReadCommand()
{
    getBufferPool().release(getBuffer());

    uv_handle_t* initAsync(reinterpret_cast<uv_handle_t*>(m_initAsync));
    uv_handle_t* dataAsync(reinterpret_cast<uv_handle_t*>(m_dataAsync));

    uv_close_cb closeCallback(
        (uv_close_cb)([](uv_handle_t* async)->void
        {
            delete async;
        })
    );

    uv_close(initAsync, closeCallback);
    uv_close(dataAsync, closeCallback);

    m_initCb.Reset();
    m_dataCb.Reset();
}

void ReadCommand::registerInitCb()
{
    uv_async_init(
        uv_default_loop(),
        m_initAsync,
        ([](uv_async_t* async)->void
        {
            Isolate* isolate(Isolate::GetCurrent());
            HandleScope scope(isolate);
            ReadCommand* readCommand(static_cast<ReadCommand*>(async->data));

            const unsigned argc = 1;
            Local<Value> argv[argc] =
            {
                readCommand->status.ok() ?
                    Local<Value>::New(isolate, Null(isolate)) : // err
                    readCommand->status.toObject(isolate)
            };

            Local<Function> local(Local<Function>::New(
                    isolate,
                    readCommand->initCb()));

            local->Call(isolate->GetCurrentContext()->Global(), argc, argv);
            readCommand->notifyCb();
        })
    );
}

void ReadCommand::registerDataCb()
{
    uv_async_init(
        uv_default_loop(),
        m_dataAsync,
        ([](uv_async_t* async)->void
        {
            Isolate* isolate(Isolate::GetCurrent());
            HandleScope scope(isolate);
            ReadCommand* readCommand(static_cast<ReadCommand*>(async->data));

            if (readCommand->status.ok())
            {
                /*

                Ideally we'd pass kgFunction as arg 4 of this callback,
                allowing JS-land to asyncly call us as long as their keepGoing
                logic is truthy.  For now we'll make JS return a value from
                their onData function, since we can't capture state with this
                method.

                auto keepGoingCb([](const FunctionCallbackInfo<Value>& args)
                {
                    std::cout << "Got keep going signal!" << std::endl;
                });

                Local<FunctionTemplate> kgTemplate(
                        FunctionTemplate::New(isolate, keepGoingCb));
                Local<Function> kgFunction(kgTemplate->GetFunction());
                */

                MaybeLocal<Object> buffer(
                        node::Buffer::Copy(
                            isolate,
                            readCommand->getBuffer()->data(),
                            readCommand->getBuffer()->size()));

                const unsigned argc = 3;
                Local<Value>argv[argc] =
                {
                    Local<Value>::New(isolate, Null(isolate)),
                    Local<Value>::New(isolate, buffer.ToLocalChecked()),
                    Local<Value>::New(
                            isolate,
                            Number::New(isolate, readCommand->done()))
                };

                Local<Function> local(Local<Function>::New(
                        isolate,
                        readCommand->dataCb()));

                Local<Value> keepGoing =
                    local->Call(
                            isolate->GetCurrentContext()->Global(),
                            argc,
                            argv);

                readCommand->terminate(!keepGoing->BooleanValue());
            }
            else
            {
                const unsigned argc = 1;
                Local<Value> argv[argc] =
                    { readCommand->status.toObject(isolate) };

                Local<Function> local(Local<Function>::New(
                        isolate,
                        readCommand->dataCb()));

                local->Call(isolate->GetCurrentContext()->Global(), argc, argv);
            }

            readCommand->notifyCb();
        })
    );
}

ReadCommandUnindexed::ReadCommandUnindexed(
        std::shared_ptr<Session> session,
        ItcBufferPool& itcBufferPool,
        bool compress,
        const std::string schemaString,
        v8::UniquePersistent<v8::Function> initCb,
        v8::UniquePersistent<v8::Function> dataCb)
    : ReadCommand(
            session,
            itcBufferPool,
            compress,
            schemaString,
            std::move(initCb),
            std::move(dataCb))
{ }

ReadCommandQuadIndex::ReadCommandQuadIndex(
        std::shared_ptr<Session> session,
        ItcBufferPool& itcBufferPool,
        bool compress,
        const std::string schemaString,
        entwine::BBox bbox,
        std::size_t depthBegin,
        std::size_t depthEnd,
        v8::UniquePersistent<v8::Function> initCb,
        v8::UniquePersistent<v8::Function> dataCb)
    : ReadCommand(
            session,
            itcBufferPool,
            compress,
            schemaString,
            std::move(initCb),
            std::move(dataCb))
    , m_bbox(bbox)
    , m_depthBegin(depthBegin)
    , m_depthEnd(depthEnd)
{ }

void ReadCommandUnindexed::query()
{
    m_readQuery = m_session->query(m_schema, m_compress);
}

void ReadCommandQuadIndex::query()
{
    m_readQuery = m_session->query(
            m_schema,
            m_compress,
            m_bbox,
            m_depthBegin,
            m_depthEnd);
}

ReadCommand* ReadCommandFactory::create(
        Isolate* isolate,
        std::shared_ptr<Session> session,
        ItcBufferPool& itcBufferPool,
        const std::string schemaString,
        bool compress,
        v8::Local<v8::Object> query,
        v8::UniquePersistent<v8::Function> initCb,
        v8::UniquePersistent<v8::Function> dataCb)
{
    ReadCommand* readCommand(0);

    const auto depthSymbol(toSymbol(isolate, "depth"));
    const auto depthBeginSymbol(toSymbol(isolate, "depthBegin"));
    const auto depthEndSymbol(toSymbol(isolate, "depthEnd"));
    const auto bboxSymbol(toSymbol(isolate, "bounds"));

    if (
            query->HasOwnProperty(depthSymbol) ||
            query->HasOwnProperty(depthBeginSymbol) ||
            query->HasOwnProperty(depthEndSymbol))
    {
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

        entwine::BBox bbox;

        if (query->HasOwnProperty(bboxSymbol))
        {
            bbox = parseBBox(query->Get(bboxSymbol));
            if (!bbox.exists()) return readCommand;
        }

        query->Delete(bboxSymbol);

        if (isEmpty(query))
        {
            readCommand = new ReadCommandQuadIndex(
                    session,
                    itcBufferPool,
                    compress,
                    schemaString,
                    bbox,
                    depthBegin,
                    depthEnd,
                    std::move(initCb),
                    std::move(dataCb));
        }
    }
    else if (isEmpty(query))
    {
        readCommand = new ReadCommandUnindexed(
                session,
                itcBufferPool,
                compress,
                schemaString,
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

