#include <node_buffer.h>
#include <curl/curl.h>

#include "pdal-bindings.hpp"
#include "read-command.hpp"
#include "once.hpp"

using namespace v8;

namespace
{
    // TODO Configure.
    const std::size_t numBuffers = 1024;
    const std::size_t maxReadLength = 65536;
    static ItcBufferPool itcBufferPool(numBuffers, maxReadLength);

    const std::size_t readIdSize = 24;
    const std::string hexValues = "0123456789ABCDEF";

    Once curlOnce([]()->void {
        std::cout << "Destructing global Curl environment" << std::endl;
        curl_global_cleanup();
    });

    bool isInteger(const Value& value)
    {
        return value.IsInt32() || value.IsUint32();
    }

    bool isDouble(const Value& value)
    {
        return value.IsNumber() && !isInteger(value);
    }

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

    S3Info parseS3Info(const v8::Local<v8::Value>& rawArg)
    {
        S3Info info;
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

                return S3Info(url, bucket, access, hidden);
            }
        }

        return info;
    }

    void safe(std::string& err, std::function<void()> f)
    {
        try
        {
            f();
        }
        catch (const std::runtime_error& e)
        {
            err = e.what();
        }
        catch (const std::bad_alloc& ba)
        {
            err = "Caught bad alloc";
        }
        catch (...)
        {
            err = "Unknown error";
        }
    }
}

Persistent<Function> PdalBindings::constructor;

