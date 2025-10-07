#pragma once
#include "ICommand.h"
#include "Commands.h"
#include <string>
#include <vector>

class HelpCommand : public ICommand {
public:
    void execute(const std::vector<std::string>& args, ClientContext& ctx) override;
};
