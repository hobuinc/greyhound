#pragma once

#include "commands/command.hpp"

namespace command
{

class Hierarchy : public Command
{
public:
    Hierarchy(const Args& args)
        : Command(args)
        , m_vertical(m_json["vertical"].asBool())
    { }

protected:
    virtual void work() override
    {
        const Json::Value result(
                m_session.hierarchy(
                    m_bounds.get(),
                    m_depthBegin,
                    m_depthEnd,
                    m_vertical,
                    m_scale.get(),
                    m_offset.get()));

        m_status.set(result);
    }

    bool m_vertical;
};

}

