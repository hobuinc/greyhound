#pragma once

#include "commands/command.hpp"

namespace command
{

class Files : public Command
{
public:
    Files(const Args& args)
        : Command(args)
    {
        m_search = entwine::maybeCreate<Json::Value>(m_json["search"]);

        if (m_search && m_bounds)
        {
            throw std::runtime_error("Both 'search' and 'bounds' specified");
        }
        else if (!m_search && !m_bounds)
        {
            throw std::runtime_error("Empty query");
        }
    }

protected:
    virtual void work() override
    {
        if (m_search)
        {
            m_status.set(m_session.files(*m_search));
        }
        else
        {
            m_status.set(
                m_session.files(*m_bounds, m_scale.get(), m_offset.get()));
        }
    }

    std::unique_ptr<Json::Value> m_search;
};

}

