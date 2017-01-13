#pragma once

#include <memory>

#include <node.h>
#include <node_object_wrap.h>
#include <v8.h>

class Session;
class ItcBufferPool;

struct CRYPTO_dynlock_value
{
    std::mutex mutex;
};

class Bindings : public node::ObjectWrap
{
public:
    static void init(v8::Handle<v8::Object> exports);

private:
    Bindings();
    ~Bindings();

    static v8::Persistent<v8::Function> constructor;

    using Args = v8::FunctionCallbackInfo<v8::Value>;

    static void construct(const Args& args);

    static void global(const Args& args);

    static void create(const Args& args);
    static void destroy(const Args& args);
    static void info(const Args& args);
    static void read(const Args& args);
    static void hierarchy(const Args& args);

    std::shared_ptr<Session> m_session;
    ItcBufferPool& m_itcBufferPool;
};

