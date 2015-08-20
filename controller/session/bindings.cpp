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

    std::vector<std::string> parsePathList(const v8::Local<v8::Value>& rawArg)
    {
        std::vector<std::string> paths;

        if (!rawArg->IsUndefined() && rawArg->IsArray())
        {
            Local<Array> rawArray(Array::Cast(*rawArg));

            for (std::size_t i(0); i < rawArray->Length(); ++i)
            {
                const v8::Local<v8::Value>& rawValue(
                    rawArray->Get(Integer::New(i)));

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
                Local<Object> rawObj(Object::Cast(*rawArg));

                const v8::Local<v8::Value>& rawAccess(
                        rawObj->Get(String::NewSymbol("access")));
                const v8::Local<v8::Value>& rawHidden(
                        rawObj->Get(String::NewSymbol("hidden")));

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
}

namespace ghEnv
{
    std::vector<std::mutex> sslMutexList(CRYPTO_num_locks());

    void sslLock(int mode, int n, const char* file, int line)
    {
        if (mode & CRYPTO_LOCK) sslMutexList.at(n).lock();
        else sslMutexList.at(n).unlock();
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
        if (mode & CRYPTO_LOCK) lock->mutex.lock();
        else lock->mutex.unlock();
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
    // Prepare constructor template
    Local<FunctionTemplate> tpl = FunctionTemplate::New(construct);
    tpl->SetClassName(String::NewSymbol("Bindings"));
    tpl->InstanceTemplate()->SetInternalFieldCount(1);

    // Prototype
    tpl->PrototypeTemplate()->Set(String::NewSymbol("construct"),
        FunctionTemplate::New(construct)->GetFunction());
    tpl->PrototypeTemplate()->Set(String::NewSymbol("create"),
        FunctionTemplate::New(create)->GetFunction());
    tpl->PrototypeTemplate()->Set(String::NewSymbol("destroy"),
        FunctionTemplate::New(destroy)->GetFunction());
    tpl->PrototypeTemplate()->Set(String::NewSymbol("info"),
        FunctionTemplate::New(info)->GetFunction());
    tpl->PrototypeTemplate()->Set(String::NewSymbol("read"),
        FunctionTemplate::New(read)->GetFunction());

    constructor = Persistent<Function>::New(tpl->GetFunction());
    exports->Set(String::NewSymbol("Bindings"), constructor);
}

Handle<Value> Bindings::construct(const Arguments& args)
{
    HandleScope scope;

    if (args.IsConstructCall())
    {
        // Invoked as constructor with 'new'.
        Bindings* obj = new Bindings();
        obj->Wrap(args.This());
        return args.This();
    }
    else
    {
        // Invoked as a function, turn into construct call.
        return scope.Close(constructor->NewInstance());
    }
}

Handle<Value> Bindings::create(const Arguments& args)
{
    HandleScope scope;
    Bindings* obj = ObjectWrap::Unwrap<Bindings>(args.This());

    if (args.Length() != 6)
    {
        throw std::runtime_error("Wrong number of arguments to create");
    }

    std::size_t i(0);
    const auto& nameArg     (args[i++]);
    const auto& awsAuthArg  (args[i++]);
    const auto& pathsArg    (args[i++]);
    const auto& queryArg    (args[i++]);
    const auto& cacheArg    (args[i++]);
    const auto& cbArg       (args[i++]);

    std::string errMsg("");

    if (!nameArg->IsString()) errMsg += "\t'name' must be a string";
    if (!pathsArg->IsArray()) errMsg += "\t'paths' must be an array";
    if (!cbArg->IsFunction()) throw std::runtime_error("Invalid create CB");

    Persistent<Function> callback(
            Persistent<Function>::New(Local<Function>::Cast(cbArg)));

    if (errMsg.size())
    {
        std::cout << "Client error: " << errMsg << std::endl;
        Status status(400, errMsg);
        const unsigned argc = 1;
        Local<Value> argv[argc] = { status.toObject() };

        callback->Call(Context::GetCurrent()->Global(), argc, argv);
        return scope.Close(Undefined());
    }

    const std::string name(*v8::String::Utf8Value(nameArg->ToString()));
    const std::vector<std::string> paths(parsePathList(pathsArg));
    const std::size_t maxQuerySize(queryArg->IntegerValue());
    const std::size_t maxCacheSize(cacheArg->IntegerValue());

    initConfigurable(awsAuthArg, maxCacheSize, maxQuerySize);

    // Store everything we'll need to perform initialization.
    uv_work_t* req(new uv_work_t);
    req->data = new CreateData(
            obj->m_session,
            name,
            paths,
            commonArbiter,
            cache,
            callback);

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
            HandleScope scope;
            CreateData* createData(static_cast<CreateData*>(req->data));

            const unsigned argc = 1;
            Local<Value> argv[argc] = { createData->status.toObject() };

            createData->callback->Call(
                Context::GetCurrent()->Global(), argc, argv);

            delete createData;
            delete req;
        })
    );

    return scope.Close(Undefined());
}

Handle<Value> Bindings::destroy(const Arguments& args)
{
    HandleScope scope;
    Bindings* obj = ObjectWrap::Unwrap<Bindings>(args.This());

    obj->m_session.reset();

    return scope.Close(Undefined());
}

Handle<Value> Bindings::info(const Arguments& args)
{
    HandleScope scope;
    Bindings* obj = ObjectWrap::Unwrap<Bindings>(args.This());

    const std::string info(obj->m_session->info());
    return scope.Close(String::New(info.data(), info.size()));
}

Handle<Value> Bindings::read(const Arguments& args)
{
    HandleScope scope;
    Bindings* obj = ObjectWrap::Unwrap<Bindings>(args.This());

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

    Persistent<Function> initCb(
            Persistent<Function>::New(Local<Function>::Cast(initCbArg)));

    Persistent<Function> dataCb(
            Persistent<Function>::New(Local<Function>::Cast(dataCbArg)));

    if (!errMsg.empty())
    {
        Status status(400, errMsg);
        const unsigned argc = 1;
        Local<Value> argv[argc] = { status.toObject() };

        initCb->Call(Context::GetCurrent()->Global(), argc, argv);
        return scope.Close(Undefined());
    }

    ReadCommand* readCommand(
            ReadCommandFactory::create(
                obj->m_session,
                obj->m_itcBufferPool,
                readId,
                schemaString,
                compress,
                query,
                initCb,
                dataCb));

    if (!readCommand)
    {
        Status status(400, std::string("Invalid read query parameters"));
        const unsigned argc = 1;
        Local<Value> argv[argc] = { status.toObject() };

        initCb->Call(Context::GetCurrent()->Global(), argc, argv);
        return scope.Close(Undefined());
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
                        readCommand->status.set(500, "Error during query");
                    }

                    readCommand->doCb(readCommand->dataAsync());
                }
                while (!readCommand->done() && readCommand->status.ok());

                readCommand->getBufferPool().release(readCommand->getBuffer());
            });
        }),
        (uv_after_work_cb)([](uv_work_t* req, int status)->void
        {
            HandleScope scope;
            ReadCommand* readCommand(static_cast<ReadCommand*>(req->data));

            delete readCommand;
            delete req;
        })
    );

    return scope.Close(Undefined());
}

//////////////////////////////////////////////////////////////////////////////

void init(Handle<Object> exports)
{
    Bindings::init(exports);
}

NODE_MODULE(session, init)

