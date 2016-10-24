#include "commands/hierarchy.hpp"

#include <iostream>

#include "session.hpp"

#include <entwine/util/json.hpp>
#include <entwine/util/unique.hpp>

using namespace v8;

HierarchyCommand::HierarchyCommand(
        std::shared_ptr<Session> session,
        const entwine::Bounds& bounds,
        std::size_t depthBegin,
        std::size_t depthEnd,
        bool vertical,
        const entwine::Scale* scale,
        const entwine::Offset* offset,
        v8::UniquePersistent<v8::Function> cb)
    : m_session(session)
    , m_bounds(bounds)
    , m_depthBegin(depthBegin)
    , m_depthEnd(depthEnd)
    , m_vertical(vertical)
    , m_scale(entwine::maybeClone(scale))
    , m_offset(entwine::maybeClone(offset))
    , m_result()
    , m_cb(std::move(cb))
{ }

HierarchyCommand::~HierarchyCommand()
{ }

void HierarchyCommand::run()
{
    m_result = m_session->hierarchy(
            m_bounds,
            m_depthBegin,
            m_depthEnd,
            m_vertical,
            m_scale.get(),
            m_offset.get());
}

HierarchyCommand* HierarchyCommand::create(
        v8::Isolate* isolate,
        std::shared_ptr<Session> session,
        v8::Local<v8::Object> query,
        v8::UniquePersistent<v8::Function> cb)
{
    HierarchyCommand* command(nullptr);

    const auto depthBeginSymbol(toSymbol(isolate, "depthBegin"));
    const auto depthEndSymbol(toSymbol(isolate, "depthEnd"));
    const auto boundsSymbol(toSymbol(isolate, "bounds"));
    const auto verticalSymbol(toSymbol(isolate, "vertical"));
    const auto scaleSymbol(toSymbol(isolate, "scale"));
    const auto offsetSymbol(toSymbol(isolate, "offset"));

    if (
            query->HasOwnProperty(depthBeginSymbol) &&
            query->HasOwnProperty(depthEndSymbol) &&
            query->HasOwnProperty(boundsSymbol))
    {
        const std::size_t depthBegin(
                query->Get(depthBeginSymbol)->Uint32Value());
        const std::size_t depthEnd(
                query->Get(depthEndSymbol)->Uint32Value());

        const entwine::Bounds bounds(parseBounds(query->Get(boundsSymbol)));

        const bool vertical(
                query->HasOwnProperty(verticalSymbol) &&
                query->Get(verticalSymbol)->BooleanValue());

        std::unique_ptr<entwine::Scale> scale;
        std::unique_ptr<entwine::Offset> offset;

        if (query->HasOwnProperty(scaleSymbol))
        {
            scale = entwine::makeUnique<entwine::Scale>(
                    entwine::parse(
                        *v8::String::Utf8Value(
                            query->Get(scaleSymbol)->ToString())));
        }

        if (query->HasOwnProperty(offsetSymbol))
        {
            offset = entwine::makeUnique<entwine::Offset>(
                    entwine::parse(
                        *v8::String::Utf8Value(
                            query->Get(offsetSymbol)->ToString())));
        }

        if (bounds.exists())
        {
            command = new HierarchyCommand(
                    session,
                    bounds,
                    depthBegin,
                    depthEnd,
                    vertical,
                    scale.get(),
                    offset.get(),
                    std::move(cb));
        }
        else std::cout << "No bounds" << std::endl;
    }

    if (!command)
    {
        std::cout << "Bad hierarchy command" << std::endl;
        Status status(400, std::string("Invalid hierarchy query parameters"));
        const unsigned argc = 1;
        Local<Value> argv[argc] = { status.toObject(isolate) };

        Local<Function> local(Local<Function>::New(isolate, cb));
        local->Call(isolate->GetCurrentContext()->Global(), argc, argv);
    }

    return command;
}

