#include <node.h>
#include <v8.h>

#include "pdal-bindings.hpp"
#include "read-command.hpp"

using namespace v8;

namespace
{
    const std::size_t chunkSize = 65536;

    bool isInteger(const Value& value)
    {
        return value.IsInt32() || value.IsUint32();
    }

    bool isDouble(const Value& value)
    {
        return value.IsNumber() && !isInteger(value);
    }
}

Persistent<Function> PdalBindings::constructor;

PdalBindings::PdalBindings()
    : m_pdalSession()
    , m_readCommand()
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
    tpl->PrototypeTemplate()->Set(String::NewSymbol("getDimensions"),
        FunctionTemplate::New(getDimensions)->GetFunction());
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

    // Perform PdalSession construction here.  This way we can use a single
    // PdalBindings object to validate multiple pipelines.
    obj->m_pdalSession.reset(new PdalSession());

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

Handle<Value> PdalBindings::getDimensions(const Arguments& args)
{
    HandleScope scope;
    PdalBindings* obj = ObjectWrap::Unwrap<PdalBindings>(args.This());

    const std::string dimensions(obj->m_pdalSession->getDimensions());

    return scope.Close(String::New(dimensions.data(), dimensions.size()));
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
    obj->m_readCommand = ReadCommandFactory::create(args, obj->m_pdalSession);

    if (!obj->m_readCommand)
    {
        return scope.Close(Undefined());
    }

    // Store our read command where our worker functions can access it.
    uv_work_t* readReq(new uv_work_t);
    readReq->data = obj->m_readCommand;

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
                delete readCommand;
                return;
            }

            HandleScope scope;

            const unsigned argc = 3;
            Local<Value> argv[argc] =
                {
                    Local<Value>::New(Null()), // err
                    Local<Value>::New(Integer::New(readCommand->numPoints())),
                    Local<Value>::New(Integer::New(readCommand->numBytes()))
                };

            // Call the provided callback to return the status of the
            // data about to be streamed to the remote host.
            readCommand->callback()->Call(
                Context::GetCurrent()->Global(), argc, argv);

            // Dispose of the persistent handle so this callback may be
            // garbage collected.
            readCommand->callback().Dispose();

            // Create a token for the actual data transmission portion of
            // the read.
            uv_work_t* sendReq(new uv_work_t);
            sendReq->data = readCommand;

            // Now stream all the buffered point data to the remote host
            // asynchronously.
            uv_queue_work(
                uv_default_loop(),
                sendReq,
                (uv_work_cb)([](uv_work_t *sendReq)->void {
                    ReadCommand* readCommand(
                        reinterpret_cast<ReadCommand*>(
                                sendReq->data));

                    try
                    {
                        const std::size_t numBytes(readCommand->numBytes());
                        std::size_t offset(0);

                        while (offset < numBytes && !readCommand->cancel())
                        {
                            readCommand->transmit(
                                offset,
                                std::min(chunkSize, numBytes - offset));

                            offset += chunkSize;
                        }

                        if (readCommand->cancel())
                        {
                            std::cout <<
                                "Cancelled at (" <<
                                offset <<
                                " / " <<
                                numBytes <<
                                ") bytes" <<
                                std::endl;
                        }
                    }
                    catch (...)
                    {
                        std::cout <<
                            "Caught error transmitting buffer" <<
                            std::endl;
                    }
                }),
                (uv_after_work_cb)([](uv_work_t* sendReq, int status)->void {
                    ReadCommand* readCommand(
                        reinterpret_cast<ReadCommand*>(sendReq->data));

                    // Read and data transmission complete.  Clean everything
                    // up - the bufferTransmitter may no longer be used.
                    delete readCommand;
                    delete sendReq;
                })
            );

            delete readReq;
        })
    );

    return scope.Close(Undefined());
}

Handle<Value> PdalBindings::cancel(const Arguments& args)
{
    HandleScope scope;
    PdalBindings* obj = ObjectWrap::Unwrap<PdalBindings>(args.This());

    bool cancelled(false);

    // TODO Race condition.  Mutex here with the deletes in read().
    if (obj->m_readCommand)
    {
        obj->m_readCommand->cancel(true);
        cancelled = true;
        std::cout << "Cancelling..." << std::endl;
    }

    return scope.Close(Boolean::New(cancelled));
}

//////////////////////////////////////////////////////////////////////////////

void init(Handle<Object> exports)
{
    PdalBindings::init(exports);
}

NODE_MODULE(pdalBindings, init)

