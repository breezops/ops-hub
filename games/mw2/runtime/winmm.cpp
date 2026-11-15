// Local MW2 x64 test loader. Forwards WinMM and applies an exact-build
// storage patch only when the explicit local test marker is present.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>
#include <cstdint>
#include <cwchar>
#include <memory>
#include <initializer_list>
#include "winmm_names.h"
#ifdef SPECOPS4_PATCH_PLAN
#include "patch.h"
#endif

#pragma comment(lib, "bcrypt.lib")

namespace {
constexpr std::uint8_t SupportedHash[32] = {
    0x2f, 0x8c, 0x9c, 0xfd, 0xa1, 0xb8, 0xc0, 0xfb,
    0x30, 0x8f, 0xd9, 0x71, 0x95, 0x77, 0x15, 0x4e,
    0x82, 0x22, 0xfc, 0x76, 0x54, 0xf5, 0xe8, 0xf1,
    0xbf, 0x54, 0x57, 0x9b, 0xca, 0x69, 0xd8, 0x05,
};
#ifdef SPECOPS4_PATCH_PLAN
volatile LONG reported_exception = 0;
#endif

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

#ifdef SPECOPS4_PATCH_PLAN
LONG CALLBACK trace_exception(EXCEPTION_POINTERS* exception) {
    if (!exception || !exception->ExceptionRecord || !exception->ContextRecord ||
        exception->ExceptionRecord->ExceptionCode != EXCEPTION_ACCESS_VIOLATION ||
        InterlockedIncrement(&reported_exception) > 8)
        return EXCEPTION_CONTINUE_SEARCH;
    auto* context = exception->ContextRecord;
    const auto* module = reinterpret_cast<std::uint8_t*>(GetModuleHandleW(nullptr));
    wchar_t line[512]{};
    swprintf_s(line,
               L"fault: exe+rip=%llx access=%llu address=%p rax=%llx rcx=%llx rdx=%llx rsp=%llx",
               static_cast<unsigned long long>(context->Rip - reinterpret_cast<std::uintptr_t>(module)),
               static_cast<unsigned long long>(exception->ExceptionRecord->ExceptionInformation[0]),
               reinterpret_cast<void*>(exception->ExceptionRecord->ExceptionInformation[1]),
               static_cast<unsigned long long>(context->Rax),
               static_cast<unsigned long long>(context->Rcx),
               static_cast<unsigned long long>(context->Rdx),
               static_cast<unsigned long long>(context->Rsp));
    log_line(line);
    swprintf_s(line, L"fault: r11=%llx r12=%llx r13=%llx r14=%llx r15=%llx",
               static_cast<unsigned long long>(context->R11),
               static_cast<unsigned long long>(context->R12),
               static_cast<unsigned long long>(context->R13),
               static_cast<unsigned long long>(context->R14),
               static_cast<unsigned long long>(context->R15));
    log_line(line);
    swprintf_s(line, L"fault: rbx=%llx rbp=%llx rsi=%llx rdi=%llx r8=%llx r9=%llx r10=%llx",
               static_cast<unsigned long long>(context->Rbx),
               static_cast<unsigned long long>(context->Rbp),
               static_cast<unsigned long long>(context->Rsi),
               static_cast<unsigned long long>(context->Rdi),
               static_cast<unsigned long long>(context->R8),
               static_cast<unsigned long long>(context->R9),
               static_cast<unsigned long long>(context->R10));
    log_line(line);
    std::uintptr_t return_address = 0;
    SIZE_T read = 0;
    if (ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(context->Rsp),
                          &return_address, sizeof(return_address), &read) && read == sizeof(return_address)) {
        swprintf_s(line, L"fault: return=exe+%llx",
                   static_cast<unsigned long long>(return_address -
                   reinterpret_cast<std::uintptr_t>(module)));
        log_line(line);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}
#endif

DWORD WINAPI verify_worker(void* module) {
    wchar_t entry[160]{};
    swprintf_s(entry, L"loader: attached to pid %lu", GetCurrentProcessId());
    log_line(entry);
    const bool supported = supported_executable();
    log_line(supported ? L"loader: supported iw4sp.exe SHA-256"
                       : L"loader: unsupported executable; no modifications");
#ifdef SPECOPS4_PATCH_PLAN
    wchar_t marker[MAX_PATH]{};
    const DWORD path_length = GetModuleFileNameW(static_cast<HMODULE>(module), marker, MAX_PATH);
    wchar_t* separator = path_length < MAX_PATH ? wcsrchr(marker, L'\\') : nullptr;
    if (supported && separator &&
        wcscpy_s(separator + 1, MAX_PATH - static_cast<size_t>(separator + 1 - marker),
                 L"specops4.storage-test.enable") == 0 &&
        GetFileAttributesW(marker) != INVALID_FILE_ATTRIBUTES) {
        log_line(L"loader: local storage test gate found");
        if (!apply_storage_patch(reinterpret_cast<std::uint8_t*>(GetModuleHandleW(nullptr)), log_line))
            log_line(L"loader: storage patch refused; game left unmodified");
        else if (AddVectoredExceptionHandler(1, trace_exception))
            log_line(L"loader: local fault trace armed");
        return 0;
    }
#endif
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
    HANDLE worker = CreateThread(nullptr, 0, verify_worker, self, 0, nullptr);
    if (worker) CloseHandle(worker);
    return TRUE;
}
