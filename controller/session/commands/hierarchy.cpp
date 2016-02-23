#include "commands/hierarchy.hpp"

#include <iostream>

#include "session.hpp"

using namespace v8;

HierarchyCommand::HierarchyCommand(
        std::shared_ptr<Session> session,
        const entwine::BBox& bbox,
        std::size_t depthBegin,
        std::size_t depthEnd,
        v8::UniquePersistent<v8::Function> cb)
    : m_session(session)
    , m_bbox(bbox)
    , m_depthBegin(depthBegin)
    , m_depthEnd(depthEnd)
    , m_cb(std::move(cb))
{ }

HierarchyCommand::~HierarchyCommand()
{ }

void HierarchyCommand::run()
{
    m_result = m_session->hierarchy(m_bbox, m_depthBegin, m_depthEnd);
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
    const auto bboxSymbol(toSymbol(isolate, "bounds"));

    if (
            query->HasOwnProperty(depthBeginSymbol) &&
            query->HasOwnProperty(depthEndSymbol) &&
            query->HasOwnProperty(bboxSymbol))
    {
        const std::size_t depthBegin(
                query->Get(depthBeginSymbol)->Uint32Value());
        const std::size_t depthEnd(
                query->Get(depthEndSymbol)->Uint32Value());

        const entwine::BBox bbox(parseBBox(query->Get(bboxSymbol)));

        if (bbox.exists())
        {
            command = new HierarchyCommand(
                    session,
                    bbox,
                    depthBegin,
                    depthEnd,
                    std::move(cb));
        }
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

