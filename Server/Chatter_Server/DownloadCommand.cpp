#include "DownloadCommand.h"
#include <fstream>
#include <filesystem>
#include <openssl/ssl.h>

void DownloadCommand::execute(const std::vector<std::string>& args, ClientContext& ctx) {
    if (args.empty()) {
        std::string msg = "Usage: /download <filename>\n";
        SSL_write(ctx.ssl, msg.c_str(), static_cast<int>(msg.size()));
        return;
    }

    std::string filename = args[0];
    std::string path = "uploads/" + filename;

    if (!std::filesystem::exists(path)) {
        std::string msg = "File not found.\n";
        SSL_write(ctx.ssl, msg.c_str(), static_cast<int>(msg.size()));
        return;
    }

    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) {
        std::string msg = "Failed to open file.\n";
        SSL_write(ctx.ssl, msg.c_str(), static_cast<int>(msg.size()));
        return;
    }

    std::vector<char> buffer((std::istreambuf_iterator<char>(ifs)), {});
    uint32_t filesize = static_cast<uint32_t>(buffer.size());

    // Send header
    std::string header = "/filedata " + filename + " " + std::to_string(filesize);
    SSL_write(ctx.ssl, header.c_str(), static_cast<int>(header.size()));

    // Send file data directly (no binary size needed)
    SSL_write(ctx.ssl, buffer.data(), filesize);
}