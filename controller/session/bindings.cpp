#include <thread>
#include <sstream>

#include <node_buffer.h>
#include <curl/curl.h>
#include <openssl/crypto.h>

#include <pdal/PointLayout.hpp>
#include <pdal/StageFactory.hpp>

#include <entwine/drivers/s3.hpp>
#include <entwine/types/bbox.hpp>
#include <entwine/types/dim-info.hpp>
#include <entwine/types/point.hpp>
#include <entwine/types/schema.hpp>

#include "session.hpp"
#include "commands/create.hpp"
#include "commands/read.hpp"
#include "util/buffer-pool.hpp"
#include "util/once.hpp"

#include "bindings.hpp"

// TODO Remove.
using namespace v8;

namespace
{
    // TODO Configure.
    const std::size_t numBuffers = 1024;
    const std::size_t maxReadLength = 65536;
    ItcBufferPool itcBufferPool(numBuffers, maxReadLength);

    std::mutex factoryMutex;
    std::unique_ptr<pdal::StageFactory> stageFactory(new pdal::StageFactory());

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
    std::shared_ptr<entwine::Arbiter> arbiter(0);

    void parseAwsAuth(const v8::Local<v8::Value>& rawArg)
    {
        std::lock_guard<std::mutex> lock(initMutex);

        if (!arbiter)
        {
            entwine::DriverMap drivers;

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

                entwine::AwsAuth awsAuth(access, hidden);
                drivers.insert(
                        { "s3", std::make_shared<entwine::S3Driver>(awsAuth) });
            }

            arbiter.reset(new entwine::Arbiter(drivers));
        }
    }

    entwine::DimList parseDims(
            v8::Local<v8::Array> schemaArray,
            const entwine::Schema& sessionSchema)
    {
        entwine::DimList dims;

        for (std::size_t i(0); i < schemaArray->Length(); ++i)
        {
            Local<Object> dimObj(schemaArray->Get(
                        Integer::New(i))->ToObject());

            const std::string sizeString(*v8::String::Utf8Value(
                    dimObj->Get(String::New("size"))->ToString()));

            const std::size_t size(strtoul(sizeString.c_str(), 0, 0));

            if (size)
            {
                const std::string name(*v8::String::Utf8Value(
                        dimObj->Get(String::New("name"))->ToString()));

                const std::string baseTypeName(*v8::String::Utf8Value(
                        dimObj->Get(String::New("type"))->ToString()));

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

    Once once([]()->void {
        std::cout << "Destructing global environment" << std::endl;
        curl_global_cleanup();
    });
}

Persistent<Function> Bindings::constructor;

Bindings::Bindings()
    : m_session(new Session(*stageFactory, factoryMutex))
    , m_itcBufferPool(itcBufferPool)
{
    ghEnv::once.ensure([]()->void {
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
    tpl->PrototypeTemplate()->Set(String::NewSymbol("getNumPoints"),
        FunctionTemplate::New(getNumPoints)->GetFunction());
    tpl->PrototypeTemplate()->Set(String::NewSymbol("getSchema"),
        FunctionTemplate::New(getSchema)->GetFunction());
    tpl->PrototypeTemplate()->Set(String::NewSymbol("getStats"),
        FunctionTemplate::New(getStats)->GetFunction());
    tpl->PrototypeTemplate()->Set(String::NewSymbol("getSrs"),
        FunctionTemplate::New(getSrs)->GetFunction());
    tpl->PrototypeTemplate()->Set(String::NewSymbol("getBounds"),
        FunctionTemplate::New(getBounds)->GetFunction());
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
    std::cout << "Bindings::create" << std::endl;
    HandleScope scope;
    Bindings* obj = ObjectWrap::Unwrap<Bindings>(args.This());

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

    if (!nameArg->IsString())
        errMsg += "\t'name' must be a string";

    if (!inputsArg->IsArray())
        errMsg += "\t'inputs' must be an array of strings";

    if (!outputArg->IsString())
        errMsg += "\t'output' must be a string";

    if (!cbArg->IsFunction())
        throw std::runtime_error("Invalid callback supplied to 'create'");

    Persistent<Function> callback(
            Persistent<Function>::New(Local<Function>::Cast(cbArg)));

    if (errMsg.size())
    {
        Status status(400, errMsg);
        const unsigned argc = 1;
        Local<Value> argv[argc] = { status.toObject() };

        callback->Call(Context::GetCurrent()->Global(), argc, argv);
        return scope.Close(Undefined());
    }

    parseAwsAuth(awsAuthArg);
    const std::string name(*v8::String::Utf8Value(nameArg->ToString()));
    const std::vector<std::string> inputs(parsePathList(inputsArg));
    const std::string output(*v8::String::Utf8Value(outputArg->ToString()));
    const std::size_t maxQuerySize(queryArg->IntegerValue());
    const std::size_t maxCacheSize(cacheArg->IntegerValue());

    const Paths paths(inputs, output);

    // Store everything we'll need to perform initialization.
    uv_work_t* req(new uv_work_t);
    req->data = new CreateData(
            obj->m_session,
            name,
            paths,
            maxQuerySize,
            maxCacheSize,
            arbiter,
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
                        createData->maxQuerySize,
                        createData->maxCacheSize,
                        createData->arbiter))
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

Handle<Value> Bindings::getNumPoints(const Arguments& args)
{
    HandleScope scope;
    Bindings* obj = ObjectWrap::Unwrap<Bindings>(args.This());

    return scope.Close(Integer::New(obj->m_session->getNumPoints()));
}

Handle<Value> Bindings::getSchema(const Arguments& args)
{
    HandleScope scope;
    Bindings* obj = ObjectWrap::Unwrap<Bindings>(args.This());

    const entwine::Schema& schema(obj->m_session->schema());
    const auto& dims(schema.dims());

    // Convert our entwine::Schema to a JS array.
    Local<Array> jsSchema(Array::New(dims.size()));

    for (std::size_t i(0); i < dims.size(); ++i)
    {
        const auto& dim(dims[i]);

        Local<Object> jsDim(Object::New());

        jsDim->Set(
                String::NewSymbol("name"),
                String::New(dim.name().data(), dim.name().size()));

        jsDim->Set(
                String::NewSymbol("type"),
                String::New(dim.typeString().data(), dim.typeString().size()));

        jsDim->Set(
                String::NewSymbol("size"),
                Integer::New(dim.size()));

        jsSchema->Set(Integer::New(i), jsDim);
    }

    return scope.Close(jsSchema);
}

Handle<Value> Bindings::getStats(const Arguments& args)
{
    HandleScope scope;
    Bindings* obj = ObjectWrap::Unwrap<Bindings>(args.This());

    const std::string stats(obj->m_session->getStats());

    return scope.Close(String::New(stats.data(), stats.size()));
}

Handle<Value> Bindings::getSrs(const Arguments& args)
{
    HandleScope scope;
    Bindings* obj = ObjectWrap::Unwrap<Bindings>(args.This());

    const std::string wkt(obj->m_session->getSrs());

    return scope.Close(String::New(wkt.data(), wkt.size()));
}

Handle<Value> Bindings::getBounds(const Arguments& args)
{
    HandleScope scope;
    Bindings* obj = ObjectWrap::Unwrap<Bindings>(args.This());

    const entwine::BBox bbox(obj->m_session->getBounds());

    v8::Handle<v8::Array> jsBounds = v8::Array::New(4);

    jsBounds->Set(0, v8::Number::New(bbox.min().x));
    jsBounds->Set(1, v8::Number::New(bbox.min().y));
    jsBounds->Set(2, v8::Number::New(bbox.min().z));
    jsBounds->Set(3, v8::Number::New(bbox.max().x));
    jsBounds->Set(4, v8::Number::New(bbox.max().y));
    jsBounds->Set(5, v8::Number::New(bbox.max().z));

    return scope.Close(jsBounds);
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
    const auto& readCbArg   (args[i++]);
    const auto& dataCbArg   (args[i++]);

    std::string errMsg("");

    if (!schemaArg->IsArray())
        errMsg += "\t'schema' must be a string";

    if (!compressArg->IsBoolean())
        errMsg += "\t'compress' must be a boolean";

    if (!dataCbArg->IsFunction())
        errMsg += "\t'dataCb' must be a function";

    if (!readCbArg->IsFunction())
        throw std::runtime_error("Invalid callback supplied to 'read'");

    entwine::DimList dims;

    const Local<Array> schemaArray(Array::Cast(*schemaArg));

    if (schemaArray->Length())
    {
        dims = parseDims(schemaArray, obj->m_session->schema());

        if (!dims.size())
            errMsg += "\t'schema' has no valid dimensions";
    }

    Persistent<Function> readCb(
            Persistent<Function>::New(Local<Function>::Cast(readCbArg)));

    if (errMsg.size())
    {
        Status status(400, errMsg);
        const unsigned argc = 1;
        Local<Value> argv[argc] = { status.toObject() };

        readCb->Call(Context::GetCurrent()->Global(), argc, argv);
        return scope.Close(Undefined());
    }

    const bool compress(compressArg->BooleanValue());
    Local<Object> query(queryArg->ToObject());

    Persistent<Function> dataCb(
            Persistent<Function>::New(Local<Function>::Cast(dataCbArg)));

    ReadCommand* readCommand(
            ReadCommandFactory::create(
                obj->m_session,
                obj->m_itcBufferPool,
                readId,
                dims,
                compress,
                query,
                readCb,
                dataCb));

    if (!readCommand)
    {
        Status status(400, std::string("Invalid read query parameters"));
        const unsigned argc = 1;
        Local<Value> argv[argc] = { status.toObject() };

        readCb->Call(Context::GetCurrent()->Global(), argc, argv);
        return scope.Close(Undefined());
    }

    // Store our read command where our worker functions can access it.
    uv_work_t* readReq(new uv_work_t);
    readReq->data = readCommand;

    // Read points asynchronously.
    uv_queue_work(
        uv_default_loop(),
        readReq,
        (uv_work_cb)([](uv_work_t* readReq)->void
        {
            ReadCommand* readCommand(static_cast<ReadCommand*>(readReq->data));

            readCommand->safe([readCommand]()->void {
                // Run the query.  This will ensure indexing if needed, and
                // will obtain everything needed to start streaming binary
                // data to the client.
                readCommand->run();
            });
        }),
        (uv_after_work_cb)([](uv_work_t* readReq, int status)->void
        {
            ReadCommand* readCommand(
                static_cast<ReadCommand*>(readReq->data));

            if (!readCommand->status.ok())
            {
                HandleScope scope;
                const unsigned argc = 1;
                Local<Value> argv[argc] = { readCommand->status.toObject() };

                readCommand->readCb()->Call(
                    Context::GetCurrent()->Global(), argc, argv);

                delete readCommand;
                delete readReq;
                scope.Close(Undefined());
                return;
            }

            const std::string id(readCommand->readId());

            // Call the initial callback.
            {
                const unsigned argc = 5;
                Local<Value> argv[argc] =
                {
                    Local<Value>::New(Null()), // err
                    Local<Value>::New(String::New(id.data(), id.size())),
                    Local<Value>::New(Integer::New(readCommand->numPoints())),
                    Local<Value>::New(Integer::New(readCommand->numBytes())),
                    getRasterMeta(readCommand)
                };

                readCommand->readCb()->Call(
                    Context::GetCurrent()->Global(), argc, argv);
            }

            uv_work_t* dataReq(new uv_work_t);
            dataReq->data = readCommand;

            uv_async_init(
                uv_default_loop(),
                readCommand->async(),
                ([](uv_async_t* async, int status)->void
                {
                    HandleScope scope;

                    ReadCommand* readCommand(
                        static_cast<ReadCommand*>(async->data));

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

                    readCommand->getBuffer()->flush();
                    scope.Close(Undefined());
                })
            );

            uv_queue_work(
                uv_default_loop(),
                dataReq,
                (uv_work_cb)([](uv_work_t* dataReq)->void
                {
                    ReadCommand* readCommand(
                        static_cast<ReadCommand*>(dataReq->data));

                    readCommand->safe([readCommand]()->void
                    {
                        readCommand->acquire();

                        // Go through this once even if there is no data for
                        // this query so we close any outstanding sockets.
                        do
                        {
                            readCommand->getBuffer()->grab();
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

                            // A bit strange, but we need to send this via the
                            // same async token used in the uv_async_init()
                            // call, so it must be a member of ReadCommand
                            // since that object is the only thing we can
                            // access in this background work queue.
                            readCommand->async()->data = readCommand;
                            uv_async_send(readCommand->async());
                        }
                        while (
                                !readCommand->done() &&
                                readCommand->status.ok());

                        readCommand->getBufferPool().release(
                            readCommand->getBuffer());
                    });
                }),
                (uv_after_work_cb)([](uv_work_t* dataReq, int status)->void
                {
                    ReadCommand* readCommand(
                        static_cast<ReadCommand*>(dataReq->data));

                    uv_handle_t* async(
                        reinterpret_cast<uv_handle_t*>(
                            readCommand->async()));

                    uv_close_cb closeCallback(
                        (uv_close_cb)([](uv_handle_t* async)->void
                        {
                            delete async;
                        }));

                    uv_close(async, closeCallback);

                    delete dataReq;
                    delete readCommand;
                })
            );

            delete readReq;
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

