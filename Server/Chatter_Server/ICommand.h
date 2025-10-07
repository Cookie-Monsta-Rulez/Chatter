#pragma once

#include <vector>
#include <string>
#include "ClientContext.h"
// Interface for all commands
class ICommand {
public:
    virtual ~ICommand() = default;
    virtual void execute(const std::vector<std::string>& args, ClientContext& ctx) = 0;
};