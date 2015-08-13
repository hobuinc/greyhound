#pragma once

#include <memory>

#include <node.h>
#include <node_object_wrap.h>
#include <v8.h>

#include "commands/read.hpp"

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

    typedef v8::FunctionCallbackInfo<v8::Value> Arguments;

private:
    Bindings();
    ~Bindings();

    static v8::Persistent<v8::Function> constructor;

    static void construct(const Arguments& args);

    static void create(const Arguments& args);
    static void destroy(const Arguments& args);
    static void getNumPoints(const Arguments& args);
    static void getSchema(const Arguments& args);
    static void getStats(const Arguments& args);
    static void getSrs(const Arguments& args);
    static void getBounds(const Arguments& args);
    static void getType(const Arguments& args);
    static void read(const Arguments& args);

    std::shared_ptr<Session> m_session;
    ItcBufferPool& m_itcBufferPool;
};

