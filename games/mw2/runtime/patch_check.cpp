#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>
#include "patch.h"
#include "patch_plan.h"

static void log_line(const wchar_t* message) { std::wprintf(L"%ls\n", message); }

int wmain(int argc, wchar_t** argv) {
    if (argc != 2) { std::fwprintf(stderr, L"Usage: patch_check <iw4sp.exe>\n"); return 2; }
    HANDLE file = CreateFileW(argv[1], GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (file == INVALID_HANDLE_VALUE) return 2;
    LARGE_INTEGER length{};
    if (!GetFileSizeEx(file, &length) || length.QuadPart <= 0 || length.QuadPart > 64 * 1024 * 1024) return 2;
    std::vector<std::uint8_t> disk(static_cast<std::size_t>(length.QuadPart));
    DWORD read = 0;
    const bool loaded = ReadFile(file, disk.data(), static_cast<DWORD>(disk.size()), &read, nullptr) &&
                        read == disk.size();
    CloseHandle(file);
    if (!loaded) return 2;
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(disk.data());
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew < 0x40 ||
        static_cast<std::size_t>(dos->e_lfanew) + sizeof(IMAGE_NT_HEADERS64) > disk.size()) return 2;
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(disk.data() + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE || nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC) return 2;
    const auto image_size = nt->OptionalHeader.SizeOfImage;
    auto* image = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, image_size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    if (!image) return 2;
    std::memcpy(image, disk.data(), nt->OptionalHeader.SizeOfHeaders);
    const auto* section = IMAGE_FIRST_SECTION(nt);
    for (unsigned index = 0; index < nt->FileHeader.NumberOfSections; ++index) {
        const auto& current = section[index];
        if (current.PointerToRawData + current.SizeOfRawData > disk.size() ||
            current.VirtualAddress + current.SizeOfRawData > image_size) return 2;
        std::memcpy(image + current.VirtualAddress, disk.data() + current.PointerToRawData,
                    current.SizeOfRawData);
    }
    const std::vector<std::uint8_t> original(image, image + image_size);
    image[PatchPlan[0].rva] ^= 1;
    if (apply_storage_patch(image, log_line)) return 1;
    image[PatchPlan[0].rva] ^= 1;
    if (std::memcmp(image, original.data(), image_size) != 0) return 1;
    if (!apply_storage_patch(image, log_line)) return 1;
    unsigned changed = 0;
    for (const auto& spec : PatchPlan)
        changed += std::memcmp(image + spec.rva, original.data() + spec.rva, spec.size) != 0;
    if (changed != PatchCount) return 1;
    for (const std::uint32_t hook_rva : {0x19EEF4u, 0x1B14D5u}) {
        if (image[hook_rva] != 0xE9 ||
            std::memcmp(image + hook_rva, original.data() + hook_rva, 8) == 0) return 1;
        ++changed;
    }
    if (!remove_storage_patch(log_line)) return 1;
    if (std::memcmp(image, original.data(), image_size) != 0) return 1;
    VirtualFree(image, 0, MEM_RELEASE);
    std::printf("PASS %u patch sites installed and fully rolled back in isolated mapped image\n", changed);
    return 0;
}
