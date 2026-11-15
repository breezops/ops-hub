#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <regex>
#include <algorithm>
#include <cstring>
#include "diagnostics.h"
#include "../../core/frame_codec.h"

namespace mw2 {
namespace fs = std::filesystem;
namespace {
constexpr char Hash[] = "2f8c9cfda1b8c0fb308fd9719577154e8222fc7654f5e8f1bf54579bca69d805";
constexpr wchar_t SettingsKey[] = L"Software\\BreezOps\\MW2SpecOps4";
std::wstring regString(HKEY root, const wchar_t* key, const wchar_t* value, DWORD flags = 0) {
    DWORD size = 0;
    if (RegGetValueW(root, key, value, RRF_RT_REG_SZ | flags, nullptr, nullptr, &size) != ERROR_SUCCESS || size > 65536) return {};
    std::vector<wchar_t> out(size / sizeof(wchar_t) + 1, 0);
    if (RegGetValueW(root, key, value, RRF_RT_REG_SZ | flags, nullptr, out.data(), &size) != ERROR_SUCCESS) return {};
    return out.data();
}
std::string sha256(const std::vector<std::uint8_t>& bytes) {
    BCRYPT_ALG_HANDLE alg = nullptr;
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0) throw std::runtime_error("SHA-256 provider unavailable");
    unsigned char digest[32];
    auto status = BCryptHash(alg, nullptr, 0, const_cast<PUCHAR>(bytes.data()), static_cast<ULONG>(bytes.size()), digest, sizeof(digest));
    BCryptCloseAlgorithmProvider(alg, 0);
    if (status < 0) throw std::runtime_error("SHA-256 failed");
    std::ostringstream out;
    for (auto b : digest) out << std::hex << std::setw(2) << std::setfill('0') << unsigned(b);
    return out.str();
}
}

