#pragma once

#include <memory>

#include <node.h>
#include <node_object_wrap.h>
#include <v8.h>

#include "types/js.hpp"

class Session;
class BufferPool;

class Bindings : public node::ObjectWrap
{
public:
    static void init(v8::Handle<v8::Object> exports);

    Session& session();

private:
    Bindings(std::string name);
    ~Bindings();

    static v8::Persistent<v8::Function> constructor;

    static void construct(const Args& args);

    static void global(const Args& args);

    static void create(const Args& args);
    static void info(const Args& args);
    static void read(const Args& args);
    static void hierarchy(const Args& args);
    static void files(const Args& args);

    std::unique_ptr<Session> m_session;
};

