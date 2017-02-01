#pragma once

#include "commands/command.hpp"

namespace command
{

class Info : public Command
{
public:
    Info(const Args& args) : Command(args) { }

protected:
    virtual void work() override
    {
        m_status.set(m_session.info());
    }
};

}

