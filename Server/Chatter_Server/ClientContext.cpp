#include "ClientContext.h"
#include "Globals.h"  // For clientSSLs, etc.
#include <functional>


void ClientContext::sendMessage(const std::string& msg) {
    SSL_write(ssl, msg.c_str(), static_cast<int>(msg.length()));
}

void ClientContext::disconnect() {
    SSL_shutdown(ssl);
    SSL_free(ssl);
    closesocket(clientSocket);
}
