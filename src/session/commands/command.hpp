#pragma once

#include <memory>
#include <type_traits>

#include <node.h>
#include <node_object_wrap.h>
#include <uv.h>
#include <v8.h>

#include <entwine/util/json.hpp>
#include <entwine/reader/query.hpp>

#include "bindings.hpp"
#include "session.hpp"
#include "commands/status.hpp"
#include "types/js.hpp"

class Command
{
    friend class Commander;

public:
    Command(const Args& args)
        : m_args(args)
        , m_isolate(m_args.GetIsolate())
        , m_scope(m_isolate)
        , m_cb(getCallback(args))
        , m_bindings(*node::ObjectWrap::Unwrap<Bindings>(args.Holder()))
        , m_session(m_bindings.session())
        , m_json(args.Length() > 1 ?
                toJson(m_isolate, args[0]) : Json::nullValue)
        , m_bounds(entwine::maybeCreate<entwine::Bounds>(m_json["bounds"]))
        , m_scale(entwine::maybeCreate<entwine::Scale>(m_json["scale"]))
        , m_offset(entwine::maybeCreate<entwine::Offset>(m_json["offset"]))
        , m_depthBegin(
                m_json.isMember("depth") ?
                    m_json["depth"].asUInt64() :
                    m_json["depthBegin"].asUInt64())
        , m_depthEnd(
                m_json.isMember("depth") ?
                    m_json["depth"].asUInt64() + 1 :
                    m_json["depthEnd"].asUInt64())
    {
        if (m_json.isMember("depth"))
        {
            if (m_json.isMember("depthBegin") || m_json.isMember("depthEnd"))
            {
                throw std::runtime_error("Invalid depth specification");
            }
        }
    }

    virtual ~Command() { }

protected:
    virtual void work() = 0;

    virtual void run() noexcept
    {
        const Status current(Status::safe([this]() { work(); }));
        if (!current.ok()) m_status = current;
    }

    Status& status() { return m_status; }
    v8::UniquePersistent<v8::Function>& cb() { return m_cb; }
    v8::Isolate* isolate() { return m_isolate; }

    const Args& m_args;
    v8::Isolate* m_isolate;
    v8::HandleScope m_scope;
    v8::UniquePersistent<v8::Function> m_cb;

    Bindings& m_bindings;
    Session& m_session;

    Status m_status;
    const Json::Value m_json;

    // These are pretty common across multiple commands, so they'll be
    // extracted here if they exist in the query.
    std::unique_ptr<entwine::Bounds> m_bounds;
    std::unique_ptr<entwine::Scale> m_scale;
    std::unique_ptr<entwine::Offset> m_offset;
    std::size_t m_depthBegin = 0;
    std::size_t m_depthEnd = 0;
};

class Loopable : public Command
{
    friend class Commander;

public:
    Loopable(const Args& args)
        : Command(args)
    { }

protected:
    virtual bool done() const = 0;

    void send(uv_async_t* async)
    {
        m_wait = true;
        uv_async_send(async);
        std::unique_lock<std::mutex> lock(m_mutex);
        m_cv.wait(lock, [this]()->bool { return !m_wait; });
    }

    void sent()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_wait = false;
        lock.unlock();
        m_cv.notify_all();
    }

    std::mutex m_mutex;
    std::condition_variable m_cv;
    bool m_wait = false;
};

class Commander
{
public:
    template<typename T> static void run(const Args& args)
    {
        static_assert(
                std::is_base_of<Command, T>::value,
                "Commander::run requires a Command type");

        std::unique_ptr<Command> command(createSafe<T>(args));
        if (!command) return;

        queue(
                std::move(command),
                (uv_work_cb)([](uv_work_t* req) noexcept
                {
                    static_cast<Command*>(req->data)->run();
                }),
                (uv_after_work_cb)([](uv_work_t* req, int status)
                {
                    v8::Isolate* isolate(v8::Isolate::GetCurrent());
                    v8::HandleScope scope(isolate);

                    std::unique_ptr<uv_work_t> work(req);
                    std::unique_ptr<Command> command(
                            static_cast<Command*>(req->data));

                    command->status().call(isolate, command->cb());
                }));
    }

    template<typename T> static void loop(const Args& args)
    {
        static_assert(
                std::is_base_of<Loopable, T>::value,
                "Commander::loop requires a Loopable type");

        std::unique_ptr<Loopable> loopable(createSafe<T>(args));
        if (!loopable) return;

        auto work = (uv_work_cb)[](uv_work_t* req) noexcept
        {
            Loopable* loopable(static_cast<Loopable*>(req->data));

            UniqueAsync async(new uv_async_t());
            async->data = loopable;

            uv_async_init(uv_default_loop(), async.get(), [](uv_async_t* async)
            {
                v8::Isolate* isolate(v8::Isolate::GetCurrent());
                v8::HandleScope scope(isolate);

                Loopable* loopable(static_cast<Loopable*>(async->data));
                loopable->status().call(isolate, loopable->cb());
                loopable->sent();
            });

            while (loopable->status().ok() && !loopable->done())
            {
                loopable->run();
                loopable->send(async.get());
            }
        };

        queue(
                std::move(loopable),
                work,
                (uv_after_work_cb)([](uv_work_t* req, int status)
                {
                    v8::Isolate* isolate(v8::Isolate::GetCurrent());
                    v8::HandleScope scope(isolate);

                    std::unique_ptr<uv_work_t> work(req);
                    std::unique_ptr<Loopable> loopable(
                            static_cast<Loopable*>(req->data));

                    // When looping, only send the status if it's bad.
                    // Otherwise we've already been streaming binary data.
                    if (!loopable->status().ok())
                    {
                        loopable->status().call(isolate, loopable->cb());
                    }
                }));
    }

private:
    template<typename T, typename Work, typename Done>
    static void queue(std::unique_ptr<T> command, Work work, Done done)
    {
        std::unique_ptr<uv_work_t> req(entwine::makeUnique<uv_work_t>());
        req->data = command.release();

        uv_queue_work(uv_default_loop(), req.release(), work, done);
    }

    template<typename T>
    static std::unique_ptr<T> createSafe(const Args& args)
    {
        std::unique_ptr<T> t;

        const Status status(Status::safe([&t, &args]()
        {
            t = entwine::makeUnique<T>(args);
        }));

        if (status.ok()) return t;
        else
        {
            auto cb(getCallback(args));
            status.call(cb);
            return std::unique_ptr<T>();
        }
    }

    struct AsyncDeleter
    {
        void operator()(uv_async_t* async)
        {
            auto handle(reinterpret_cast<uv_handle_t*>(async));
            uv_close(handle, (uv_close_cb)([](uv_handle_t* handle)
            {
                delete handle;
            }));
        }
    };

    using UniqueAsync = std::unique_ptr<uv_async_t, AsyncDeleter>;
};

