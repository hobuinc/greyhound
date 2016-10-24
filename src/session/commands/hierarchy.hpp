#pragma once

#include <memory>

#include "commands/background.hpp"

#include <entwine/types/defs.hpp>

class Session;

class HierarchyCommand : public Background
{
public:
    HierarchyCommand(
            std::shared_ptr<Session> session,
            const entwine::Bounds& bounds,
            std::size_t depthBegin,
            std::size_t depthEnd,
            bool vertical,
            const entwine::Scale* scale,
            const entwine::Offset* offset,
            v8::UniquePersistent<v8::Function> cb);

    virtual ~HierarchyCommand();

    static HierarchyCommand* create(
            v8::Isolate* isolate,
            std::shared_ptr<Session> session,
            v8::Local<v8::Object> query,
            v8::UniquePersistent<v8::Function> cb);

    void run();
    std::string result() const { return m_result; }
    v8::UniquePersistent<v8::Function>& cb() { return m_cb; }

private:
    std::shared_ptr<Session> m_session;
    const entwine::Bounds m_bounds;
    const std::size_t m_depthBegin;
    const std::size_t m_depthEnd;
    const bool m_vertical;

    std::unique_ptr<entwine::Scale> m_scale;
    std::unique_ptr<entwine::Offset> m_offset;

    std::string m_result;

    v8::UniquePersistent<v8::Function> m_cb;
};

