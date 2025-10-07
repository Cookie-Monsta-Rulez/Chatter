#pragma once
#include "ICommand.h"
#include <filesystem>
#include <fstream>  

class UploadCommand : public ICommand {
public:
    void execute(const std::vector<std::string>& args, ClientContext& ctx) override;
};
