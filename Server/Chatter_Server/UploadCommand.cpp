#include "UploadCommand.h"
#include <openssl/ssl.h>
#include <iostream>

void UploadCommand::execute(const std::vector<std::string>& args, ClientContext& ctx) {
    if (args.size() < 2) {
        std::string msg = "Usage: /upload <filename> <filesize>\n";
        SSL_write(ctx.ssl, msg.c_str(), static_cast<int>(msg.size()));
        return;
    }

    std::string filename = args[0];
    size_t filesize = std::stoull(args[1]);

    std::filesystem::create_directories("uploads");
    std::ofstream ofs("uploads/" + filename, std::ios::binary);
    if (!ofs) {
        std::string msg = "Server: Unable to create file.\n";
        SSL_write(ctx.ssl, msg.c_str(), static_cast<int>(msg.size()));
        return;
    }

    char buffer[4096];
    size_t received = 0;
    while (received < filesize) {
        int chunk = SSL_read(ctx.ssl, buffer, static_cast<int>(std::min<size_t>(sizeof(buffer), filesize - received)));
        if (chunk <= 0) break;
        ofs.write(buffer, chunk);
        received += chunk;
    }
    ofs.close();

    std::string notify = ctx.username + " uploaded file: " + filename + "\n";
    SSL_write(ctx.ssl, "Upload complete.\n", 17);
    ctx.broadcast(notify);
}