#include <node.h>
#include <node_buffer.h>
#include <v8.h>

#include "pdal-bindings.hpp"
#include "read-command.hpp"

using namespace v8;

namespace
{
    const std::size_t chunkSize = 65536;
    const std::size_t readIdSize = 24;
    const std::string hexValues = "0123456789ABCDEF";

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
}

Persistent<Function> PdalBindings::constructor;

PdalBindings::PdalBindings()
    : m_pdalSession(new PdalSession())
    , m_readCommands()
{ }

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
    tpl->PrototypeTemplate()->Set(String::NewSymbol("cancel"),
        FunctionTemplate::New(cancel)->GetFunction());
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

void PdalBindings::doInitialize(
        const Arguments& args,
        const bool execute)
{
    HandleScope scope;

    std::string errMsg("");

    if (args[0]->IsUndefined() || !args[0]->IsString())
        errMsg = "'pipeline' must be a string - args[0]";
    if (args[1]->IsUndefined() || !args[1]->IsFunction())
        // Fatal.
        throw std::runtime_error("Invalid callback supplied to 'create'");

    Persistent<Function> callback(
            Persistent<Function>::New(Local<Function>::Cast(args[1])));

    if (errMsg.size())
    {
        errorCallback(callback, errMsg);
        scope.Close(Undefined());
        return;
    }

    const std::string pipeline(*v8::String::Utf8Value(args[0]->ToString()));

    PdalBindings* obj = ObjectWrap::Unwrap<PdalBindings>(args.This());

    // Store everything we'll need to perform initialization.
    uv_work_t* req(new uv_work_t);
    req->data = new CreateData(
            obj->m_pdalSession,
            pipeline,
            execute,
            callback);

    uv_queue_work(
        uv_default_loop(),
        req,
        (uv_work_cb)([](uv_work_t *req)->void {
            CreateData* createData(reinterpret_cast<CreateData*>(req->data));

            try
            {
                createData->pdalSession->initialize(
                    createData->pipeline,
                    createData->execute);
            }
            catch (const std::runtime_error& e)
            {
                createData->errMsg = e.what();
            }
            catch (const std::bad_alloc& ba)
            {
                createData->errMsg = "Memory allocation failed in CREATE";
            }
            catch (...)
            {
                createData->errMsg = "Unknown error";
            }
        }),
        (uv_after_work_cb)([](uv_work_t* req, int status)->void {
            HandleScope scope;

            CreateData* createData(reinterpret_cast<CreateData*>(req->data));

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

            // Dispose of the persistent handle so the callback may be
            // garbage collected.
            createData->callback.Dispose();

            delete createData;
            delete req;
        }));

    scope.Close(Undefined());
}

Handle<Value> PdalBindings::create(const Arguments& args)
{
    HandleScope scope;
    PdalBindings* obj = ObjectWrap::Unwrap<PdalBindings>(args.This());
    obj->doInitialize(args);
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
            obj->m_readCommands,
            readId,
            args));

    if (!readCommand)
    {
        return scope.Close(Undefined());
    }

    obj->m_readCommands[readId] = readCommand;

    // Store our read command where our worker functions can access it.
    uv_work_t* readReq(new uv_work_t);
    readReq->data = readCommand;

    // Read points asynchronously.
    uv_queue_work(
        uv_default_loop(),
        readReq,
        (uv_work_cb)([](uv_work_t *readReq)->void {
            ReadCommand* readCommand(
                reinterpret_cast<ReadCommand*>(readReq->data));

            // Buffer the point data from PDAL.  If any exceptions occur,
            // catch/store them so they can be passed to the callback.
            try
            {
                readCommand->run();
            }
            catch (const std::runtime_error& e)
            {
                readCommand->errMsg(e.what());
            }
            catch (...)
            {
                readCommand->errMsg("Unknown error");
            }
        }),
        (uv_after_work_cb)([](uv_work_t* readReq, int status)->void {
            ReadCommand* readCommand(
                reinterpret_cast<ReadCommand*>(readReq->data));

            if (readCommand->errMsg().size())
            {
                // Propagate the error back to the remote host.  This call
                // will dispose of the callback.
                errorCallback(
                    readCommand->callback(),
                    readCommand->errMsg());

                // Clean up since we won't be calling the async send code.
                readCommand->eraseSelf();
                delete readCommand;
                return;
            }

            HandleScope scope;

            const std::string id(readCommand->readId());

            if (readCommand->rasterize())
            {
                ReadCommandRastered* readCommandRastered(
                        reinterpret_cast<ReadCommandRastered*>(readCommand));

                if (readCommandRastered)
                {
                    const RasterMeta rasterMeta(
                            readCommandRastered->rasterMeta());

                    const unsigned argc = 11;
                    Local<Value> argv[argc] =
                    {
                        Local<Value>::New(Null()), // err
                        Local<Value>::New(String::New(id.data(), id.size())),
                        Local<Value>::New(Integer::New(
                                    readCommand->numPoints())),
                        Local<Value>::New(Integer::New(
                                    readCommand->numBytes())),
                        // TODO This makes a copy.  Should use 4-arg version.
                        // https://groups.google.com/forum/#!msg/nodejs/gz8YF3oLit0/dDN8RAB22RAJ
                        Local<Value>::New(node::Buffer::New(
                                    reinterpret_cast<const char*>(
                                        readCommand->data()),
                                    readCommand->numBytes())->handle_),
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

                    readCommand->callback()->Call(
                        Context::GetCurrent()->Global(), argc, argv);
                }
                else
                {
                    errorCallback(
                        readCommand->callback(),
                        "Invalid ReadCommand");
                }
            }
            else
            {
                const unsigned argc = 5;
                Local<Value> argv[argc] =
                {
                    Local<Value>::New(Null()), // err
                    Local<Value>::New(String::New(id.data(), id.size())),
                    Local<Value>::New(Integer::New(readCommand->numPoints())),
                    Local<Value>::New(Integer::New(readCommand->numBytes())),
                    Local<Value>::New(node::Buffer::New(
                                reinterpret_cast<const char*>(
                                    readCommand->data()),
                                readCommand->numBytes())->handle_)
                };

                // Call the provided callback to return the status of the
                // data about to be streamed to the remote host.
                readCommand->callback()->Call(
                    Context::GetCurrent()->Global(), argc, argv);
            }

            // Dispose of the persistent handle so this callback may be
            // garbage collected.
            readCommand->callback().Dispose();

            delete readReq;
        })
    );

    return scope.Close(Undefined());
}

Handle<Value> PdalBindings::cancel(const Arguments& args)
{
    HandleScope scope;
    bool cancelled(false);

    if (!args[0]->IsUndefined() && args[0]->IsString())
    {
        const std::string readId(*v8::String::Utf8Value(args[0]->ToString()));

        PdalBindings* obj = ObjectWrap::Unwrap<PdalBindings>(args.This());

        auto it(obj->m_readCommands.find(readId));

        // TODO Race condition.  Mutex here with the deletes in read().
        if (it != obj->m_readCommands.end())
        {
            it->second->cancel(true);
            cancelled = true;
            std::cout << "Cancelling..." << std::endl;
        }
    }

    return scope.Close(Boolean::New(cancelled));
}

//////////////////////////////////////////////////////////////////////////////

void init(Handle<Object> exports)
{
    PdalBindings::init(exports);
}

NODE_MODULE(pdalBindings, init)

