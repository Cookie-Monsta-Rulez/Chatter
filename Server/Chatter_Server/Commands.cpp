// Commands.cpp
#include "Commands.h"
#include <sstream>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <thread> // Added for std::thread
#include "UploadCommand.h"
#include "DownloadCommand.h"
#include "HelpCommand.h"

void handleClient(SOCKET clientSocket, SSL_CTX* sslContext, const std::string& SecretToken);

static std::map<std::string, std::unique_ptr<ICommand>> commandRegistry;

namespace fs = std::filesystem;

// ------------ Command Parsing -------------
Command::Command(const std::string& input) : rawInput(input) {
    if (input.empty() || input[0] != '/') return;
    size_t spacePos = input.find(' ');
    commandName = (spacePos != std::string::npos) ? input.substr(1, spacePos - 1) : input.substr(1);
    commandArgument = (spacePos != std::string::npos) ? input.substr(spacePos + 1) : "";
}

std::string Command::getCommand() const {
    return commandName;
}

std::string Command::getArgument() const {
    return commandArgument;
}

bool Command::isValid() const {
    return !commandName.empty();
}


