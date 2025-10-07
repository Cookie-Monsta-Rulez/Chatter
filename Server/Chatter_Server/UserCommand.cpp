#include "Commands.h"
#include "KickCommand.h"
#include "ICommand.h"

class KickCommand : public ICommand {
public:
    void execute(const std::vector<std::string>& args, ClientContext& ctx) override {
        if (args.empty()) {
            ctx.sendMessage("Usage: /kick <username>");
            return;
        }
        std::string target = args[0];
    }
};
