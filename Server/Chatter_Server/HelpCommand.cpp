#include "HelpCommand.h"
#include "ClientContext.h"
#include "CommandManager.h"
#include <sstream>
#include <openssl/ssl.h>

// Forward declaration of registry
extern std::unordered_map<std::string, std::unique_ptr<ICommand>> commandRegistry;

void HelpCommand::execute(const std::vector<std::string>&, ClientContext& ctx) {
    std::ostringstream oss;
    oss << "Available commands:\n";

    for (const auto& [name, _] : commandRegistry) {
        oss << "  /" << name << "\n";  // You can add usage hints here later
    }

    std::string helpText = oss.str();
    SSL_write(ctx.ssl, helpText.c_str(), static_cast<int>(helpText.length()));
}
