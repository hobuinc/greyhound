#include <thread>

#include <node.h>
#include <v8.h>

#include <pdal/PipelineReader.hpp>

#include <boost/asio.hpp>

#include "pdal-session.hpp"

using namespace v8;

PdalInstance::PdalInstance(const std::string& pipeline)
    : m_pipelineManager()
    , m_schema()
    , m_pointBuffer()
{
    std::istringstream ssPipeline(pipeline);
    pdal::PipelineReader pipelineReader(m_pipelineManager);
    pipelineReader.readPipeline(ssPipeline);

    m_pipelineManager.execute();
    const pdal::PointBufferSet& pbSet(m_pipelineManager.buffers());
    m_pointBuffer = pbSet.begin()->get();

    try
    {
        m_schema = packSchema(*m_pipelineManager.schema());

        m_schema.getDimension("X");
        m_schema.getDimension("Y");
        m_schema.getDimension("Z");
    }
    catch (pdal::dimension_not_found&)
    {
        throw std::runtime_error(
                "Pipeline output should contain X, Y and Z dimensions");
    }
}

std::size_t PdalInstance::getNumPoints() const
{
    return m_pointBuffer->getNumPoints();
}

std::string PdalInstance::getSchema() const
{
    return pdal::Schema::to_xml(m_schema);
}

std::size_t PdalInstance::getStride() const
{
    return m_schema.getByteSize();
}

std::size_t PdalInstance::read(
        unsigned char** buffer,
        const std::size_t start,
        const std::size_t count)
{
    *buffer = 0;
    if (start >= getNumPoints())
        throw std::runtime_error("Invalid starting offset in 'read'");

    // If zero points specified, read all points after 'start'.
    const std::size_t pointsToRead(
            count > 0 ?
                std::min<std::size_t>(count, getNumPoints() - start) :
                getNumPoints() - start);

    const pdal::Schema* fullSchema(m_pipelineManager.schema());
    const pdal::schema::index_by_index& idx(
            fullSchema->getDimensions().get<pdal::schema::index>());

    *buffer = new unsigned char[m_schema.getByteSize() * pointsToRead];

    boost::uint8_t* pos(static_cast<boost::uint8_t*>(*buffer));

    for (boost::uint32_t i(start); i < start + pointsToRead; ++i)
    {
        for (boost::uint32_t d = 0; d < idx.size(); ++d)
        {
            if (!idx[d].isIgnored())
            {
                m_pointBuffer->context().rawPtBuf()->getField(
                        idx[d],
                        i,
                        pos);

                pos += idx[d].getByteSize();
            }
        }
    }

    return pointsToRead;
}

pdal::Schema PdalInstance::packSchema(const pdal::Schema& fullSchema)
{
    pdal::Schema packedSchema;

    const pdal::schema::index_by_index& idx(
            fullSchema.getDimensions().get<pdal::schema::index>());

    for (boost::uint32_t d = 0; d < idx.size(); ++d)
    {
        if (!idx[d].isIgnored())
        {
            packedSchema.appendDimension(idx[d]);
        }
    }

    return packedSchema;
}

//////////////////////////////////////////////////////////////////////////////

BufferTransmitter::BufferTransmitter(
        const std::string& host,
        const int port,
        const unsigned char* data,
        const std::size_t size)
    : m_host(host)
    , m_port(port)
    , m_data(data)
    , m_size(size)
{ }

void BufferTransmitter::operator()()
{
    namespace asio = boost::asio;
    using boost::asio::ip::tcp;

    std::stringstream portStream;
    portStream << m_port;

    asio::io_service service;
    tcp::resolver resolver(service);

    tcp::resolver::query q(m_host, portStream.str());
    tcp::resolver::iterator iter = resolver.resolve(q), end;

    tcp::socket socket(service);

    int retryCount = 0;
    boost::system::error_code ignored_error;

    // Don't fail yet, the setup service may be setting up the receiver.
    tcp::resolver::iterator connectIter;

    while(
        (connectIter = asio::connect(socket, iter, ignored_error)) == end &&
            retryCount++ < 500)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    if(connectIter == end)
    {
        // TODO: We need to propagate the error information to the user
        return;
    }

    // Send our data.
    asio::write(
            socket,
            asio::buffer(m_data, m_size),
            ignored_error);
}

//////////////////////////////////////////////////////////////////////////////

Persistent<Function> PdalSession::constructor;

PdalSession::PdalSession()
    : m_pdalInstance()
{ }

PdalSession::~PdalSession()
{ }

