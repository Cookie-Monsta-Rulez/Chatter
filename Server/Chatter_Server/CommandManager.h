#pragma once
#include <string>
#include "ClientContext.h"

void dispatchCommand(const std::string& command, ClientContext& ctx);
void registerCommands();
