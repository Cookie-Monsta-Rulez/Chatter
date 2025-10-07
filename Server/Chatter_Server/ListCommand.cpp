#include "ListCommand.h"
#include <sstream>
#include <openssl/ssl.h>

void ListCommand::execute(const std::vector<std::string>&, ClientContext& ctx) {
    namespace fs = std::filesystem;
    std::ostringstream oss;
    oss << "/filelist";

    for (const auto& entry : fs::directory_iterator("uploads")) {
        if (entry.is_regular_file()) {
            oss << " " << entry.path().filename().string();
        }
    }

    std::string fileList = oss.str();
    SSL_write(ctx.ssl, fileList.c_str(), static_cast<int>(fileList.size()));
}