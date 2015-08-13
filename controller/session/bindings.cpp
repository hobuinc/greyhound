#include <thread>
#include <sstream>

#include <execinfo.h>
#include <unistd.h>

#include <curl/curl.h>
#include <openssl/crypto.h>

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
    const std::size_t maxReadLength = 65536;
    ItcBufferPool itcBufferPool(numBuffers, maxReadLength);

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

    void initConfigurable(
            Isolate* isolate,
            const v8::Local<v8::Value>& rawArg,
            const std::size_t maxCacheSize,
            const std::size_t maxQuerySize)
    {
        std::lock_guard<std::mutex> lock(initMutex);

        if (!cache)
        {
            signal(SIGSEGV, handler);

            cache.reset(new entwine::Cache(maxCacheSize, maxQuerySize));
        }

        if (!commonArbiter)
        {
            if (!rawArg->IsUndefined() && rawArg->IsObject())
            {
                Object* rawObj(Object::Cast(*rawArg));

                const v8::Local<v8::Value>& rawAccess(
                        rawObj->Get(String::NewFromUtf8(isolate, "access")));
                const v8::Local<v8::Value>& rawHidden(
                        rawObj->Get(String::NewFromUtf8(isolate, "hidden")));

                const std::string access(
                        *v8::String::Utf8Value(rawAccess->ToString()));
                const std::string hidden(
                        *v8::String::Utf8Value(rawHidden->ToString()));

                commonArbiter.reset(
                        new arbiter::Arbiter(
                            arbiter::AwsAuth(access, hidden)));
            }
            else
            {
                commonArbiter.reset(new arbiter::Arbiter());
            }
        }
    }

    entwine::DimList parseDims(
            Isolate* isolate,
            v8::Array* schemaArray,
            const entwine::Schema& sessionSchema)
    {
        entwine::DimList dims;

        for (std::size_t i(0); i < schemaArray->Length(); ++i)
        {
            Local<Object> dimObj(schemaArray->Get(
                        Integer::New(isolate, i))->ToObject());

            const std::string sizeString(*v8::String::Utf8Value(
                    dimObj->Get(String::NewFromUtf8(isolate, "size"))
                    ->ToString()));

            const std::size_t size(strtoul(sizeString.c_str(), 0, 0));

            if (size)
            {
                const std::string name(*v8::String::Utf8Value(
                        dimObj->Get(String::NewFromUtf8(isolate, "name"))
                        ->ToString()));

                const std::string baseTypeName(*v8::String::Utf8Value(
                        dimObj->Get(String::NewFromUtf8(isolate, "type"))
                        ->ToString()));

                const pdal::Dimension::Id::Enum id(
                        sessionSchema.pdalLayout().findDim(name));

                if (sessionSchema.pdalLayout().hasDim(id))
                {
                    dims.push_back(
                            entwine::DimInfo(
                                name,
                                baseTypeName,
                                size));
                }
            }
        }

        return dims;
    }

    entwine::DimList parseDimList(
            Isolate* isolate,
            const v8::Local<v8::Value>& schemaArg,
            const entwine::Schema& sessionSchema)
    {
        entwine::DimList dims;
        Array* schemaArray(Array::Cast(*schemaArg));
        if (schemaArray->Length())
        {
            dims = parseDims(isolate, schemaArray, sessionSchema);
        }

        return dims;
    }
}

namespace ghEnv
{
    std::vector<std::mutex> sslMutexList(CRYPTO_num_locks());

    void sslLock(int mode, int n, const char* file, int line)
    {
        if (mode & CRYPTO_LOCK)
        {
            sslMutexList.at(n).lock();
        }
        else
        {
            sslMutexList.at(n).unlock();
        }
    }

    unsigned long sslId()
    {
        std::ostringstream ss;
        ss << std::this_thread::get_id();
        return std::stoull(ss.str());
    }

    CRYPTO_dynlock_value* dynamicCreate(const char* file, int line)
    {
        return new CRYPTO_dynlock_value();
    }

    void dynamicLock(
            int mode,
            CRYPTO_dynlock_value* lock,
            const char* file,
            int line)
    {
        if (mode & CRYPTO_LOCK)
        {
            lock->mutex.lock();
        }
        else
        {
            lock->mutex.unlock();
        }
    }

    void dynamicDestroy(CRYPTO_dynlock_value* lock, const char* file, int line)
    {
        delete lock;
    }

