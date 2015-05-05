#pragma once

#include <memory>

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

    static v8::Handle<v8::Value> construct(const v8::Arguments& args);

    static v8::Handle<v8::Value> create(const v8::Arguments& args);
    static v8::Handle<v8::Value> destroy(const v8::Arguments& args);
    static v8::Handle<v8::Value> getNumPoints(const v8::Arguments& args);
    static v8::Handle<v8::Value> getSchema(const v8::Arguments& args);
    static v8::Handle<v8::Value> getStats(const v8::Arguments& args);
    static v8::Handle<v8::Value> getSrs(const v8::Arguments& args);
    static v8::Handle<v8::Value> getBounds(const v8::Arguments& args);
    static v8::Handle<v8::Value> read(const v8::Arguments& args);

    std::shared_ptr<Session> m_session;
    ItcBufferPool& m_itcBufferPool;
};

