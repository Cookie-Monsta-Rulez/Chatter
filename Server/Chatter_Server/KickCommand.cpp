#include "KickCommand.h"
#include "ClientState.hpp"
#include <iostream>
#include <algorithm>
#include <mutex>
#include <winsock2.h>
#include <openssl/ssl.h>
#include "Globals.h"  // For clientSSLs, clientUsernames, etc.
#include "ClientContext.h"
#include <functional>

extern SOCKET serverSocket;
extern std::mutex clientsMutex;
extern std::map<SOCKET, SSL*> clientSSLs;
extern std::map<SOCKET, std::string> clientUsernames;
extern std::vector<SOCKET> clientSockets;

void KickCommand::execute(const std::vector<std::string>& args, ClientContext& ctx) {
    if (args.empty()) {
        ctx.sendMessage("Usage: /kick <username>");
        return;
    }

    std::string targetUsername = args[0];
    SOCKET targetSocket = INVALID_SOCKET;

    {
        std::lock_guard<std::mutex> lock(clientsMutex);

        // Find the socket associated with the username
        for (const auto& [sock, name] : clientUsernames) {
            if (name == targetUsername) {
                targetSocket = sock;
                break;
            }
        }

        if (targetSocket == INVALID_SOCKET) {
            ctx.sendMessage("User not found.");
            return;
        }

        if (targetSocket == serverSocket) {
            std::cerr << "Attempted to kick the server socket—aborting kick.\n";
            ctx.sendMessage("Cannot kick the server socket.");
            return;
        }

        std::cout << "Kicking socket: " << targetSocket << " (username: " << targetUsername << ")\n";

        // Remove from shared state BEFORE closing
        clientSSLs.erase(targetSocket);
        clientUsernames.erase(targetSocket);
        clientSockets.erase(
            std::remove(clientSockets.begin(), clientSockets.end(), targetSocket),
            clientSockets.end()
        );
    }

    // Notify and clean up
    SSL* targetSSL = nullptr;
    {
        std::lock_guard<std::mutex> lock(clientsMutex);
        if (clientSSLs.count(targetSocket)) {
            targetSSL = clientSSLs[targetSocket];
        }
    }

    if (targetSSL) {
        SSL_write(targetSSL, "You have been kicked.\n", 23);
        SSL_shutdown(targetSSL);
        SSL_free(targetSSL);
    }

    closesocket(targetSocket);

    ctx.sendMessage("User kicked successfully.");
    ctx.broadcast(targetUsername + " was kicked from the chat.");
}
