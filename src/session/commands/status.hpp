#pragma once

#include <memory>
#include <string>
#include <vector>

#include "types/js.hpp"

class JsConvertible
{
public:
    ~JsConvertible() { }
    virtual Arg convert(v8::Isolate* isolate) const = 0;
    virtual Json::Value toJson() const { return Json::nullValue; }
};

class JsonConvertible : public JsConvertible
{
public:
    JsonConvertible() = default;
    JsonConvertible(const Json::Value& json) : m_json(json) { }

    virtual Arg convert(v8::Isolate* isolate) const override
    {
        return toJs(isolate, m_json);
    }

    virtual Json::Value toJson() const override
    {
        return m_json;
    }

private:
    const Json::Value m_json;
};

class BufferConvertible : public JsConvertible
{
public:
    BufferConvertible(const std::vector<char>& buffer) : m_buffer(buffer) { }
    virtual Arg convert(v8::Isolate* isolate) const override
    {
        return toJs(isolate, m_buffer);
    }

private:
    const std::vector<char>& m_buffer;
};

class Status
{
public:
    Status() : m_args{ std::make_shared<JsonConvertible>() } { }

    template<typename F>
    static Status safe(F f) noexcept
    {
        Status status;
        try { f(); }
        catch (std::exception& e) { status.setError(400, e.what()); }
        catch (...) { status.setError(500); }
        return status;
    }

    void set(const Json::Value& json)
    {
        m_args = {
            std::make_shared<JsonConvertible>(),
            std::make_shared<JsonConvertible>(json)
        };
    }

    void set(const std::vector<char>& buffer, bool done)
    {
        m_args = {
            std::make_shared<JsonConvertible>(),
            std::make_shared<BufferConvertible>(buffer),
            std::make_shared<JsonConvertible>(done)
        };
    }

    void setError(int code)
    {
        setError(code, "Unknown error");
    }

    void setError(int code, std::string message)
    {
        Json::Value err;
        err["code"] = code;
        err["message"] = message;
        m_args = { std::make_shared<JsonConvertible>(err) };
    }

    bool ok() const
    {
        return m_args.empty() || m_args.front()->toJson() == Json::nullValue;
    }

    Arg call(
            v8::Isolate* isolate,
            v8::UniquePersistent<v8::Function>& f) const
    {
        auto converted(toJs(isolate));
        v8::Local<v8::Function> local(v8::Local<v8::Function>::New(isolate, f));
        return local->Call(
                isolate->GetCurrentContext()->Global(),
                converted.size(),
                converted.data());
    }

    std::vector<Arg> toJs(v8::Isolate* isolate) const
    {
        std::vector<Arg> js;
        for (const auto& arg : m_args) js.push_back(arg->convert(isolate));
        return js;
    }

private:
    // We can't just store Arg values here, since our native values might be
    // defined within a different isolate context than the one at which we
    // actually invoke a callback.
    std::vector<std::shared_ptr<JsConvertible>> m_args;
};

