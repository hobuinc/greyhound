#include "bindings.hpp"

#include <thread>
#include <sstream>

#include <execinfo.h>
#include <unistd.h>

#include <curl/curl.h>
#include <json/json.h>

#include <pdal/PointLayout.hpp>

#include <entwine/reader/cache.hpp>
#include <entwine/reader/reader.hpp>
#include <entwine/third/arbiter/arbiter.hpp>
#include <entwine/types/dim-info.hpp>
#include <entwine/types/outer-scope.hpp>
#include <entwine/types/point.hpp>
#include <entwine/types/schema.hpp>
#include <entwine/util/json.hpp>
#include <entwine/util/unique.hpp>

#include "session.hpp"
#include "commands/create.hpp"
#include "commands/info.hpp"
#include "commands/files.hpp"
#include "commands/hierarchy.hpp"
#include "commands/read.hpp"

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

    std::once_flag globalInitOnce;

    std::vector<std::string> paths;
    entwine::OuterScope outerScope;
    std::unique_ptr<entwine::Cache> cache;
}

struct CRYPTO_dynlock_value
{
    std::mutex mutex;
};

Persistent<Function> Bindings::constructor;

Bindings::Bindings(std::string name)
    : m_session(entwine::makeUnique<Session>(name, paths, outerScope, *cache))
{ }

Bindings::~Bindings()
{ }

void Bindings::init(v8::Handle<v8::Object> exports)
{
    Isolate* isolate(Isolate::GetCurrent());

    // Prepare constructor template
    Local<FunctionTemplate> tpl(v8::FunctionTemplate::New(isolate, construct));
    tpl->SetClassName(String::NewFromUtf8(isolate, "Session"));
    tpl->InstanceTemplate()->SetInternalFieldCount(1);

    NODE_SET_METHOD(exports, "global", global);

    NODE_SET_PROTOTYPE_METHOD(tpl, "construct", construct);
    NODE_SET_PROTOTYPE_METHOD(tpl, "create",    create);
    NODE_SET_PROTOTYPE_METHOD(tpl, "info",      info);
    NODE_SET_PROTOTYPE_METHOD(tpl, "files",     files);
    NODE_SET_PROTOTYPE_METHOD(tpl, "read",      read);
    NODE_SET_PROTOTYPE_METHOD(tpl, "hierarchy", hierarchy);

    constructor.Reset(isolate, tpl->GetFunction());
    exports->Set(String::NewFromUtf8(isolate, "Session"), tpl->GetFunction());
}

void Bindings::construct(const Args& args)
{
    Isolate* isolate(args.GetIsolate());
    HandleScope scope(isolate);

    if (args.IsConstructCall())
    {
        // Invoked as constructor with 'new'.
        const std::string name(*v8::String::Utf8Value(args[0]->ToString()));
        Bindings* obj = new Bindings(name);
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

void Bindings::global(const Args& args)
{
    std::call_once(globalInitOnce, [&args]()
    {
        Isolate* isolate(args.GetIsolate());
        HandleScope scope(isolate);

        if (args.Length() != 3)
        {
            throw std::runtime_error("Wrong number of arguments to global");
        }

        std::size_t i(0);
        const auto& pathsArg(args[i++]);
        const auto& cacheSizeArg(args[i++]);
        const auto& arbiterArg(args[i++]);

        paths = entwine::extract<std::string>(toJson(isolate, pathsArg));

        const std::size_t cacheSize(toJson(isolate, cacheSizeArg).asUInt64());
        cache = entwine::makeUnique<entwine::Cache>(cacheSize);

        outerScope.getArbiter(toJson(isolate, arbiterArg));

        signal(SIGSEGV, handler);
        signal(SIGBUS, handler);
        curl_global_init(CURL_GLOBAL_ALL);
    });
}

void Bindings::create(const Args& args)
{
    Commander::run<command::Create>(args);
}

void Bindings::info(const Args& args)
{
    Commander::run<command::Info>(args);
}

void Bindings::files(const Args& args)
{
    Commander::run<command::Files>(args);
}

void Bindings::hierarchy(const Args& args)
{
    Commander::run<command::Hierarchy>(args);
}

void Bindings::read(const Args& args)
{
    Commander::loop<command::Read>(args);
}

Session& Bindings::session() { return *m_session; }

//////////////////////////////////////////////////////////////////////////////

void init(Handle<Object> exports)
{
    Bindings::init(exports);
}

NODE_MODULE(session, init)

