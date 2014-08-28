#pragma once

#include <memory>
#include <vector>

#include <v8.h>

#include <pdal/Dimension.hpp>
#include <pdal/PointContext.hpp>

class BufferTransmitter;
class PdalSession;

void errorCallback(
        v8::Persistent<v8::Function> callback,
        std::string errMsg);

struct DimInfo
{
    DimInfo(std::string name, std::string type, std::size_t size)
        : id(pdal::Dimension::id(name)), type(type), size(size)
    { }

    DimInfo(
            const pdal::Dimension::Id::Enum id,
            const pdal::Dimension::Type::Enum type)
        : id(id)
        , type(pdal::Dimension::toName(pdal::Dimension::base(type)))
        , size(pdal::Dimension::size(type))
    { }

    const pdal::Dimension::Id::Enum id;
    const std::string type;
    const std::size_t size;
};

struct Schema
{
    explicit Schema(std::vector<DimInfo> dims) : dims(dims) { }
    const std::vector<DimInfo> dims;

    std::size_t stride() const
    {
        std::size_t stride(0);

        for (const auto& dim : dims)
        {
            stride += dim.size;
        }

        return stride;
    }
};

class ReadCommand
{
public:
    virtual void run() = 0;

    void cancel(bool cancel) { m_cancel = cancel; }
    void errMsg(std::string errMsg) { m_errMsg = errMsg; }

    void transmit(std::size_t offset, std::size_t numBytes);

    std::string readId()    const { return m_readId; }
    std::size_t numPoints() const { return m_numPoints; }
    std::size_t numBytes()  const { return m_numPoints * m_schema.stride(); }
    std::string errMsg()    const { return m_errMsg;    }
    bool        cancel()    const { return m_cancel;    }
    v8::Persistent<v8::Function> callback() const { return m_callback; }

    ReadCommand(
            std::shared_ptr<PdalSession> pdalSession,
            std::map<std::string, ReadCommand*>& readCommands,
            std::string readId,
            std::string host,
            std::size_t port,
            Schema schema,
            v8::Persistent<v8::Function> callback);

    virtual ~ReadCommand()
    { }

    // PdalBindings::m_readCommands maintains a map of currently executing
    // READ commands.  These entries need to be removed once their execution
    // is complete.  However, the only objects that may be accessed from
    // the uv_work_queue background threading functions are those which are
    // wrapped within this ReadCommand* class.  So when this ReadCommand is
    // finished executing, we need to erase ourselves from this map.
    void eraseSelf()
    {
        m_readCommands.erase(m_readId);
    }

protected:
    const std::shared_ptr<PdalSession> m_pdalSession;
    std::map<std::string, ReadCommand*>& m_readCommands;
    const std::string m_readId;
    const std::string m_host;
    const std::size_t m_port;
    const Schema m_schema;
    std::size_t m_numPoints;

    v8::Persistent<v8::Function> m_callback;
    bool m_cancel;

    std::vector<unsigned char> m_data;
    std::shared_ptr<BufferTransmitter> m_bufferTransmitter;
    std::string m_errMsg;

private:
    Schema schemaOrDefault(Schema reqSchema);
};

class ReadCommandUnindexed : public ReadCommand
{
public:
    ReadCommandUnindexed(
            std::shared_ptr<PdalSession> pdalSession,
            std::map<std::string, ReadCommand*>& readCommands,
            std::string readId,
            std::string host,
            std::size_t port,
            Schema schema,
            std::size_t start,
            std::size_t count,
            v8::Persistent<v8::Function> callback)
        : ReadCommand(
                pdalSession,
                readCommands,
                readId,
                host,
                port,
                schema,
                callback)
        , m_start(start)
        , m_count(count)
    { }

    virtual void run();

private:
    const std::size_t m_start;
    const std::size_t m_count;
};

class ReadCommandPointRadius : public ReadCommand
{
public:
    ReadCommandPointRadius(
            std::shared_ptr<PdalSession> pdalSession,
            std::map<std::string, ReadCommand*>& readCommands,
            std::string readId,
            std::string host,
            std::size_t port,
            Schema schema,
            bool is3d,
            double radius,
            double x,
            double y,
            double z,
            v8::Persistent<v8::Function> callback)
        : ReadCommand(
                pdalSession,
                readCommands,
                readId,
                host,
                port,
                schema,
                callback)
        , m_is3d(is3d)
        , m_radius(radius)
        , m_x(x)
        , m_y(y)
        , m_z(z)
    { }

    virtual void run();

private:
    const bool m_is3d;
    const double m_radius;
    const double m_x;
    const double m_y;
    const double m_z;
};

class ReadCommandQuadIndex : public ReadCommand
{
public:
    ReadCommandQuadIndex(
            std::shared_ptr<PdalSession> pdalSession,
            std::map<std::string, ReadCommand*>& readCommands,
            std::string readId,
            std::string host,
            std::size_t port,
            Schema schema,
            double xMin,
            double yMin,
            double xMax,
            double yMax,
            std::size_t depthBegin,
            std::size_t depthEnd,
            v8::Persistent<v8::Function> callback)
        : ReadCommand(
                pdalSession,
                readCommands,
                readId,
                host,
                port,
                schema,
                callback)
        , m_xMin(xMin)
        , m_yMin(yMin)
        , m_xMax(xMax)
        , m_yMax(yMax)
        , m_depthBegin(depthBegin)
        , m_depthEnd(depthEnd)
        , m_isBBoxQuery(true)
    { }

    ReadCommandQuadIndex(
            std::shared_ptr<PdalSession> pdalSession,
            std::map<std::string, ReadCommand*>& readCommands,
            std::string readId,
            std::string host,
            std::size_t port,
            Schema schema,
            std::size_t depthBegin,
            std::size_t depthEnd,
            v8::Persistent<v8::Function> callback)
        : ReadCommand(
                pdalSession,
                readCommands,
                readId,
                host,
                port,
                schema,
                callback)
        , m_xMin()
        , m_yMin()
        , m_xMax()
        , m_yMax()
        , m_depthBegin(depthBegin)
        , m_depthEnd(depthEnd)
        , m_isBBoxQuery(false)
    { }

    virtual void run();

private:
    const double m_xMin;
    const double m_yMin;
    const double m_xMax;
    const double m_yMax;
    const std::size_t m_depthBegin;
    const std::size_t m_depthEnd;

    const bool m_isBBoxQuery;
};

class ReadCommandFactory
{
public:
    static ReadCommand* create(
            std::shared_ptr<PdalSession> pdalSession,
            std::map<std::string, ReadCommand*>& readCommands,
            std::string readId,
            const v8::Arguments& args);
};

