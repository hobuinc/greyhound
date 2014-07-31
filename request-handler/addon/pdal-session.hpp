#pragma once

#include <pdal/PipelineManager.hpp>
#include <pdal/Schema.hpp>

class PdalInstance
{
public:
    PdalInstance(const std::string& pipeline);

    std::size_t getNumPoints() const;
    std::string getSchema() const;
    std::size_t getStride() const;

    std::size_t read(
            unsigned char** buffer,
            std::size_t start,
            std::size_t count);

private:
    pdal::PipelineManager m_pipelineManager;
    pdal::Schema m_schema;
    const pdal::PointBuffer* m_pointBuffer;

    pdal::Schema packSchema(const pdal::Schema& fullSchema);
};

class BufferTransmitter
{
public:
    BufferTransmitter(
            const std::string& host,
            int port,
            const unsigned char* data,
            std::size_t size);

    void operator()();

private:
    const std::string m_host;
    const int m_port;
    const unsigned char* const m_data;
    const std::size_t m_size;
};

class PdalSession : public node::ObjectWrap
{
public:
    static void init(v8::Handle<v8::Object> exports);

private:
    PdalSession();
    ~PdalSession();

    static v8::Persistent<v8::Function> constructor;

    static v8::Handle<v8::Value> construct(const v8::Arguments& args);
    static v8::Handle<v8::Value> create(const v8::Arguments& args);
    static v8::Handle<v8::Value> destroy(const v8::Arguments& args);
    static v8::Handle<v8::Value> getNumPoints(const v8::Arguments& args);
    static v8::Handle<v8::Value> getSchema(const v8::Arguments& args);
    static v8::Handle<v8::Value> read(const v8::Arguments& args);

    std::shared_ptr<PdalInstance> m_pdalInstance;

    struct ReadData
    {
        ReadData(
                std::shared_ptr<PdalInstance> pdalInstance,
                std::string host,
                std::size_t port,
                std::size_t start,
                std::size_t count,
                v8::Persistent<v8::Function> callback)
            : pdalInstance(pdalInstance)
            , host(host)
            , port(port)
            , start(start)
            , count(count)
            , data(0)
            , numPoints(0)
            , callback(callback)
        { }

        // Inputs
        const std::shared_ptr<PdalInstance> pdalInstance;
        const std::string host;
        const std::size_t port;
        const std::size_t start;
        const std::size_t count;

        // Outputs
        unsigned char* data;
        std::size_t numPoints;
        std::size_t numBytes;

        v8::Persistent<v8::Function> callback;
    };
};