void PdalSession::init(v8::Handle<v8::Object> exports)
{
    // Prepare constructor template
    Local<FunctionTemplate> tpl = FunctionTemplate::New(construct);
    tpl->SetClassName(String::NewSymbol("PdalSession"));
    tpl->InstanceTemplate()->SetInternalFieldCount(1);

    // Prototype
    tpl->PrototypeTemplate()->Set(String::NewSymbol("construct"),
        FunctionTemplate::New(create)->GetFunction());
    tpl->PrototypeTemplate()->Set(String::NewSymbol("create"),
        FunctionTemplate::New(create)->GetFunction());
    tpl->PrototypeTemplate()->Set(String::NewSymbol("destroy"),
        FunctionTemplate::New(destroy)->GetFunction());
    tpl->PrototypeTemplate()->Set(String::NewSymbol("getNumPoints"),
        FunctionTemplate::New(getNumPoints)->GetFunction());
    tpl->PrototypeTemplate()->Set(String::NewSymbol("getSchema"),
        FunctionTemplate::New(getSchema)->GetFunction());
    tpl->PrototypeTemplate()->Set(String::NewSymbol("read"),
        FunctionTemplate::New(read)->GetFunction());

    constructor = Persistent<Function>::New(tpl->GetFunction());
    exports->Set(String::NewSymbol("PdalSession"), constructor);
}

Handle<Value> PdalSession::construct(const Arguments& args)
{
    HandleScope scope;

    if (args.IsConstructCall())
    {
        // Invoked as constructor with 'new'.
        PdalSession* obj = new PdalSession();
        obj->Wrap(args.This());
        return args.This();
    }
    else
    {
        // Invoked as a function, turn into construct call.
        return scope.Close(constructor->NewInstance());
    }
}

Handle<Value> PdalSession::create(const Arguments& args)
{
    HandleScope scope;

    const std::string pipeline(*v8::String::Utf8Value(args[0]->ToString()));
    Local<Function> cb = Local<Function>::Cast(args[1]);



    const unsigned argc = 1;
    Local<Value> argv[argc] = { Local<Value>::New(Null()) };
    Local<Value>& err = argv[0];

    try
    {
        PdalSession* obj = ObjectWrap::Unwrap<PdalSession>(args.This());
        obj->m_pdalInstance.reset(new PdalInstance(pipeline));
    }
    catch (const std::runtime_error& e)
    {
        err = Local<Value>::New(String::New(e.what()));
    }
    catch (...)
    {
        err = Local<Value>::New(String::New("Unknown error"));
    }

    cb->Call(Context::GetCurrent()->Global(), argc, argv);

    return scope.Close(Undefined());
}

Handle<Value> PdalSession::destroy(const Arguments& args) {
    HandleScope scope;

    PdalSession* obj = ObjectWrap::Unwrap<PdalSession>(args.This());
    obj->m_pdalInstance.reset();

    return scope.Close(Undefined());
}

Handle<Value> PdalSession::getNumPoints(const Arguments& args)
{
    HandleScope scope;

    PdalSession* obj = ObjectWrap::Unwrap<PdalSession>(args.This());

    return scope.Close(Integer::New(obj->m_pdalInstance->getNumPoints()));
}

Handle<Value> PdalSession::getSchema(const Arguments& args)
{
    HandleScope scope;

    PdalSession* obj = ObjectWrap::Unwrap<PdalSession>(args.This());
    const std::string schema(obj->m_pdalInstance->getSchema());

    return scope.Close(String::New(schema.data(), schema.size()));
}

Handle<Value> PdalSession::read(const Arguments& args)
{
    HandleScope scope;

    // Input args.
    const std::string host(*v8::String::Utf8Value(args[0]->ToString()));
    const std::size_t port(args[1]->IsUndefined() ? 0 : args[1]->Uint32Value());
    const std::size_t start(args[2]->IsUndefined() ? 0 : args[2]->Uint32Value());
    const std::size_t count(args[3]->IsUndefined() ? 0 : args[3]->Uint32Value());
    Local<Function> cb = Local<Function>::Cast(args[4]);

    PdalSession* obj = ObjectWrap::Unwrap<PdalSession>(args.This());

    // Output args.
    const unsigned argc = 3;
    Local<Value> argv[argc] =
        {
            Local<Value>::New(Null()),          // err
            Local<Value>::New(Integer::New(0)), // points to send
            Local<Value>::New(Integer::New(0))  //numBytes
        };

    Local<Value>& err(argv[0]);
    Local<Value>& pointsToSend(argv[1]);
    Local<Value>& bytesToSend(argv[2]);

    if (start >= obj->m_pdalInstance->getNumPoints())
    {
        const std::string msg("Invalid start offset in 'read' request");
        err = Local<Value>::New(String::New(msg.data(), msg.size()));
    }
    else
    {
        // Read the points to our buffer.
        // TODO Should probably be asynchronous.
        unsigned char* data(0);
        const std::size_t numPoints(
                obj->m_pdalInstance->read(&data, start, count));
        const std::size_t numBytes(
                numPoints * obj->m_pdalInstance->getStride());

        // Set return variables.
        pointsToSend = Local<Value>::New(Integer::New(numPoints));
        bytesToSend  = Local<Value>::New(Integer::New(numBytes));

        // Start the data stream.
        std::thread t(BufferTransmitter(host, port, data, numBytes));
        t.detach();
    }

    cb->Call(Context::GetCurrent()->Global(), argc, argv);

    return scope.Close(Undefined());
}

//////////////////////////////////////////////////////////////////////////////

void init(Handle<Object> exports)
{
    PdalSession::init(exports);
}

NODE_MODULE(pdalSession, init)

