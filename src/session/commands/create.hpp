#pragma once

#include "commands/command.hpp"

namespace command
{

class Create : public Command
{
public:
    Create(const Args& args) : Command(args) { }

protected:
    virtual void work() override
    {
        if (!m_session.initialize()) m_status.setError(404, "Not found");
    }
};

}

