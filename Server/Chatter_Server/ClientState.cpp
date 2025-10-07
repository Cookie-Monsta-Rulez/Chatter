// ClientState.hpp
#pragma once
#include <vector>
#include <map>
#include <mutex>
#include <winsock2.h>
#include <openssl/ssl.h>

extern std::mutex clientsMutex;
extern std::vector<SOCKET> clientSockets;
extern std::map<SOCKET, std::string> clientUsernames;
extern std::map<SOCKET, SSL*> clientSSLs;