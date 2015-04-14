#include <thread>
#include <sstream>

#include <node_buffer.h>
#include <curl/curl.h>
#include <openssl/crypto.h>
#include <entwine/types/bbox.hpp>
#include <entwine/types/dim-info.hpp>
#include <entwine/types/point.hpp>
#include <entwine/types/schema.hpp>

#include "buffer-pool.hpp"
#include "pdal-session.hpp"
#include "commands/create.hpp"
#include "commands/read.hpp"
#include "util/once.hpp"

#include "pdal-bindings.hpp"

// TODO Remove.
using namespace v8;

namespace
{
    // TODO Configure.
    const std::size_t numBuffers = 1024;
    const std::size_t maxReadLength = 65536;
    static ItcBufferPool itcBufferPool(numBuffers, maxReadLength);

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

    entwine::S3Info parseS3Info(const v8::Local<v8::Value>& rawArg)
    {
        entwine::S3Info info;
        if (!rawArg->IsUndefined() && rawArg->IsArray())
        {
            Local<Array> rawArray(Array::Cast(*rawArg));

            if (rawArray->Length() == 4)
            {
                const v8::Local<v8::Value>& rawUrl(
                        rawArray->Get(Integer::New(0)));
                const v8::Local<v8::Value>& rawBucket(
                        rawArray->Get(Integer::New(1)));
                const v8::Local<v8::Value>& rawAccess(
                        rawArray->Get(Integer::New(2)));
                const v8::Local<v8::Value>& rawHidden(
                        rawArray->Get(Integer::New(3)));

                const std::string url(
                        *v8::String::Utf8Value(rawUrl->ToString()));
                const std::string bucket(
                        *v8::String::Utf8Value(rawBucket->ToString()));
                const std::string access(
                        *v8::String::Utf8Value(rawAccess->ToString()));
                const std::string hidden(
                        *v8::String::Utf8Value(rawHidden->ToString()));

                return entwine::S3Info(url, bucket, access, hidden);
            }
        }

        return info;
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

Persistent<Function> PdalBindings::constructor;

PdalBindings::PdalBindings()
    : m_pdalSession(new PdalSession())
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

PdalBindings::~PdalBindings()
{ }

void PdalBindings::init(v8::Handle<v8::Object> exports)
{
    // Prepare constructor template
    Local<FunctionTemplate> tpl = FunctionTemplate::New(construct);
    tpl->SetClassName(String::NewSymbol("PdalBindings"));
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
    tpl->PrototypeTemplate()->Set(String::NewSymbol("read"),
        FunctionTemplate::New(read)->GetFunction());

    constructor = Persistent<Function>::New(tpl->GetFunction());
    exports->Set(String::NewSymbol("PdalBindings"), constructor);
}

Handle<Value> PdalBindings::construct(const Arguments& args)
{
    HandleScope scope;

    if (args.IsConstructCall())
    {
        // Invoked as constructor with 'new'.
        PdalBindings* obj = new PdalBindings();
        obj->Wrap(args.This());
        return args.This();
    }
    else
    {
        // Invoked as a function, turn into construct call.
        return scope.Close(constructor->NewInstance());
    }
}

Handle<Value> PdalBindings::create(const Arguments& args)
{
    std::cout << "PdalBindings::create" << std::endl;
    HandleScope scope;
    PdalBindings* obj = ObjectWrap::Unwrap<PdalBindings>(args.This());

    if (args.Length() != 5)
    {
        throw std::runtime_error("Wrong number of arguments to create");
    }

    std::size_t i(0);
    const auto& nameArg     (args[i++]);
    const auto& s3Arg       (args[i++]);
    const auto& inputsArg   (args[i++]);
    const auto& outputArg   (args[i++]);
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

    const std::string name(*v8::String::Utf8Value(nameArg->ToString()));
    const entwine::S3Info s3Info(parseS3Info(s3Arg));
    const std::vector<std::string> inputs(parsePathList(inputsArg));
    const std::string output(*v8::String::Utf8Value(outputArg->ToString()));

    const Paths paths(s3Info, inputs, output);

    if (errMsg.size())
    {
        Status status(400, errMsg);
        const unsigned argc = 1;
        Local<Value> argv[argc] = { status.toObject() };

        callback->Call(Context::GetCurrent()->Global(), argc, argv);
        return scope.Close(Undefined());
    }

    // Store everything we'll need to perform initialization.
    uv_work_t* req(new uv_work_t);
    req->data = new CreateData(obj->m_pdalSession, name, paths, callback);

    std::cout << "Queueing create" << std::endl;
    uv_queue_work(
        uv_default_loop(),
        req,
        (uv_work_cb)([](uv_work_t *req)->void
        {
            CreateData* createData(static_cast<CreateData*>(req->data));

            createData->safe([createData]()->void
            {
                if (!createData->pdalSession->initialize(
                        createData->name,
                        createData->paths))
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

Handle<Value> PdalBindings::destroy(const Arguments& args)
{
    HandleScope scope;
    PdalBindings* obj = ObjectWrap::Unwrap<PdalBindings>(args.This());

    obj->m_pdalSession.reset();

    return scope.Close(Undefined());
}

Handle<Value> PdalBindings::getNumPoints(const Arguments& args)
{
    HandleScope scope;
    PdalBindings* obj = ObjectWrap::Unwrap<PdalBindings>(args.This());

    return scope.Close(Integer::New(obj->m_pdalSession->getNumPoints()));
}

Handle<Value> PdalBindings::getSchema(const Arguments& args)
{
    HandleScope scope;
    PdalBindings* obj = ObjectWrap::Unwrap<PdalBindings>(args.This());

    // TODO Return object.
    const std::string schema(obj->m_pdalSession->getSchemaString());

    return scope.Close(String::New(schema.data(), schema.size()));
}

Handle<Value> PdalBindings::getStats(const Arguments& args)
{
    HandleScope scope;
    PdalBindings* obj = ObjectWrap::Unwrap<PdalBindings>(args.This());

    const std::string stats(obj->m_pdalSession->getStats());

    return scope.Close(String::New(stats.data(), stats.size()));
}

Handle<Value> PdalBindings::getSrs(const Arguments& args)
{
    HandleScope scope;
    PdalBindings* obj = ObjectWrap::Unwrap<PdalBindings>(args.This());

    const std::string wkt(obj->m_pdalSession->getSrs());

    return scope.Close(String::New(wkt.data(), wkt.size()));
}

Handle<Value> PdalBindings::read(const Arguments& args)
{
    HandleScope scope;
    PdalBindings* obj = ObjectWrap::Unwrap<PdalBindings>(args.This());

    // Call the factory to get the specialized 'read' command based on
    // the input args.  If there is an error with the input args, this call
    // will attempt to make an error callback (if a callback argument can be
    // identified) and return a null ptr.
    const std::string readId(generateReadId());

    ReadCommand* readCommand(
        ReadCommandFactory::create(
            obj->m_pdalSession,
            obj->m_itcBufferPool,
            readId,
            args));

    if (!readCommand)
    {
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

                readCommand->queryCallback()->Call(
                    Context::GetCurrent()->Global(), argc, argv);

                readCommand->dataCallback()->Call(
                    Context::GetCurrent()->Global(), argc, argv);

                delete readCommand;
                delete readReq;
                scope.Close(Undefined());
                return;
            }

            const std::string id(readCommand->readId());

            if (readCommand->rasterize())
            {
                ReadCommandRastered* readCommandRastered(
                        static_cast<ReadCommandRastered*>(readCommand));

                if (readCommandRastered)
                {
                    const RasterMeta rasterMeta(
                            readCommandRastered->rasterMeta());

                    const unsigned argc = 10;
                    Local<Value> argv[argc] =
                    {
                        Local<Value>::New(Null()), // err
                        Local<Value>::New(String::New(id.data(), id.size())),
                        Local<Value>::New(Integer::New(
                                    readCommand->numPoints())),
                        Local<Value>::New(Integer::New(
                                    readCommand->numBytes())),
                        Local<Value>::New(Number::New(
                                    rasterMeta.xBegin)),
                        Local<Value>::New(Number::New(
                                    rasterMeta.xStep)),
                        Local<Value>::New(Integer::New(
                                    rasterMeta.xNum())),
                        Local<Value>::New(Number::New(
                                    rasterMeta.yBegin)),
                        Local<Value>::New(Number::New(
                                    rasterMeta.yStep)),
                        Local<Value>::New(Integer::New(
                                    rasterMeta.yNum()))
                    };

                    readCommand->queryCallback()->Call(
                        Context::GetCurrent()->Global(), argc, argv);
                }
                else
                {
                    errorCallback(
                        readCommand->queryCallback(),
                        "Invalid ReadCommand");
                }
            }
            else
            {
                const unsigned argc = 4;
                Local<Value> argv[argc] =
                {
                    Local<Value>::New(Null()), // err
                    Local<Value>::New(String::New(id.data(), id.size())),
                    Local<Value>::New(Integer::New(readCommand->numPoints())),
                    Local<Value>::New(Integer::New(readCommand->numBytes()))
                };

                // Call the provided callback to return the status of the
                // data about to be streamed to the remote host.
                readCommand->queryCallback()->Call(
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

                        readCommand->dataCallback()->Call(
                            Context::GetCurrent()->Global(), argc, argv);
                    }
                    else
                    {
                        const unsigned argc = 1;
                        Local<Value> argv[argc] =
                            { readCommand->status.toObject() };

                        readCommand->dataCallback()->Call(
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
    PdalBindings::init(exports);
}

NODE_MODULE(pdalBindings, init)

