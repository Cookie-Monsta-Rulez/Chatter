#pragma once
#include "ICommand.h"
#include "Commands.h"
#include "ClientContext.h"

class KickCommand : public ICommand {
public:
    void execute(const std::vector<std::string>& args, ClientContext& ctx) override;
};

