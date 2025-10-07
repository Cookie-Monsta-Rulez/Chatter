#pragma once
#include "ICommand.h"
#include <filesystem>

class ListCommand : public ICommand {
public:
    void execute(const std::vector<std::string>& args, ClientContext& ctx) override;
};