fs::path savedFolder() { return regString(HKEY_CURRENT_USER, SettingsKey, L"GameFolder"); }
void saveFolder(const fs::path& path) {
    const auto value = path.wstring();
    RegSetKeyValueW(HKEY_CURRENT_USER, SettingsKey, L"GameFolder", REG_SZ, value.c_str(), static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t)));
}
void saveReport(const fs::path& destination, const std::wstring& text) {
    const int count = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    std::string utf8(count, '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), utf8.data(), count, nullptr, nullptr);
    std::ofstream out(destination, std::ios::binary);
    out.write(utf8.data(), static_cast<std::streamsize>(utf8.size()));
    if (!out) throw std::runtime_error("Cannot write report");
}

std::vector<fs::path> discover() {
    std::vector<fs::path> result;
    auto add = [&](const fs::path& p) {
        if (p.empty()) return;
        std::error_code ec;
        if (fs::is_regular_file(p / L"iw4sp.exe", ec) && std::find(result.begin(), result.end(), p) == result.end()) result.push_back(p);
    };
    add(savedFolder());
    std::error_code ec;
    add(fs::current_path(ec));
    wchar_t module[32768];
    if (GetModuleFileNameW(nullptr, module, 32768)) {
        auto p = fs::path(module).parent_path();
        add(p); add(p.parent_path());
    }
    std::vector<fs::path> roots;
    for (auto root : {regString(HKEY_CURRENT_USER, L"Software\\Valve\\Steam", L"SteamPath"),
                      regString(HKEY_LOCAL_MACHINE, L"Software\\Valve\\Steam", L"InstallPath", RRF_SUBKEY_WOW6432KEY)}) {
        if (!root.empty()) roots.emplace_back(root);
    }
    for (const auto& steam : roots) {
        add(steam / L"steamapps/common/Call of Duty Modern Warfare 2");
        std::ifstream input(steam / L"steamapps/libraryfolders.vdf", std::ios::binary);
        if (!input) continue;
        std::string vdf((std::istreambuf_iterator<char>(input)), {});
        const std::regex pattern("\\\"path\\\"\\s+\\\"([^\\\"]+)\\\"");
        for (auto m = std::sregex_iterator(vdf.begin(), vdf.end(), pattern); m != std::sregex_iterator(); ++m) {
            std::string path = (*m)[1].str();
            for (std::size_t pos = 0; (pos = path.find("\\\\", pos)) != std::string::npos; ++pos) path.replace(pos, 2, "\\");
            const int count = MultiByteToWideChar(CP_UTF8, 0, path.data(), static_cast<int>(path.size()), nullptr, 0);
            std::wstring wide(count, 0);
            MultiByteToWideChar(CP_UTF8, 0, path.data(), static_cast<int>(path.size()), wide.data(), count);
            add(fs::path(wide) / L"steamapps/common/Call of Duty Modern Warfare 2");
        }
    }
    return result;
}

Diagnostics inspect(const fs::path& folder, bool componentTests) {
    Diagnostics result;
    std::wostringstream report;
    report << L"OPS HUB / MW2 SPEC OPS 4\r\nLauncher " << Version
           << L"\r\nMOD STATUS: IN DEVELOPMENT - FOUR-PLAYER MODE UNAVAILABLE\r\n\r\nGame folder: " << folder.wstring() << L"\r\n";
    try {
        auto exe = folder / L"iw4sp.exe";
        std::error_code ec;
        result.found = fs::is_regular_file(exe, ec);
        if (!result.found) throw std::runtime_error("iw4sp.exe not found. Choose the installed MW2 (2009) folder.");
        auto size = fs::file_size(exe);
        if (size == 0 || size > 64 * 1024 * 1024) throw std::runtime_error("Executable size is outside the supported range.");
        std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
        std::ifstream file(exe, std::ios::binary);
        if (!file.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()))) throw std::runtime_error("Cannot read executable. Check file permissions or finish the download.");
        const auto digest = sha256(bytes);
        report << L"SHA-256: " << std::wstring(digest.begin(), digest.end()) << L"\r\n";
        result.supported = digest == Hash;
        if (!result.supported) throw std::runtime_error("Different executable build. Four-player research checks are disabled for this version.");
        report << L"PASS: Supported Steam x64 executable, build 25374375.\r\n";
        result.summary = L"Supported Steam build detected";
        if (componentTests) {
            const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(bytes.data());
            const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(bytes.data() + dos->e_lfanew);
            std::vector<std::uint8_t> image(nt->OptionalHeader.SizeOfImage, 0);
            std::memcpy(image.data(), bytes.data(), nt->OptionalHeader.SizeOfHeaders);
            const auto* sections = IMAGE_FIRST_SECTION(nt);
            for (unsigned i = 0; i < nt->FileHeader.NumberOfSections; ++i) {
                const auto& s = sections[i];
                std::memcpy(image.data() + s.VirtualAddress, bytes.data() + s.PointerToRawData, s.SizeOfRawData);
            }
            const auto original = image;
            int passed = 0;
            auto check = [&](bool condition, const wchar_t* name) {
                report << (condition ? L"PASS: " : L"FAIL: ") << name << L"\r\n";
                if (condition) ++passed;
            };
            image[0x2305c7] ^= 1;
            auto before = image;
            auto test = specops4::patch_frame_codec(image.data(), image.size());
            check(!test.ok && test.failed_rva == 0x2305c7 && image == before, L"Signature mismatch leaves all bytes untouched");
            image = original;
            check(!specops4::patch_frame_codec(image.data(), 0x2305c7).ok && image == original, L"Truncated image refused");
            check(!specops4::patch_frame_codec(nullptr, 0).ok, L"Null image refused");
            check(specops4::patch_frame_codec(image.data(), image.size()).ok, L"Both frame-codec instruction signatures match");
            std::vector<std::size_t> changed;
            for (std::size_t i = 0; i < image.size(); ++i) if (image[i] != original[i]) changed.push_back(i);
            check(changed == std::vector<std::size_t>{0x22f1d3, 0x2305c9, 0x2305d5}, L"Only three expected private-memory bytes change");
            before = image;
            check(!specops4::patch_frame_codec(image.data(), image.size()).ok && image == before, L"Repeat application refused");
            result.componentPassed = passed == 6;
            result.summary = result.componentPassed ? L"7 / 7 checks passed (including build identity)" : L"Component checks failed";
        } else report << L"Component checks: not run. Select Run component checks.\r\n";
    } catch (const std::exception& e) {
        std::string error = e.what();
        result.summary.assign(error.begin(), error.end());
        report << L"CHECK: " << result.summary << L"\r\n";
    }
    report << L"\r\nThese checks inspect a private memory copy only. They do not test multiplayer.\r\nNo game file has been patched. No DLL installed. No gameplay connection is started by these checks.\r\nThe launcher lobby is separate from the game. Original Spec Ops still supports up to two players.\r\n";
    result.report = report.str();
    return result;
}
}
