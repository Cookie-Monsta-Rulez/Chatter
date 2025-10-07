// Commands.h
#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>
#include "ClientContext.h"

// Represents a parsed command input
class Command {
private:
    std::string rawInput;
    std::string commandName;
    std::string commandArgument;

public:
    Command(const std::string& input);
    std::string getCommand() const;
    std::string getArgument() const;
    bool isValid() const;
};

// Register all available commands
void registerCommands();

// Dispatch a command based on user input
void dispatchCommand(const std::string& input, ClientContext& ctx);
