#include <thread>
#include <sstream>

#include <execinfo.h>
#include <unistd.h>

#include <curl/curl.h>

#include <pdal/PointLayout.hpp>
#include <pdal/StageFactory.hpp>

#include <entwine/reader/cache.hpp>
#include <entwine/reader/reader.hpp>
#include <entwine/third/arbiter/arbiter.hpp>
#include <entwine/types/bbox.hpp>
#include <entwine/types/dim-info.hpp>
#include <entwine/types/point.hpp>
#include <entwine/types/schema.hpp>

#include "session.hpp"
#include "commands/create.hpp"
#include "util/buffer-pool.hpp"
#include "util/once.hpp"

#include "bindings.hpp"

using namespace v8;

namespace
{
    void handler(int sig)
    {
        void* array[16];
        const std::size_t size(backtrace(array, 16));

        std::cout << "Got error " << sig << std::endl;
        backtrace_symbols_fd(array, size, STDERR_FILENO);
        exit(1);
    }

    const std::size_t numBuffers = 1024;
    ItcBufferPool itcBufferPool(numBuffers);

    std::mutex factoryMutex;
    std::unique_ptr<pdal::StageFactory> stageFactory(new pdal::StageFactory());

    std::shared_ptr<arbiter::Arbiter> commonArbiter(0);
    std::shared_ptr<entwine::Cache> cache(0);

    const std::size_t readIdSize = 24;
    const std::string hexValues = "0123456789ABCDEF";

    std::string generateReadId()
    {
        std::string id(readIdSize, '0');

        for (std::size_t i(0); i < id.size(); ++i)
        {
            id[i] = hexValues[rand() % hexValues.size()];
        }

        return id;
    }

    std::vector<std::string> parsePathList(
            Isolate* isolate,
            const v8::Local<v8::Value>& rawArg)
    {
        std::vector<std::string> paths;

        if (!rawArg->IsUndefined() && rawArg->IsArray())
        {
            Array* rawArray(Array::Cast(*rawArg));

            for (std::size_t i(0); i < rawArray->Length(); ++i)
            {
                const v8::Local<v8::Value>& rawValue(
                    rawArray->Get(Integer::New(isolate, i)));

                if (rawValue->IsString())
                {
                    paths.push_back(std::string(
                            *v8::String::Utf8Value(rawValue->ToString())));
                }
            }
        }

        return paths;
    }

    std::mutex initMutex;

    void initConfigurable(std::size_t maxCacheSize)
    {
        std::lock_guard<std::mutex> lock(initMutex);

        if (!cache)
        {
            signal(SIGSEGV, handler);

            cache.reset(new entwine::Cache(maxCacheSize));
        }

        if (!commonArbiter)
        {
            commonArbiter.reset(new arbiter::Arbiter());
        }
    }
}

namespace ghEnv
{
    Once curlOnce([]()->void {
        std::cout << "Destructing global environment" << std::endl;
        curl_global_cleanup();
    });

    Once cacheOnce;
}

Persistent<Function> Bindings::constructor;

Bindings::Bindings()
    : m_session(new Session(*stageFactory, factoryMutex))
    , m_itcBufferPool(itcBufferPool)
{
    ghEnv::curlOnce.ensure([]()->void {
        std::cout << "Initializing global environment" << std::endl;
        curl_global_init(CURL_GLOBAL_ALL);
    });
}

Bindings::~Bindings()
{ }

void Bindings::init(v8::Handle<v8::Object> exports)
{
    Isolate* isolate(Isolate::GetCurrent());

    // Prepare constructor template
    Local<FunctionTemplate> tpl(v8::FunctionTemplate::New(isolate, construct));
    tpl->SetClassName(String::NewFromUtf8(isolate, "Bindings"));
    tpl->InstanceTemplate()->SetInternalFieldCount(1);

    NODE_SET_PROTOTYPE_METHOD(tpl, "construct", construct);
    NODE_SET_PROTOTYPE_METHOD(tpl, "create",    create);
    NODE_SET_PROTOTYPE_METHOD(tpl, "destroy",   destroy);
    NODE_SET_PROTOTYPE_METHOD(tpl, "info",      info);
    NODE_SET_PROTOTYPE_METHOD(tpl, "read",      read);

    constructor.Reset(isolate, tpl->GetFunction());
    exports->Set(String::NewFromUtf8(isolate, "Bindings"), tpl->GetFunction());
}

