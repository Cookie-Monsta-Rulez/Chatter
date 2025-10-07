#pragma once
#include "ICommand.h"

class DownloadCommand : public ICommand {
public:
    void execute(const std::vector<std::string>& args, ClientContext& ctx) override;
};
