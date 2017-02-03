#pragma once

#include <vector>

#include <entwine/types/schema.hpp>

#include "commands/command.hpp"
#include "read-queries/base.hpp"
#include "types/buffer-pool.hpp"

namespace command
{

class Read : public Loopable
{
public:
    Read(const Args& args)
        : Loopable(args)
        , m_compress(m_json["compress"].asBool())
        , m_filter(m_json["filter"])
        , m_schema(entwine::maybeCreate<entwine::Schema>(m_json["schema"]))
        , m_query(
                m_session.getQuery(
                    m_bounds.get(),
                    m_depthBegin,
                    m_depthEnd,
                    m_scale.get(),
                    m_offset.get(),
                    m_schema.get(),
                    m_filter,
                    m_compress))
    { }

protected:
    virtual void work() override
    {
        auto& bufferPool(ReadPool::get());
        std::vector<char>& buffer(bufferPool.acquire());
        m_query->read(buffer);

        bufferPool.capture(buffer);
        m_status.set(buffer, m_query->done());
    }

    virtual bool done() const override
    {
        return m_query->done() || Loopable::done();
    }

    bool m_compress;
    Json::Value m_filter;
    std::unique_ptr<entwine::Schema> m_schema;
    std::unique_ptr<ReadQuery> m_query;

    std::vector<char> m_buffer;
};

}

