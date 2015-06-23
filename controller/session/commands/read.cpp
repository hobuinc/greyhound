#include <node_buffer.h>

#include <pdal/PointLayout.hpp>

#include <entwine/types/dim-info.hpp>
#include <entwine/types/point.hpp>
#include <entwine/types/schema.hpp>

#include "session.hpp"
#include "read-queries/base.hpp"
#include "util/buffer-pool.hpp"
#include "util/schema.hpp"

#include "commands/read.hpp"

using namespace v8;

namespace
{
    bool isInteger(const v8::Local<v8::Value>& value)
    {
        return value->IsInt32() || value->IsUint32();
    }

    bool isDefined(const v8::Local<v8::Value>& value)
    {
        return !value->IsUndefined();
    }

    v8::Local<v8::String> toSymbol(const std::string& str)
    {
        return v8::String::NewSymbol(str.c_str());
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

            bounds.append(rawBounds[0].asDouble());
            bounds.append(rawBounds[1].asDouble());
            bounds.append(0);
            bounds.append(rawBounds[2].asDouble());
            bounds.append(rawBounds[3].asDouble());
            bounds.append(0);

            // TODO
            json["is3d"] = false;

            bbox = entwine::BBox(json);
        }
        catch (...)
        {
            std::cout << "Invalid BBox in query." << std::endl;
        }

        return bbox;
    }

    v8::Local<v8::Value> getRasterMeta(ReadCommand* readCommand)
    {
        v8::Local<v8::Value> result(Local<Value>::New(Undefined()));

        if (readCommand->rasterize())
        {
            ReadCommandRastered* readCommandRastered(
                    static_cast<ReadCommandRastered*>(readCommand));

            if (!readCommandRastered)
                throw std::runtime_error("Invalid ReadCommand state");

            const RasterMeta rasterMeta(
                    readCommandRastered->rasterMeta());

            Local<Object> rasterObj(Object::New());
            rasterObj->Set(
                    String::NewSymbol("xBegin"),
                    Number::New(rasterMeta.xBegin));
            rasterObj->Set(
                    String::NewSymbol("xStep"),
                    Number::New(rasterMeta.xStep));
            rasterObj->Set(
                    String::NewSymbol("xNum"),
                    Integer::New(rasterMeta.xNum()));
            rasterObj->Set(
                    String::NewSymbol("yBegin"),
                    Number::New(rasterMeta.yBegin));
            rasterObj->Set(
                    String::NewSymbol("yStep"),
                    Number::New(rasterMeta.yStep));
            rasterObj->Set(
                    String::NewSymbol("yNum"),
                    Integer::New(rasterMeta.yNum()));

            result = v8::Local<v8::Value>::New(rasterObj);
        }

        return result;
    }
}

ReadCommand::ReadCommand(
        std::shared_ptr<Session> session,
        ItcBufferPool& itcBufferPool,
        const std::string readId,
        const bool compress,
        entwine::DimList dims,
        v8::Persistent<v8::Function> initCb,
        v8::Persistent<v8::Function> dataCb)
    : m_session(session)
    , m_itcBufferPool(itcBufferPool)
    , m_itcBuffer()
    , m_readId(readId)
    , m_compress(compress)
    , m_schema(dims)
    , m_numSent(0)
    , m_initAsync(new uv_async_t())
    , m_dataAsync(new uv_async_t())
    , m_initCb(initCb)
    , m_dataCb(dataCb)
    , m_wait(false)
    , m_cancel(false)
{
    // This allows us to unwrap our own ReadCommand during async CBs.
    m_initAsync->data = this;
    m_dataAsync->data = this;
}

ReadCommand::~ReadCommand()
{
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

    m_initCb.Dispose();
    m_dataCb.Dispose();
}

void ReadCommand::registerInitCb()
{
    uv_async_init(
        uv_default_loop(),
        m_initAsync,
        ([](uv_async_t* async, int status)->void
        {
            HandleScope scope;
            ReadCommand* readCommand(static_cast<ReadCommand*>(async->data));

            if (readCommand->status.ok())
            {
                const std::string id(readCommand->readId());
                const unsigned argc = 5;
                Local<Value> argv[argc] =
                {
                    Local<Value>::New(Null()), // err
                    Local<Value>::New(String::New(id.data(), id.size())),
                    Local<Value>::New(Integer::New(readCommand->numPoints())),
                    Local<Value>::New(Integer::New(readCommand->numBytes())),
                    getRasterMeta(readCommand)
                };

                readCommand->initCb()->Call(
                    Context::GetCurrent()->Global(), argc, argv);
            }
            else
            {
                const unsigned argc = 1;
                Local<Value> argv[argc] = { readCommand->status.toObject() };

                readCommand->initCb()->Call(
                    Context::GetCurrent()->Global(), argc, argv);
            }

            readCommand->notifyCb();
            scope.Close(Undefined());
        })
    );
}

void ReadCommand::registerDataCb()
{
    uv_async_init(
        uv_default_loop(),
        m_dataAsync,
        ([](uv_async_t* async, int status)->void
        {
            HandleScope scope;
            ReadCommand* readCommand(static_cast<ReadCommand*>(async->data));

            if (readCommand->status.ok())
            {
                const unsigned argc = 3;
                Local<Value>argv[argc] =
                {
                    Local<Value>::New(Null()), // err
                    Local<Value>::New(node::Buffer::New(
                            readCommand->getBuffer()->data(),
                            readCommand->getBuffer()->size())->handle_),
                    Local<Value>::New(Number::New(readCommand->done()))
                };

                readCommand->dataCb()->Call(
                    Context::GetCurrent()->Global(), argc, argv);
            }
            else
            {
                const unsigned argc = 1;
                Local<Value> argv[argc] =
                    { readCommand->status.toObject() };

                readCommand->dataCb()->Call(
                    Context::GetCurrent()->Global(), argc, argv);
            }

            readCommand->notifyCb();

            scope.Close(Undefined());
        })
    );
}

