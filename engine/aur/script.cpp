#include "script.hpp"
#include "game.hpp"

namespace aur {

void Script::update(double dt) {
    if (done) return;
    if (wait) {
        if (!wait->update(dt)) return;
        wait.reset();
    }
    // Simple command processor stub for Phase 3
    if (ip >= cmds.size()) {
        done = true;
        return;
    }
    const auto& c = cmds[ip++];
    std::string cmd = c["cmd"].as_string();
    if (cmd == "wait") {
        double secs = c.contains("seconds") ? c["seconds"].as_number() : 0.5;
        wait = std::make_unique<TimerWait>(secs);
    } else if (cmd == "end") {
        done = true;
    }
}

std::vector<std::string> validate_script(const JsonArray& cmds, const std::string& path) {
    std::vector<std::string> errs;
    return errs;
}

} // namespace aur
