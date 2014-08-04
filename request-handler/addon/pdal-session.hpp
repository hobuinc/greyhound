#pragma once

#include <memory>

#include <boost/asio.hpp>

#include <pdal/PipelineManager.hpp>
#include <pdal/Schema.hpp>

class PdalInstance
{
public:
    PdalInstance();

    void parse(const std::string& pipeline);
    void initialize(const std::string& pipeline);

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

    void transmit();

private:
    std::unique_ptr<boost::asio::ip::tcp::socket> m_socket;
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
    static v8::Handle<v8::Value> parse(const v8::Arguments& args);
    static v8::Handle<v8::Value> create(const v8::Arguments& args);
    static v8::Handle<v8::Value> destroy(const v8::Arguments& args);
    static v8::Handle<v8::Value> getNumPoints(const v8::Arguments& args);
    static v8::Handle<v8::Value> getSchema(const v8::Arguments& args);
    static v8::Handle<v8::Value> read(const v8::Arguments& args);

    // Helper function to perform an errback.
    static void errorCallback(
            v8::Persistent<v8::Function> callback,
            std::string errMsg);

    std::shared_ptr<PdalInstance> m_pdalInstance;

    struct CreateData
    {
        CreateData(
                std::shared_ptr<PdalInstance> pdalInstance,
                std::string pipeline,
                v8::Persistent<v8::Function> callback)
            : pdalInstance(pdalInstance)
            , pipeline(pipeline)
            , errMsg()
            , callback(callback)
        { }

        // Inputs
        const std::shared_ptr<PdalInstance> pdalInstance;
        const std::string pipeline;

        // Outputs
        std::string errMsg;

        v8::Persistent<v8::Function> callback;
    };

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
            , bufferTransmitter()
            , numPoints(0)
            , errMsg()
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
        std::shared_ptr<BufferTransmitter> bufferTransmitter;
        std::size_t numPoints;
        std::size_t numBytes;
        std::string errMsg;

        v8::Persistent<v8::Function> callback;
    };
};

