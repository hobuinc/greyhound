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
            Json::Value jsonBBox;
            reader.parse(bboxStr, jsonBBox, false);
            bbox = entwine::BBox::fromJson(jsonBBox);
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
        const std::string readId,
        const bool compress,
        entwine::DimList dims,
        v8::Persistent<v8::Function> readCb,
        v8::Persistent<v8::Function> dataCb)
    : m_session(session)
    , m_itcBufferPool(itcBufferPool)
    , m_itcBuffer()
    , m_async(new uv_async_t())
    , m_readId(readId)
    , m_compress(compress)
    , m_schema(dims)
    , m_numSent(0)
    , m_readCb(readCb)
    , m_dataCb(dataCb)
    , m_cancel(false)
{ }

ReadCommand::~ReadCommand()
{
    m_readCb.Dispose();
    m_dataCb.Dispose();
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

v8::Persistent<v8::Function> ReadCommand::readCb() const
{
    return m_readCb;
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
        v8::Persistent<v8::Function> readCb,
        v8::Persistent<v8::Function> dataCb)
    : ReadCommand(
            session,
            itcBufferPool,
            readId,
            compress,
            dims,
            readCb,
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
        v8::Persistent<v8::Function> readCb,
        v8::Persistent<v8::Function> dataCb)
    : ReadCommand(
            session,
            itcBufferPool,
            readId,
            compress,
            dims,
            readCb,
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
        v8::Persistent<v8::Function> readCb,
        v8::Persistent<v8::Function> dataCb)
    : ReadCommand(
            session,
            itcBufferPool,
            readId,
            compress,
            dims,
            readCb,
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
        v8::Persistent<v8::Function> readCb,
        v8::Persistent<v8::Function> dataCb)
{
    ReadCommand* readCommand(0);

    if (!dims.size())
    {
        dims = session->schema().dims();
    }

    const auto depthBeginSymbol(toSymbol("depthBegin"));
    const auto depthEndSymbol(toSymbol("depthEnd"));
    const auto rasterizeSymbol(toSymbol("rasterize"));
    const auto bboxSymbol(toSymbol("bbox"));

    if (
            query->HasOwnProperty(depthBeginSymbol) ||
            query->HasOwnProperty(depthEndSymbol))
    {
        const std::size_t depthBegin(
                query->HasOwnProperty(depthBeginSymbol) ?
                    query->Get(depthBeginSymbol)->Uint32Value() : 0);

        const std::size_t depthEnd(
                query->HasOwnProperty(depthEndSymbol) ?
                    query->Get(depthEndSymbol)->Uint32Value() : 0);

        entwine::BBox bbox;

        if (query->HasOwnProperty(bboxSymbol))
        {
            bbox = parseBBox(query->Get(bboxSymbol));
            if (!bbox.exists()) return readCommand;
        }

        query->Delete(depthBeginSymbol);
        query->Delete(depthEndSymbol);
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
                    readCb,
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
                    readCb,
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
                readCb,
                dataCb);
    }

    return readCommand;
}