void Bindings::construct(const FunctionCallbackInfo<Value>& args)
{
    Isolate* isolate(args.GetIsolate());
    HandleScope scope(isolate);

    if (args.IsConstructCall())
    {
        // Invoked as constructor with 'new'.
        Bindings* obj = new Bindings();
        obj->Wrap(args.Holder());
        args.GetReturnValue().Set(args.Holder());
    }
    else
    {
        // Invoked as a function, turn into construct call.
        Local<Function> ctor(Local<Function>::New(isolate, constructor));
        args.GetReturnValue().Set(ctor->NewInstance());
    }
}

void Bindings::create(const FunctionCallbackInfo<Value>& args)
{
    Isolate* isolate(args.GetIsolate());
    HandleScope scope(isolate);

    Bindings* obj = ObjectWrap::Unwrap<Bindings>(args.Holder());

    if (args.Length() != 4)
    {
        throw std::runtime_error("Wrong number of arguments to create");
    }

    std::size_t i(0);
    const auto& nameArg     (args[i++]);
    const auto& pathsArg    (args[i++]);
    const auto& cacheArg    (args[i++]);
    const auto& cbArg       (args[i++]);

    std::string errMsg("");

    if (!nameArg->IsString()) errMsg += "\t'name' must be a string";
    if (!pathsArg->IsArray()) errMsg += "\t'paths' must be an array";
    if (!cbArg->IsFunction()) throw std::runtime_error("Invalid create CB");

    UniquePersistent<Function> callback(isolate, Local<Function>::Cast(cbArg));

    if (errMsg.size())
    {
        std::cout << "Client error: " << errMsg << std::endl;
        Status status(400, errMsg);
        const unsigned argc = 1;
        Local<Value> argv[argc] = { status.toObject(isolate) };

        Local<Function> local(Local<Function>::New(isolate, callback));

        local->Call(isolate->GetCurrentContext()->Global(), argc, argv);
        callback.Reset();
        return;
    }

    const std::string name(*v8::String::Utf8Value(nameArg->ToString()));
    const std::vector<std::string> paths(parsePathList(isolate, pathsArg));
    const std::size_t maxCacheSize(cacheArg->IntegerValue());

    initConfigurable(maxCacheSize);

    // Store everything we'll need to perform initialization.
    uv_work_t* req(new uv_work_t);
    req->data = new CreateData(
            obj->m_session,
            name,
            paths,
            commonArbiter,
            cache,
            std::move(callback));

    uv_queue_work(
        uv_default_loop(),
        req,
        (uv_work_cb)([](uv_work_t *req)->void
        {
            CreateData* createData(static_cast<CreateData*>(req->data));

            createData->safe([createData]()->void
            {
                if (!createData->session->initialize(
                        createData->name,
                        createData->paths,
                        createData->arbiter,
                        createData->cache))
                {
                    createData->status.set(404, "Not found");
                }
            });
        }),
        (uv_after_work_cb)([](uv_work_t* req, int status)->void
        {
            Isolate* isolate(Isolate::GetCurrent());
            HandleScope scope(isolate);

            CreateData* createData(static_cast<CreateData*>(req->data));

            const unsigned argc = 1;
            Local<Value> argv[argc] = { createData->status.toObject(isolate) };

            Local<Function> local(Local<Function>::New(
                    isolate,
                    createData->callback));

            local->Call(isolate->GetCurrentContext()->Global(), argc, argv);

            delete createData;
            delete req;
        })
    );
}

void Bindings::destroy(const FunctionCallbackInfo<Value>& args)
{
    Isolate* isolate(args.GetIsolate());
    HandleScope scope(isolate);
    Bindings* obj = ObjectWrap::Unwrap<Bindings>(args.Holder());

    obj->m_session.reset();
}

void Bindings::info(const FunctionCallbackInfo<Value>& args)
{
    Isolate* isolate(args.GetIsolate());
    HandleScope scope(isolate);
    Bindings* obj = ObjectWrap::Unwrap<Bindings>(args.Holder());

    const std::string info(obj->m_session->info());
    args.GetReturnValue().Set(String::NewFromUtf8(isolate, info.c_str()));
}

