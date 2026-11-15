// Local MW2 x64 test loader. Forwards every WinMM import used by iw4sp.exe.
// This stage validates the build and logs; it does not modify game memory.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>
#include <cstdint>
#include <cwchar>
#include <memory>
#include <initializer_list>
#include "winmm_names.h"

#pragma comment(lib, "bcrypt.lib")

namespace {
constexpr std::uint8_t SupportedHash[32] = {
    0x2f, 0x8c, 0x9c, 0xfd, 0xa1, 0xb8, 0xc0, 0xfb,
    0x30, 0x8f, 0xd9, 0x71, 0x95, 0x77, 0x15, 0x4e,
    0x82, 0x22, 0xfc, 0x76, 0x54, 0xf5, 0xe8, 0xf1,
    0xbf, 0x54, 0x57, 0x9b, 0xca, 0x69, 0xd8, 0x05,
};

void log_line(const wchar_t* line) {
    wchar_t directory[MAX_PATH]{};
    const DWORD n = GetEnvironmentVariableW(L"LOCALAPPDATA", directory, MAX_PATH);
    if (!n || n >= MAX_PATH - 30) return;
    wcscat_s(directory, L"\\OpsHub");
    CreateDirectoryW(directory, nullptr);
    wcscat_s(directory, L"\\SpecOps4");
    CreateDirectoryW(directory, nullptr);
    wcscat_s(directory, L"\\runtime.log");
    HANDLE file = CreateFileW(directory, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
                              nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return;
    char utf8[512]{};
    const int length = WideCharToMultiByte(CP_UTF8, 0, line, -1, utf8, sizeof(utf8), nullptr, nullptr);
    if (length > 1) {
        DWORD written = 0;
        WriteFile(file, utf8, static_cast<DWORD>(length - 1), &written, nullptr);
        WriteFile(file, "\r\n", 2, &written, nullptr);
    }
    CloseHandle(file);
}

bool supported_executable() {
    wchar_t path[MAX_PATH]{};
    if (!GetModuleFileNameW(nullptr, path, MAX_PATH)) return false;
    HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                              nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER size{};
    bool valid = GetFileSizeEx(file, &size) && size.QuadPart > 0 && size.QuadPart < 64 * 1024 * 1024;
    std::unique_ptr<std::uint8_t[]> bytes;
    if (valid) {
        bytes.reset(new (std::nothrow) std::uint8_t[static_cast<size_t>(size.QuadPart)]);
        DWORD read = 0;
        valid = bytes && ReadFile(file, bytes.get(), static_cast<DWORD>(size.QuadPart), &read, nullptr)
                && read == size.QuadPart;
    }
    CloseHandle(file);
    if (!valid) return false;
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0) return false;
    std::uint8_t hash[32]{};
    const bool hashed = BCryptHash(algorithm, nullptr, 0, bytes.get(),
                                   static_cast<ULONG>(size.QuadPart), hash, sizeof(hash)) >= 0;
    BCryptCloseAlgorithmProvider(algorithm, 0);
    return hashed && memcmp(hash, SupportedHash, sizeof(hash)) == 0;
}

DWORD WINAPI verify_worker(void*) {
    wchar_t entry[160]{};
    swprintf_s(entry, L"loader: attached to pid %lu", GetCurrentProcessId());
    log_line(entry);
    log_line(supported_executable() ? L"loader: supported iw4sp.exe SHA-256"
                                    : L"loader: unsupported executable; no modifications");
    log_line(L"loader: diagnostic mode; no game memory modified");
    return 0;
}
}  // namespace

extern "C" void* winmm_real[WINMM_EXPORT_COUNT]{};

BOOL WINAPI DllMain(HINSTANCE self, DWORD reason, void*) {
    if (reason != DLL_PROCESS_ATTACH) return TRUE;
    DisableThreadLibraryCalls(self);
    wchar_t path[MAX_PATH]{};
    const UINT length = GetSystemDirectoryW(path, MAX_PATH);
    if (!length || length >= MAX_PATH - 12) return FALSE;
    wcscat_s(path, L"\\winmm.dll");
    const HMODULE system_winmm = LoadLibraryW(path);
    if (!system_winmm) return FALSE;
    for (size_t index = 0; index < WINMM_EXPORT_COUNT; ++index)
        winmm_real[index] = reinterpret_cast<void*>(GetProcAddress(system_winmm, winmm_names[index]));
    for (const char* required : {"timeGetTime", "timeBeginPeriod", "timeEndPeriod"})
        if (!GetProcAddress(system_winmm, required)) return FALSE;
    HANDLE worker = CreateThread(nullptr, 0, verify_worker, nullptr, 0, nullptr);
    if (worker) CloseHandle(worker);
    return TRUE;
}
