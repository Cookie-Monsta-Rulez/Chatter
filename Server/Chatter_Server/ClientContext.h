#pragma once
#include <winsock2.h>
#include <string>
#include <openssl/ssl.h>  // If you're using OpenSSL
#include <functional>


struct ClientContext {
    SOCKET clientSocket;
    std::string username;
    SSL* ssl;
    std::function<void(const std::string&)> broadcast;

    // Optional: utility methods
    void sendMessage(const std::string& msg);
    void disconnect();
};