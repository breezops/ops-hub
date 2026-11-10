#pragma once
#include <iomanip>
#include <cctype>
#include <sstream>
#include <string>

namespace coop {
// Local pipe only. No filesystem paths, shell commands or arbitrary engine calls.
struct LauncherRequest {
    std::string action, value;
    static bool Parse(const std::string& text, LauncherRequest& result) {
        if (text.empty() || text.size() > 512 || text.find('\0') != std::string::npos) return false;
        LauncherRequest request;
        std::istringstream in(text);
        if (!(in >> request.action)) return false;
        if (request.action == "join" || request.action == "preview_join") {
            if (!(in >> std::quoted(request.value)) || request.value.empty() || request.value.size() > 200) return false;
            for (unsigned char c : request.value)
                if (!(std::isalnum(c) || c == '.' || c == '-' || c == '_' || c == ':' || c == '[' || c == ']')) return false;
        } else if (request.action != "status" && request.action != "host" && request.action != "leave" &&
                   request.action != "ready" && request.action != "countdown" && request.action != "replay" &&
                   request.action != "preview_status" && request.action != "preview_host" &&
                   request.action != "preview_leave" && request.action != "preview_ready" &&
                   request.action != "preview_start" && request.action != "preview_lobby" &&
                   request.action != "preview_reconnect") return false;
        in >> std::ws;
        if (!in.eof()) return false;
        result = request;
        return true;
    }
};
}