void Bindings::read(const FunctionCallbackInfo<Value>& args)
{
    Isolate* isolate(args.GetIsolate());
    HandleScope scope(isolate);
    Bindings* obj = ObjectWrap::Unwrap<Bindings>(args.Holder());

    // Call the factory to get the specialized 'read' command based on
    // the input args.  If there is an error with the input args, this call
    // will attempt to make an error callback (if a callback argument can be
    // identified) and return a null ptr.
    const std::string readId(generateReadId());

    std::size_t i(0);
    const auto& schemaArg   (args[i++]);
    const auto& compressArg (args[i++]);
    const auto& queryArg    (args[i++]);
    const auto& initCbArg   (args[i++]);
    const auto& dataCbArg   (args[i++]);

    std::string errMsg("");

    if (!schemaArg->IsString() && !schemaArg->IsUndefined())
        errMsg += "\t'schema' must be a string or undefined";
    if (!compressArg->IsBoolean())  errMsg += "\t'compress' must be a boolean";
    if (!queryArg->IsObject())      errMsg += "\tInvalid query type";
    if (!initCbArg->IsFunction())   throw std::runtime_error("Invalid initCb");
    if (!dataCbArg->IsFunction())   throw std::runtime_error("Invalid dataCb");

    const std::string schemaString(
            schemaArg->IsString() ?
                *v8::String::Utf8Value(schemaArg->ToString()) :
                "");

    const bool compress(compressArg->BooleanValue());
    Local<Object> query(queryArg->ToObject());

    UniquePersistent<Function> initCb(
            isolate,
            Local<Function>::Cast(initCbArg));

    UniquePersistent<Function> dataCb(
            isolate,
            Local<Function>::Cast(dataCbArg));

    if (!errMsg.empty())
    {
        Status status(400, errMsg);
        const unsigned argc = 1;
        Local<Value> argv[argc] = { status.toObject(isolate) };

        Local<Function> local(Local<Function>::New(isolate, initCb));
        local->Call(isolate->GetCurrentContext()->Global(), argc, argv);
        return;
    }

    ReadCommand* readCommand(
            ReadCommandFactory::create(
                isolate,
                obj->m_session,
                obj->m_itcBufferPool,
                readId,
                schemaString,
                compress,
                query,
                std::move(initCb),
                std::move(dataCb)));

    if (!readCommand) return;

    // Register our callbacks with their async tokens.
    readCommand->registerInitCb();
    readCommand->registerDataCb();

    // Store our read command where our worker functions can access it.
    uv_work_t* req(new uv_work_t);
    req->data = readCommand;

    // Read points asynchronously.
    uv_queue_work(
        uv_default_loop(),
        req,
        (uv_work_cb)([](uv_work_t* req)->void
        {
            ReadCommand* readCommand(static_cast<ReadCommand*>(req->data));

            // Run the query.  This will ensure indexing if needed, and
            // will obtain everything needed to start streaming binary
            // data to the client.
            readCommand->safe([readCommand]()->void
            {
                try
                {
                    readCommand->run();
                }
                catch (entwine::InvalidQuery& e)
                {
                    readCommand->status.set(400, e.what());
                }
                catch (WrongQueryType& e)
                {
                    readCommand->status.set(400, e.what());
                }
                catch (...)
                {
                    readCommand->status.set(500, "Error during query");
                }
            });

            // Call initial informative callback.  If status is no good, we're
            // done here - don't continue for data.
            readCommand->doCb(readCommand->initAsync());
            if (!readCommand->status.ok()) { return; }

            readCommand->safe([readCommand]()->void
            {
                readCommand->acquire();

                try
                {
                    do
                    {
                        readCommand->read();
                        readCommand->doCb(readCommand->dataAsync());
                    }
                    while (!readCommand->done() && readCommand->status.ok());
                }
                catch (std::runtime_error& e)
                {
                    readCommand->status.set(500, e.what());
                }
                catch (...)
                {
                    readCommand->status.set(500, "Error during query");
                }
            });
        }),
        (uv_after_work_cb)([](uv_work_t* req, int status)->void
        {
            Isolate* isolate(Isolate::GetCurrent());
            HandleScope scope(isolate);
            ReadCommand* readCommand(static_cast<ReadCommand*>(req->data));

            delete readCommand;
            delete req;
        })
    );
}

//////////////////////////////////////////////////////////////////////////////

void init(Handle<Object> exports)
{
    Bindings::init(exports);
}

NODE_MODULE(session, init)

