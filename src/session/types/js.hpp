#pragma once

#include <json/json.h>

#include <node.h>
#include <node_buffer.h>
#include <v8.h>

#include <entwine/util/json.hpp>

#include "types/buffer-pool.hpp"

using Arg = v8::Local<v8::Value>;
using Args = v8::FunctionCallbackInfo<v8::Value>;

inline Arg toJs(v8::Isolate* isolate, const Json::Value& json)
{
    if (json.isNull()) return Arg::New(isolate, Null(isolate));
    if (json.isBool()) return v8::Boolean::New(isolate, json.asBool());
    if (json.isIntegral()) return v8::Number::New(isolate, json.asInt64());
    if (json.isDouble()) return v8::Number::New(isolate, json.asDouble());
    if (json.isString())
    {
        const std::string s(json.asString());
        return v8::String::NewFromUtf8(isolate, s.c_str());
    }
    if (json.isArray())
    {
        auto array(v8::Array::New(isolate, json.size()));
        for (Json::ArrayIndex i(0); i < json.size(); ++i)
        {
            array->Set(i, toJs(isolate, json[i]));
        }
        return array;
    }
    if (json.isObject())
    {
        auto object(v8::Object::New(isolate));
        for (const std::string key : json.getMemberNames())
        {
            object->Set(toJs(isolate, key), toJs(isolate, json[key]));
        }
        return object;
    }

    throw std::runtime_error("No conversion exists: " + json.toStyledString());
}

inline Arg toJs(v8::Isolate* isolate, const std::vector<char>& buffer)
{
    v8::MaybeLocal<v8::Object> nodeBuffer(
            node::Buffer::New(
                isolate,
                const_cast<char*>(buffer.data()),
                buffer.size(),
                [](char* pos, void* hint) { ReadPool::get().release(pos); },
                nullptr));

    return Arg::New(isolate, nodeBuffer.ToLocalChecked());
}

inline Json::Value toJson(v8::Isolate* isolate, const Arg& arg)
{
    if (arg->IsUndefined()) return Json::nullValue;
    if (arg->IsNull()) return Json::nullValue;
    if (arg->IsBoolean()) return arg->BooleanValue();
    if (arg->IsInt32()) return arg->Int32Value();
    if (arg->IsUint32()) return arg->Uint32Value();
    if (arg->IsNumber()) return arg->NumberValue();
    if (arg->IsString())
    {
        return std::string(*v8::String::Utf8Value(arg->ToString()));
    }

    if (arg->IsArray())
    {
        Json::Value json;
        v8::Array* array(v8::Array::Cast(*arg));
        json.resize(array->Length());
        for (Json::ArrayIndex i(0); i < array->Length(); ++i)
        {
            json[i] = toJson(isolate, array->Get(v8::Integer::New(isolate, i)));
        }
        return json;
    }
    if (arg->IsObject())
    {
        Json::Value json;
        v8::Local<v8::Object> object(arg->ToObject());

        const std::vector<std::string> keys(
                entwine::extract<std::string>(
                    toJson(isolate, object->GetOwnPropertyNames())));

        for (const auto& key : keys)
        {
            json[key] = toJson(isolate, object->Get(toJs(isolate, key)));
        }
        return json;
    }

    throw std::runtime_error("No JSON conversion exists");
}

inline v8::UniquePersistent<v8::Function> toFunction(
        v8::Isolate* isolate,
        const Arg& arg)
{
    return v8::UniquePersistent<v8::Function>(
            isolate,
            v8::Local<v8::Function>::Cast(arg));
}

inline v8::UniquePersistent<v8::Function> getCallback(const Args& args)
{
    return toFunction(args.GetIsolate(), args[args.Length() - 1]);
}