PdalBindings::PdalBindings()
    : m_pdalSession(new PdalSession())
    , m_itcBufferPool(itcBufferPool)
{
    curlOnce.ensure([]()->void {
        std::cout << "Initializing global Curl environment" << std::endl;
        curl_global_init(CURL_GLOBAL_ALL);
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
    tpl->PrototypeTemplate()->Set(String::NewSymbol("parse"),
        FunctionTemplate::New(parse)->GetFunction());
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
    tpl->PrototypeTemplate()->Set(String::NewSymbol("getFills"),
        FunctionTemplate::New(getFills)->GetFunction());
    tpl->PrototypeTemplate()->Set(String::NewSymbol("read"),
        FunctionTemplate::New(read)->GetFunction());
    tpl->PrototypeTemplate()->Set(String::NewSymbol("serialize"),
        FunctionTemplate::New(serialize)->GetFunction());

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

void PdalBindings::doInitialize(
        const Arguments& args,
        const bool execute)
{
    HandleScope scope;

    std::string errMsg("");

    if (args[0]->IsUndefined() || !args[0]->IsString())
        errMsg = "'pipelineId' must be a string - args[0]";

    if (args[1]->IsUndefined() || !args[1]->IsString())
        errMsg = "'pipeline' must be a string - args[1]";

    if (args[2]->IsUndefined() || !args[2]->IsBoolean())
        errMsg = "'serialCompress' must be boolean - args[2]";

    if (args[5]->IsUndefined() || !args[5]->IsFunction())
        throw std::runtime_error("Invalid callback supplied to 'create'");

    Persistent<Function> callback(
            Persistent<Function>::New(Local<Function>::Cast(
                    args[5])));

    if (errMsg.size())
    {
        errorCallback(callback, errMsg);
        scope.Close(Undefined());
        return;
    }

    const std::string pipelineId(*v8::String::Utf8Value(args[0]->ToString()));
    const std::string pipeline  (*v8::String::Utf8Value(args[1]->ToString()));
    const bool serialCompress   (args[2]->BooleanValue());
    const S3Info s3Info(parseS3Info(args[3]));
    const std::vector<std::string> diskPaths(parsePathList(args[4]));

    const SerialPaths serialPaths(s3Info, diskPaths);

    PdalBindings* obj = ObjectWrap::Unwrap<PdalBindings>(args.This());

    // Store everything we'll need to perform initialization.
    uv_work_t* req(new uv_work_t);
    req->data = new CreateData(
            obj->m_pdalSession,
            pipelineId,
            pipeline,
            serialCompress,
            serialPaths,
            execute,
            callback);

    uv_queue_work(
        uv_default_loop(),
        req,
        (uv_work_cb)([](uv_work_t *req)->void {
            CreateData* createData(static_cast<CreateData*>(req->data));

            safe(createData->errMsg, [createData]()->void {
                createData->pdalSession->initialize(
                    createData->pipelineId,
                    createData->pipeline,
                    createData->serialCompress,
                    createData->serialPaths,
                    createData->execute);
            });
        }),
        (uv_after_work_cb)([](uv_work_t* req, int status)->void {
            HandleScope scope;

            CreateData* createData(static_cast<CreateData*>(req->data));

            // Output args.
            const unsigned argc = 1;
            Local<Value> argv[argc] =
                {
                    Local<Value>::New(String::New(
                            createData->errMsg.data(),
                            createData->errMsg.size()))
                };

            createData->callback->Call(
                Context::GetCurrent()->Global(), argc, argv);

            delete createData;
            delete req;
        })
    );

    scope.Close(Undefined());
}

Handle<Value> PdalBindings::create(const Arguments& args)
{
    HandleScope scope;
    PdalBindings* obj = ObjectWrap::Unwrap<PdalBindings>(args.This());
    obj->doInitialize(args, true);
    return scope.Close(Undefined());
}

Handle<Value> PdalBindings::parse(const Arguments& args)
{
    HandleScope scope;
    PdalBindings* obj = ObjectWrap::Unwrap<PdalBindings>(args.This());
    obj->doInitialize(args, false);

    // Release this session from memory now - we will need to reset the
    // session anyway in order to use it after this.
    obj->m_pdalSession.reset();

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

    const std::string schema(obj->m_pdalSession->getSchema());

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

Handle<Value> PdalBindings::getFills(const Arguments& args)
{
    HandleScope scope;
    PdalBindings* obj = ObjectWrap::Unwrap<PdalBindings>(args.This());

    const std::vector<std::size_t> fills(obj->m_pdalSession->getFills());

    Local<Array> jsFills = Array::New(fills.size());

    for (std::size_t i(0); i < fills.size(); ++i)
    {
        jsFills->Set(i, Integer::New(fills[i]));
    }

    return scope.Close(jsFills);
}

Handle<Value> PdalBindings::serialize(const Arguments& args)
{
    HandleScope scope;

    std::string errMsg("");

    if (args[2]->IsUndefined() || !args[2]->IsFunction())
        throw std::runtime_error("Invalid callback supplied to 'serialize'");

    Persistent<Function> callback(
            Persistent<Function>::New(Local<Function>::Cast(
                    args[2])));

    if (errMsg.size())
    {
        std::cout << "Erroring SERIALIZE" << std::endl;
        errorCallback(callback, errMsg);
        return scope.Close(Undefined());
    }

    const S3Info s3Info(parseS3Info(args[0]));
    const std::vector<std::string> diskPaths(parsePathList(args[1]));

    SerialPaths paths(s3Info, diskPaths);

    PdalBindings* obj = ObjectWrap::Unwrap<PdalBindings>(args.This());

    // Store everything we'll need to perform initialization.
    uv_work_t* req(new uv_work_t);
    req->data = new SerializeData(obj->m_pdalSession, paths, callback);

    std::cout << "Starting serialization task" << std::endl;

    uv_queue_work(
        uv_default_loop(),
        req,
        (uv_work_cb)([](uv_work_t *req)->void {
            SerializeData* serializeData(
                reinterpret_cast<SerializeData*>(req->data));

            safe(serializeData->errMsg, [serializeData]()->void {
                serializeData->pdalSession->serialize(serializeData->paths);
            });
        }),
        (uv_after_work_cb)([](uv_work_t* req, int status)->void {
            HandleScope scope;

            SerializeData* serializeData(
                reinterpret_cast<SerializeData*>(req->data));

            // Output args.
            const unsigned argc = 1;
            Local<Value> argv[argc] =
                {
                    Local<Value>::New(String::New(
                            serializeData->errMsg.data(),
                            serializeData->errMsg.size()))
                };

            serializeData->callback->Call(
                Context::GetCurrent()->Global(), argc, argv);

            delete serializeData;
            delete req;
        })
    );

    return scope.Close(Undefined());
}

Handle<Value> PdalBindings::read(const Arguments& args)
{
    HandleScope scope;

    // Provide access to m_pdalSession from within this static function.
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
        (uv_work_cb)([](uv_work_t* readReq)->void {
            ReadCommand* readCommand(
                static_cast<ReadCommand*>(readReq->data));

            safe(readCommand->errMsg(), [readCommand]()->void {
                // Run the query.  This will ensure indexing if needed, and
                // will obtain everything needed to start streaming binary
                // data to the client.
                readCommand->run();
            });
        }),
        (uv_after_work_cb)([](uv_work_t* readReq, int status)->void {
            ReadCommand* readCommand(
                static_cast<ReadCommand*>(readReq->data));

            if (readCommand->errMsg().size())
            {
                std::cout << "Got error callback from run()" << std::endl;
                errorCallback(
                    readCommand->queryCallback(),
                    readCommand->errMsg());

                delete readCommand;
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
                ([](uv_async_t* async, int status)->void {
                    HandleScope scope;

                    ReadCommand* readCommand(
                        static_cast<ReadCommand*>(async->data));

                    const unsigned argc = 3;
                    Local<Value>argv[argc] =
                    {
                        Local<Value>::New(Null()), // err
                        Local<Value>::New(node::Buffer::New(
                                reinterpret_cast<const char*>(
                                    readCommand->getBuffer()->data()),
                                readCommand->getBuffer()->size())->handle_),
                        Local<Value>::New(Number::New(readCommand->done()))
                    };

                    readCommand->dataCallback()->Call(
                        Context::GetCurrent()->Global(), argc, argv);

                    readCommand->getBuffer()->flush();
                    scope.Close(Undefined());
                })
            );

            uv_queue_work(
                uv_default_loop(),
                dataReq,
                (uv_work_cb)([](uv_work_t* dataReq)->void {
                    ReadCommand* readCommand(
                        static_cast<ReadCommand*>(dataReq->data));

                    safe(readCommand->errMsg(), [readCommand]()->void {
                        readCommand->acquire();

                        do
                        {
                            readCommand->getBuffer()->grab();
                            readCommand->read(maxReadLength);

                            // A bit strange, but we need to send this via the
                            // same async token use in the uv_async_init()
                            // call, so it must be a member of ReadCommand
                            // since that object is the only thing we can
                            // access in this background work queue.
                            readCommand->async()->data = readCommand;
                            uv_async_send(readCommand->async());
                        } while (!readCommand->done());

                        readCommand->getBufferPool().release(
                            readCommand->getBuffer());
                    });
                }),
                (uv_after_work_cb)([](uv_work_t* dataReq, int status)->void {
                    ReadCommand* readCommand(
                        static_cast<ReadCommand*>(dataReq->data));

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

