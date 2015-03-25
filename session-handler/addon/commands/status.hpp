#pragma once

#include <node.h>

class Status
{
public:
    Status() : m_code(200), m_message() { }
    Status(int code, std::string& message) : m_code(code), m_message(message)
    { }

    void set(int code, const std::string& message)
    {
        m_code = code;
        m_message = message;
    }

    int code() const { return m_code; }

    bool ok() const { return m_code == 200; }

    v8::Local<v8::Value> toObject() const
    {
        if (m_code == 200)
        {
            return v8::Local<v8::Value>::New(v8::Null());
        }
        else
        {
            v8::Local<v8::Object> obj = v8::Object::New();
            obj->Set(
                    v8::String::NewSymbol("code"),
                    v8::Integer::New(m_code));
            obj->Set(
                    v8::String::NewSymbol("message"),
                    v8::String::New(m_message.data(), m_message.size()));

            return obj;
        }
    }

private:
    int m_code;
    std::string m_message;
};