    Once curlCryptoOnce([]()->void {
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
    ghEnv::curlCryptoOnce.ensure([]()->void {
        std::cout << "Initializing global environment" << std::endl;
        curl_global_init(CURL_GLOBAL_ALL);

        CRYPTO_set_id_callback(ghEnv::sslId);
        CRYPTO_set_locking_callback(ghEnv::sslLock);
        CRYPTO_set_dynlock_create_callback(ghEnv::dynamicCreate);
        CRYPTO_set_dynlock_lock_callback(ghEnv::dynamicLock);
        CRYPTO_set_dynlock_destroy_callback(ghEnv::dynamicDestroy);
    });
}

Bindings::~Bindings()
{ }

void Bindings::init(v8::Handle<v8::Object> exports)
{
    Isolate* isolate(Isolate::GetCurrent());

    // Prepare constructor template
    Local<FunctionTemplate> tpl(FunctionTemplate::New(isolate, construct));
    tpl->SetClassName(String::NewFromUtf8(isolate, "Bindings"));
    tpl->InstanceTemplate()->SetInternalFieldCount(1);

    // Prototype
    NODE_SET_PROTOTYPE_METHOD(tpl, "construct",     construct);
    NODE_SET_PROTOTYPE_METHOD(tpl, "create",        create);
    NODE_SET_PROTOTYPE_METHOD(tpl, "destroy",       destroy);
    NODE_SET_PROTOTYPE_METHOD(tpl, "getNumPoints",  getNumPoints);
    NODE_SET_PROTOTYPE_METHOD(tpl, "getSchema",     getSchema);
    NODE_SET_PROTOTYPE_METHOD(tpl, "getStats",      getStats);
    NODE_SET_PROTOTYPE_METHOD(tpl, "getSrs",        getSrs);
    NODE_SET_PROTOTYPE_METHOD(tpl, "getBounds",     getBounds);
    NODE_SET_PROTOTYPE_METHOD(tpl, "getType",       getType);
    NODE_SET_PROTOTYPE_METHOD(tpl, "read",          read);

    constructor.Reset(isolate, tpl->GetFunction());
    exports->Set(String::NewFromUtf8(isolate, "Bindings"), tpl->GetFunction());
}

void Bindings::construct(const FunctionCallbackInfo<Value>& args)
{
    Isolate* isolate(Isolate::GetCurrent());
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
    Isolate* isolate(Isolate::GetCurrent());
    HandleScope scope(isolate);

    Bindings* obj = ObjectWrap::Unwrap<Bindings>(args.Holder());

    if (args.Length() != 7)
    {
        throw std::runtime_error("Wrong number of arguments to create");
    }

    std::size_t i(0);
    const auto& nameArg     (args[i++]);
    const auto& awsAuthArg  (args[i++]);
    const auto& inputsArg   (args[i++]);
    const auto& outputArg   (args[i++]);
    const auto& queryArg    (args[i++]);
    const auto& cacheArg    (args[i++]);
    const auto& cbArg       (args[i++]);

    std::string errMsg("");

    if (!nameArg->IsString())   errMsg += "\t'name' must be a string";
    if (!inputsArg->IsArray())  errMsg += "\t'inputs' must be an array";
    if (!outputArg->IsString()) errMsg += "\t'output' must be a string";
    if (!cbArg->IsFunction()) throw std::runtime_error("Invalid create CB");

    UniquePersistent<Function> callback(isolate, Local<Function>::Cast(cbArg));

    if (errMsg.size())
    {
        Status status(400, errMsg);
        const unsigned argc = 1;
        Local<Value> argv[argc] = { status.toObject(isolate) };

        Local<Function> local(Local<Function>::New(isolate, callback));

        local->Call(isolate->GetCurrentContext()->Global(), argc, argv);
        callback.Reset();
        return;
    }

    const std::string name(*v8::String::Utf8Value(nameArg->ToString()));
    const std::vector<std::string> inputs(parsePathList(isolate, inputsArg));
    const std::string output(*v8::String::Utf8Value(outputArg->ToString()));
    const std::size_t maxQuerySize(queryArg->IntegerValue());
    const std::size_t maxCacheSize(cacheArg->IntegerValue());

    initConfigurable(isolate, awsAuthArg, maxCacheSize, maxQuerySize);

    const Paths paths(inputs, output);

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
    Isolate* isolate(Isolate::GetCurrent());
    HandleScope scope(isolate);
    Bindings* obj = ObjectWrap::Unwrap<Bindings>(args.Holder());

    obj->m_session.reset();
}

void Bindings::getNumPoints(const FunctionCallbackInfo<Value>& args)
{
    Isolate* isolate(Isolate::GetCurrent());
    HandleScope scope(isolate);
    Bindings* obj = ObjectWrap::Unwrap<Bindings>(args.Holder());

    const std::size_t numPoints(obj->m_session->getNumPoints());
    args.GetReturnValue().Set(Number::New(isolate, numPoints));
}

void Bindings::getSchema(const FunctionCallbackInfo<Value>& args)
{
    Isolate* isolate(Isolate::GetCurrent());
    HandleScope scope(isolate);
    Bindings* obj = ObjectWrap::Unwrap<Bindings>(args.Holder());

    const entwine::Schema& schema(obj->m_session->schema());
    const auto& dims(schema.dims());

    // Convert our entwine::Schema to a JS array.
    Local<Array> jsSchema(Array::New(isolate, dims.size()));

    for (std::size_t i(0); i < dims.size(); ++i)
    {
        const auto& dim(dims[i]);

        Local<Object> jsDim(Object::New(isolate));

        jsDim->Set(
                String::NewFromUtf8(isolate, "name"),
                String::NewFromUtf8(isolate, dim.name().c_str()));

        jsDim->Set(
                String::NewFromUtf8(isolate, "type"),
                String::NewFromUtf8(isolate, dim.typeString().c_str()));

        jsDim->Set(
                String::NewFromUtf8(isolate, "size"),
                Integer::New(isolate, dim.size()));

        jsSchema->Set(Integer::New(isolate, i), jsDim);
    }

    args.GetReturnValue().Set(jsSchema);
}

void Bindings::getStats(const FunctionCallbackInfo<Value>& args)
{
    Isolate* isolate(Isolate::GetCurrent());
    HandleScope scope(isolate);
    Bindings* obj = ObjectWrap::Unwrap<Bindings>(args.Holder());

    const std::string stats(obj->m_session->getStats());

    args.GetReturnValue().Set(String::NewFromUtf8(isolate, stats.c_str()));
}

void Bindings::getSrs(const FunctionCallbackInfo<Value>& args)
{
    Isolate* isolate(Isolate::GetCurrent());
    HandleScope scope(isolate);
    Bindings* obj = ObjectWrap::Unwrap<Bindings>(args.Holder());

    const std::string wkt(obj->m_session->getSrs());

    args.GetReturnValue().Set(String::NewFromUtf8(isolate, wkt.c_str()));
}

void Bindings::getBounds(const FunctionCallbackInfo<Value>& args)
{
    Isolate* isolate(Isolate::GetCurrent());
    HandleScope scope(isolate);
    Bindings* obj = ObjectWrap::Unwrap<Bindings>(args.Holder());

    const entwine::BBox bbox(obj->m_session->getBounds());

    v8::Handle<v8::Array> jsBounds = v8::Array::New(isolate, 6);

    jsBounds->Set(0, v8::Number::New(isolate, bbox.min().x));
    jsBounds->Set(1, v8::Number::New(isolate, bbox.min().y));
    jsBounds->Set(2, v8::Number::New(isolate, bbox.min().z));
    jsBounds->Set(3, v8::Number::New(isolate, bbox.max().x));
    jsBounds->Set(4, v8::Number::New(isolate, bbox.max().y));
    jsBounds->Set(5, v8::Number::New(isolate, bbox.max().z));

    args.GetReturnValue().Set(jsBounds);
}

void Bindings::getType(const FunctionCallbackInfo<Value>& args)
{
    Isolate* isolate(Isolate::GetCurrent());
    HandleScope scope(isolate);
    Bindings* obj = ObjectWrap::Unwrap<Bindings>(args.Holder());

    const std::string type(obj->m_session->getType());

    args.GetReturnValue().Set(String::NewFromUtf8(isolate, type.c_str()));
}

void Bindings::read(const FunctionCallbackInfo<Value>& args)
{
    Isolate* isolate(Isolate::GetCurrent());
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

    if (!schemaArg->IsArray())      errMsg += "\t'schema' must be an array";
    if (!compressArg->IsBoolean())  errMsg += "\t'compress' must be a boolean";
    if (!queryArg->IsObject())      errMsg += "\tInvalid query type";
    if (!initCbArg->IsFunction())   throw std::runtime_error("Invalid initCb");
    if (!dataCbArg->IsFunction())   throw std::runtime_error("Invalid dataCb");

    entwine::DimList dims(
            parseDimList(
                isolate,
                schemaArg,
                obj->m_session->schema()));

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

        Local<Function> local(Local<Function>::New(
                isolate,
                initCb));

        local->Call(isolate->GetCurrentContext()->Global(), argc, argv);
        return;
    }

    ReadCommand* readCommand(
            ReadCommandFactory::create(
                isolate,
                obj->m_session,
                obj->m_itcBufferPool,
                readId,
                dims,
                compress,
                query,
                std::move(initCb),
                std::move(dataCb)));

    if (!readCommand)
    {
        Status status(400, std::string("Invalid read query parameters"));
        const unsigned argc = 1;
        Local<Value> argv[argc] = { status.toObject(isolate) };

        Local<Function> local(Local<Function>::New(
                isolate,
                initCb));

        local->Call(isolate->GetCurrentContext()->Global(), argc, argv);
        return;
    }

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
                catch (entwine::QueryLimitExceeded& e)
                {
                    readCommand->status.set(413, e.what());
                }
                catch (entwine::InvalidQuery& e)
                {
                    readCommand->status.set(400, e.what());
                }
            });

            // Call initial informative callback.  If status is no good, we're
            // done here - don't continue for data.
            readCommand->doCb(readCommand->initAsync());
            if (!readCommand->status.ok()) { return; }

            readCommand->safe([readCommand]()->void
            {
                readCommand->acquire();

                do
                {
                    try
                    {
                        readCommand->read(maxReadLength);
                    }
                    catch (std::runtime_error& e)
                    {
                        readCommand->status.set(500, e.what());
                    }
                    catch (...)
                    {
                        readCommand->status.set(500, "Error in query");
                    }

                    readCommand->doCb(readCommand->dataAsync());
                }
                while (!readCommand->done() && readCommand->status.ok());

                readCommand->getBufferPool().release(readCommand->getBuffer());
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

