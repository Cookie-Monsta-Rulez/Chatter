#include "Commands.h"
#include "KickCommand.h"
#include <unordered_map>
#include <memory>
#include <mutex>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <thread>
#include <map>
#include <vector>
#include <mutex>
#include <string>
#include <algorithm>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <windows.h>
#include <bcrypt.h>
#include <sstream>
#include "Globals.h"
#include "ClientState.hpp"
#include "ClientContext.h"
#include "HelpCommand.h"
#include "UploadCommand.h"
#include "DownloadCommand.h"
#include "ListCommand.h"

extern std::map<SOCKET, SSL*> clientSSLs;

std::unordered_map<std::string, std::unique_ptr<ICommand>> commandRegistry;

static std::mutex registryMutex;

void registerCommands() {
    std::lock_guard<std::mutex> lock(registryMutex);
    commandRegistry["kick"] = std::make_unique<KickCommand>();
    commandRegistry["help"] = std::make_unique<HelpCommand>();
    commandRegistry["upload"] = std::make_unique<UploadCommand>();
    commandRegistry["download"] = std::make_unique<DownloadCommand>();
    commandRegistry["list"] = std::make_unique<ListCommand>();
}

void dispatchCommand(const std::string& rawInput, ClientContext& ctx) {
    std::istringstream iss(rawInput);
    std::string cmdName;
    iss >> cmdName;
    cmdName = cmdName.substr(1); // remove '/'

    std::vector<std::string> args;
    std::string arg;
    while (iss >> arg) args.push_back(arg);

    std::lock_guard<std::mutex> lock(registryMutex);
    auto it = commandRegistry.find(cmdName);
    if (it != commandRegistry.end()) {
        ctx.ssl = clientSSLs[ctx.clientSocket];  // If not already set
        it->second->execute(args, ctx);
    }

    else {
        SSL* ssl = clientSSLs[ctx.clientSocket];
        std::string error = "Unknown command: " + cmdName + "\n";
        SSL_write(ssl, error.c_str(), static_cast<int>(error.length()));
    }
}