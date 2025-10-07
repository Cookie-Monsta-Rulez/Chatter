#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <thread>
#include <map>
#include <vector>
#include <mutex>
#include <string>
#include <algorithm>
#include <fstream>
#include <filesystem>
#include <openssl/ssl.h>
#include <openssl/err.h>

#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "libssl.lib")
#pragma comment(lib, "libcrypto.lib")

#include "Commands.h"
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <mutex>

// Forward declarations or includes for your command classes
#include "HelpCommand.h"
#include "KickCommand.h"
#include "ClientContext.h"

namespace fs = std::filesystem;

// Globals
std::mutex clientsMutex;
std::vector<SOCKET> clientSockets;
std::map<SOCKET, std::string> clientUsernames;
std::map<SOCKET, SSL*> clientSSLs;
std::vector<std::string> uploadedFiles; // List of files for GUI notifications

#include "Globals.h"
SOCKET serverSocket;

void broadcastMessage(const std::string& message, SOCKET senderSocket) {
    std::lock_guard<std::mutex> lock(clientsMutex);
    for (SOCKET client : clientSockets) {
        if (client != senderSocket && clientSSLs[client]) {
            SSL_write(clientSSLs[client], message.c_str(), (int)message.length());
        }
    }
}

void handleClient(SOCKET clientSocket, SSL_CTX* sslContext, const std::string& SecretToken) {
    SSL* ssl = SSL_new(sslContext);
    SSL_set_fd(ssl, static_cast<int>(clientSocket));
    if (SSL_accept(ssl) <= 0) {
        ERR_print_errors_fp(stderr);
        SSL_free(ssl);
        closesocket(clientSocket);
        return;
    }

    // Read secret token
    char tokenBuf[64] = {};
    int bytesRead = SSL_read(ssl, tokenBuf, sizeof(tokenBuf));
    std::string receivedSecret(tokenBuf, bytesRead);
    if (receivedSecret != SecretToken) {
        std::cerr << "Unauthorized client.\n";
        SSL_shutdown(ssl);
        SSL_free(ssl);
        closesocket(clientSocket);
        return;
    }

    // Read username
    char userBuf[64] = {};
    bytesRead = SSL_read(ssl, userBuf, sizeof(userBuf));
    std::string username(userBuf, bytesRead);

    {
        std::lock_guard<std::mutex> lock(clientsMutex);
        clientSockets.push_back(clientSocket);
        clientUsernames[clientSocket] = username;
        clientSSLs[clientSocket] = ssl;
    }

    std::cout << username << " joined.\n";
    broadcastMessage(username + " has joined the chat.\n", clientSocket);

    // Add this at the top of handleClient, after reading username:
    ClientContext ctx{clientSocket, username, ssl, [&](const std::string& msg) {
        broadcastMessage(msg, clientSocket);
    }};

    // Ensure uploads folder exists
    fs::create_directory("uploads");

    char buffer[4096] = {};
    while (true) {
        int bytesReceived = SSL_read(ssl, buffer, sizeof(buffer));
        if (bytesReceived <= 0) break;

        std::string message(buffer, bytesReceived);

        if (!message.empty() && message[0] == '/') {
            dispatchCommand(message, ctx);
            continue;
        }
        // Add this:
        broadcastMessage(message, clientSocket);
    }
    std::cout << username << " disconnected.\n";
    broadcastMessage(username + " has left the chat.\n", clientSocket);

    {
        std::lock_guard<std::mutex> lock(clientsMutex);
        clientUsernames.erase(clientSocket);
        clientSSLs.erase(clientSocket);
        clientSockets.erase(std::remove(clientSockets.begin(), clientSockets.end(), clientSocket), clientSockets.end());
    }

    SSL_shutdown(ssl);
    SSL_free(ssl);
    closesocket(clientSocket);
}

int main() {
    registerCommands();
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed\n";
        return 1;
    }

    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();

    SSL_CTX* ctx = SSL_CTX_new(TLS_server_method());
    if (!ctx) { std::cerr << "Failed SSL_CTX\n"; return 1; }

    if (SSL_CTX_use_certificate_file(ctx, "server.crt", SSL_FILETYPE_PEM) <= 0 ||
        SSL_CTX_use_PrivateKey_file(ctx, "server.key", SSL_FILETYPE_PEM) <= 0) {
        std::cerr << "Failed to load cert/key\n";
        ERR_print_errors_fp(stderr);
        return 1;
    }

    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == INVALID_SOCKET) { std::cerr << "Socket failed\n"; return 1; }

    std::string address;
    std::cout << "Server IP to listen on: ";
    std::getline(std::cin, address);

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(8443);
    inet_pton(AF_INET, address.c_str(), &serverAddr.sin_addr);

    if (bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "Bind failed\n";
        closesocket(serverSocket);
        return 1;
    }

    listen(serverSocket, SOMAXCONN);
    std::cout << "Secure chat server started on port 8443...\n";

    std::string SecretToken;
    std::cout << "Enter server password or type 'generate' for random: ";
    std::getline(std::cin, SecretToken);
    if (SecretToken == "generate") {
        SecretToken = "AutoGeneratedSecret123!"; // Replace with secure generator if needed
    }
    std::cout << "Secret token: " << SecretToken << "\n";

    fs::create_directory("uploads"); // ensure upload folder exists

    while (true) {
        SOCKET clientSocket = accept(serverSocket, nullptr, nullptr);
        if (clientSocket == INVALID_SOCKET) { std::cerr << "Accept failed\n"; continue; }

        std::thread t(handleClient, clientSocket, ctx, SecretToken);
        t.detach();
    }

    closesocket(serverSocket);
    SSL_CTX_free(ctx);
    WSACleanup();
    return 0;
}