std::size_t ReadCommand::numBytes() const
{
    return numPoints() * Util::stride(m_schema, rasterize());
}

void ReadCommand::cancel(bool cancel)
{
    m_cancel = cancel;
}

std::shared_ptr<ItcBuffer> ReadCommand::getBuffer()
{
    return m_itcBuffer;
}

ItcBufferPool& ReadCommand::getBufferPool()
{
    return m_itcBufferPool;
}

bool ReadCommand::done() const
{
    return m_readQuery->done();
}

void ReadCommand::run()
{
    query();
}

void ReadCommand::acquire()
{
    m_itcBuffer = m_itcBufferPool.acquire();
}

void ReadCommand::read(std::size_t maxNumBytes)
{
    m_readQuery->read(m_itcBuffer, maxNumBytes);
}

void ReadCommandRastered::read(std::size_t maxNumBytes)
{
    m_readQuery->read(m_itcBuffer, maxNumBytes, true);
}

std::size_t ReadCommand::numPoints() const
{
    return m_readQuery->numPoints();
}

std::string ReadCommand::readId() const
{
    return m_readId;
}

bool ReadCommand::cancel() const
{
    return m_cancel;
}

v8::Persistent<v8::Function> ReadCommand::initCb() const
{
    return m_initCb;
}

v8::Persistent<v8::Function> ReadCommand::dataCb() const
{
    return m_dataCb;
}

ReadCommandUnindexed::ReadCommandUnindexed(
        std::shared_ptr<Session> session,
        ItcBufferPool& itcBufferPool,
        std::string readId,
        bool compress,
        entwine::DimList dims,
        v8::Persistent<v8::Function> initCb,
        v8::Persistent<v8::Function> dataCb)
    : ReadCommand(
            session,
            itcBufferPool,
            readId,
            compress,
            dims,
            initCb,
            dataCb)
{ }

ReadCommandQuadIndex::ReadCommandQuadIndex(
        std::shared_ptr<Session> session,
        ItcBufferPool& itcBufferPool,
        std::string readId,
        bool compress,
        entwine::DimList dims,
        entwine::BBox bbox,
        std::size_t depthBegin,
        std::size_t depthEnd,
        v8::Persistent<v8::Function> initCb,
        v8::Persistent<v8::Function> dataCb)
    : ReadCommand(
            session,
            itcBufferPool,
            readId,
            compress,
            dims,
            initCb,
            dataCb)
    , m_bbox(bbox)
    , m_depthBegin(depthBegin)
    , m_depthEnd(depthEnd)
{ }

ReadCommandRastered::ReadCommandRastered(
        std::shared_ptr<Session> session,
        ItcBufferPool& itcBufferPool,
        const std::string readId,
        bool compress,
        entwine::DimList dims,
        entwine::BBox bbox,
        const std::size_t level,
        v8::Persistent<v8::Function> initCb,
        v8::Persistent<v8::Function> dataCb)
    : ReadCommand(
            session,
            itcBufferPool,
            readId,
            compress,
            dims,
            initCb,
            dataCb)
    , m_bbox(bbox)
    , m_level(level)
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

void ReadCommandRastered::query()
{
    m_readQuery = m_session->query(
            m_schema,
            m_compress,
            m_bbox,
            m_level,
            m_rasterMeta);
}

ReadCommand* ReadCommandFactory::create(
        std::shared_ptr<Session> session,
        ItcBufferPool& itcBufferPool,
        std::string readId,
        entwine::DimList dims,
        bool compress,
        v8::Local<v8::Object> query,
        v8::Persistent<v8::Function> initCb,
        v8::Persistent<v8::Function> dataCb)
{
    ReadCommand* readCommand(0);

    if (!dims.size())
    {
        dims = session->schema().dims();
    }

    const auto depthSymbol(toSymbol("depth"));
    const auto depthBeginSymbol(toSymbol("depthBegin"));
    const auto depthEndSymbol(toSymbol("depthEnd"));
    const auto rasterizeSymbol(toSymbol("rasterize"));
    const auto bboxSymbol(toSymbol("bbox"));

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
                    readId,
                    compress,
                    dims,
                    bbox,
                    depthBegin,
                    depthEnd,
                    initCb,
                    dataCb);
        }
    }
    else if (query->HasOwnProperty(rasterizeSymbol))
    {
        const std::size_t rasterize(query->Get(rasterizeSymbol)->Uint32Value());

        entwine::BBox bbox;

        if (query->HasOwnProperty(bboxSymbol))
        {
            bbox = parseBBox(query->Get(bboxSymbol));
            if (!bbox.exists()) return readCommand;
        }

        query->Delete(rasterizeSymbol);
        query->Delete(bboxSymbol);

        if (isEmpty(query))
        {
            readCommand = new ReadCommandRastered(
                    session,
                    itcBufferPool,
                    readId,
                    compress,
                    dims,
                    bbox,
                    rasterize,
                    initCb,
                    dataCb);
        }
    }
    else if (isEmpty(query))
    {
        readCommand = new ReadCommandUnindexed(
                session,
                itcBufferPool,
                readId,
                compress,
                dims,
                initCb,
                dataCb);
    }

    return readCommand;
}

