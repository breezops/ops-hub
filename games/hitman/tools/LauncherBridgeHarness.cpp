#include "LauncherBridge.h"
#include "Log.h"
#include <fstream>

namespace CoopLog {
void Write(coop::LogLevel, std::string_view, std::string_view) {}
}
int main(int argc, char** argv) {
    if (argc != 2) return 1;
    std::ofstream(argv[1]) << GetCurrentProcessId();
    LauncherBridge bridge;
    const auto start = GetTickCount64();
    while (GetTickCount64() - start < 45000) {
        bridge.Poll(double(GetTickCount64() - start) / 1000.0, [](const coop::LauncherRequest& request) {
            return "{\"ok\":true,\"version\":\"0.13.94-four-player.preview.3\",\"bridge_version\":1,\"action\":\"" + request.action + "\"}";
        });
        Sleep(5);
    }
}
