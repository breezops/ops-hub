#pragma once
#include <filesystem>
#include <string>
#include <vector>
#include "version.h"

namespace mw2 {
inline constexpr wchar_t Site[] = L"https://ops-hub.breezops.chatgpt.site/#mw2-project";
inline constexpr wchar_t Source[] = L"https://github.com/breezops/ops-hub/tree/feature/mw2-specops4-launcher/games/mw2";
struct Diagnostics {
    bool found = false;
    bool supported = false;
    bool componentPassed = false;
    std::wstring summary;
    std::wstring report;
};
Diagnostics inspect(const std::filesystem::path& folder, bool componentTests);
std::vector<std::filesystem::path> discover();
std::filesystem::path savedFolder();
void saveFolder(const std::filesystem::path& folder);
void saveReport(const std::filesystem::path& destination, const std::wstring& text);
}
