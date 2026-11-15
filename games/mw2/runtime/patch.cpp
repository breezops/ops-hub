#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <array>
#include <climits>
#include <cstdint>
#include <cstring>
#include <cwchar>
#include <vector>
#include "patch.h"
#include "patch_plan.h"

namespace {
constexpr std::uint32_t ClientBase = 0x19ADA98, ClientStride = 0x1678;
constexpr std::uint32_t ConnectionBase = 0x540BA0, ConnectionStride = 0x2650;
constexpr std::uint32_t PlayerBase = 0xEEE070, PlayerStride = 0xAFE0;
constexpr std::uint32_t AuxBase = 0xD96B00, AuxStride = 0x198;
constexpr std::array<std::uint32_t, 4> OldBases = {ClientBase, ConnectionBase, PlayerBase, AuxBase};
constexpr std::array<std::uint32_t, 4> Strides = {ClientStride, ConnectionStride, PlayerStride, AuxStride};
constexpr unsigned OriginalCount = 2, ExpandedCount = 4;

struct Change {
    std::uint8_t* address;
    std::uint8_t size;
    std::array<std::uint8_t, 15> before{};
    std::array<std::uint8_t, 15> after{};
};

std::array<std::uint8_t*, 4> buffers{};
std::uint8_t* aux_trampolines = nullptr;
std::vector<Change> installed;

std::uintptr_t align_up(std::uintptr_t value, std::uintptr_t alignment) {
    return (value + alignment - 1) & ~(alignment - 1);
}

void release_buffers() {
    if (aux_trampolines) VirtualFree(aux_trampolines, 0, MEM_RELEASE);
    aux_trampolines = nullptr;
    for (auto& buffer : buffers) {
        if (buffer) VirtualFree(buffer, 0, MEM_RELEASE);
        buffer = nullptr;
    }
}

std::uint8_t* allocate_near(std::uint8_t* module, std::size_t image_size,
                            std::size_t bytes, DWORD protection = PAGE_READWRITE) {
    SYSTEM_INFO system{};
    GetSystemInfo(&system);
    const std::uintptr_t granularity = system.dwAllocationGranularity;
    const std::uintptr_t start = align_up(reinterpret_cast<std::uintptr_t>(module) +
                                          image_size + granularity, granularity);
    for (std::uintptr_t step = 0; step < 8192; ++step) {
        const std::uintptr_t candidate = start + step * granularity;
        if (candidate < start) break;
        void* result = VirtualAlloc(reinterpret_cast<void*>(candidate), bytes,
                                    MEM_COMMIT | MEM_RESERVE, protection);
        if (result) return static_cast<std::uint8_t*>(result);
    }
    return nullptr;
}

bool write_bytes(std::uint8_t* address, const std::uint8_t* bytes, std::size_t size) {
    DWORD old_protection = 0;
    if (!VirtualProtect(address, size, PAGE_EXECUTE_READWRITE, &old_protection)) return false;
    std::memcpy(address, bytes, size);
    FlushInstructionCache(GetCurrentProcess(), address, size);
    DWORD ignored = 0;
    return VirtualProtect(address, size, old_protection, &ignored) != 0;
}

bool valid_image(std::uint8_t* module, std::uint32_t& size) {
    if (!module || reinterpret_cast<IMAGE_DOS_HEADER*>(module)->e_magic != IMAGE_DOS_SIGNATURE)
        return false;
    const auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(module);
    if (dos->e_lfanew < 0x40 || dos->e_lfanew > 0x1000) return false;
    const auto* nt = reinterpret_cast<IMAGE_NT_HEADERS64*>(module + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE || nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC)
        return false;
    size = nt->OptionalHeader.SizeOfImage;
    return size > ClientBase + ExpandedCount * ClientStride && size < 128 * 1024 * 1024;
}

bool append_aux_hooks(std::uint8_t* module, std::size_t image_size,
                      std::vector<Change>& changes, PatchLog log) {
    // Both sites index the old auxiliary records using two registers and an
    // absolute disp32. The new array cannot fit in that address field.
    constexpr std::uint32_t sites[2] = {0x19EEF4, 0x1B14D5};
    constexpr std::uint8_t expected[2][8] = {
        {0x4e,0x8b,0x9c,0x31,0x78,0x6c,0xd9,0x00},
        {0x4a,0x8b,0x8c,0x39,0x78,0x6c,0xd9,0x00}};
    // The anchor register contains the image base. Save a volatile scratch
    // register, calculate the two-register address, then add the relocated
    // RVA. This preserves flags and the original anchor convention.
    constexpr std::uint8_t prefixes[2][6] = {
        {0x41,0x52,0x4e,0x8d,0x14,0x31}, // push r10; lea r10,[rcx+r14]
        {0x41,0x53,0x4e,0x8d,0x1c,0x39}}; // push r11; lea r11,[rcx+r15]
    constexpr std::uint8_t loads[2][6] = {
        {0x4f,0x8b,0x1c,0x1a,0x41,0x5a}, // mov r11,[r10+r11]; pop r10
        {0x49,0x8b,0x0c,0x0b,0x41,0x5b}}; // mov rcx,[r11+rcx]; pop r11
    aux_trampolines = allocate_near(module, image_size, 128, PAGE_EXECUTE_READWRITE);
    if (!aux_trampolines) { log(L"patch: auxiliary trampoline allocation failed"); return false; }
    for (unsigned index = 0; index < 2; ++index) {
        Change change{};
        change.address = module + sites[index];
        change.size = 8;
        std::memcpy(change.before.data(), expected[index], 8);
        if (std::memcmp(change.address, expected[index], 8) != 0) {
            log(L"patch: auxiliary instruction differs"); return false;
        }
        auto* stub = aux_trampolines + index * 64;
        std::memcpy(stub, prefixes[index], 6);
        stub[6] = index == 0 ? 0x49 : 0x48; // mov r11/rcx, imm64
        stub[7] = index == 0 ? 0xbb : 0xb9;
        const auto address = static_cast<std::uint64_t>(
            reinterpret_cast<std::uintptr_t>(buffers[3] + 0x178) -
            reinterpret_cast<std::uintptr_t>(module));
        std::memcpy(stub + 8, &address, sizeof(address));
        std::memcpy(stub + 16, loads[index], 6);
        stub[22] = 0xe9;
        const auto back = reinterpret_cast<std::intptr_t>(change.address + 8) -
                          reinterpret_cast<std::intptr_t>(stub + 27);
        const auto forward = reinterpret_cast<std::intptr_t>(stub) -
                             reinterpret_cast<std::intptr_t>(change.address + 5);
        if (back < INT32_MIN || back > INT32_MAX ||
            forward < INT32_MIN || forward > INT32_MAX) {
            log(L"patch: auxiliary trampoline out of jump range"); return false;
        }
        const auto back32 = static_cast<std::int32_t>(back);
        const auto forward32 = static_cast<std::int32_t>(forward);
        std::memcpy(stub + 23, &back32, 4);
        change.after[0] = 0xe9;
        std::memcpy(change.after.data() + 1, &forward32, 4);
        std::memset(change.after.data() + 5, 0x90, 3);
        changes.push_back(change);
    }
    FlushInstructionCache(GetCurrentProcess(), aux_trampolines, 128);
    return true;
}
}  // namespace

bool apply_storage_patch(std::uint8_t* module, PatchLog log) {
    if (!installed.empty()) return false;
    std::uint32_t image_size = 0;
    if (!valid_image(module, image_size)) { log(L"patch: invalid mapped PE image"); return false; }
    for (unsigned kind = 0; kind < buffers.size(); ++kind) {
        const std::size_t bytes = ExpandedCount * Strides[kind];
        buffers[kind] = allocate_near(module, image_size, bytes);
        if (!buffers[kind]) { log(L"patch: nearby allocation failed"); release_buffers(); return false; }
        std::memcpy(buffers[kind], module + OldBases[kind], OriginalCount * Strides[kind]);
    }
    std::vector<Change> changes;
    changes.reserve(PatchCount + 2);
    for (const PatchSpec& spec : PatchPlan) {
        if (spec.rva >= image_size || spec.size > 15 || spec.size == 0 ||
            spec.rva + spec.size > image_size || spec.kind > 6) {
            log(L"patch: invalid plan entry"); release_buffers(); return false;
        }
        Change change{};
        change.address = module + spec.rva;
        change.size = spec.size;
        std::memcpy(change.before.data(), spec.expected, spec.size);
        std::memcpy(change.after.data(), spec.expected, spec.size);
        if (std::memcmp(change.address, change.before.data(), spec.size) != 0) {
            log(L"patch: instruction bytes differ; refusing build");
            release_buffers(); return false;
        }
        if (spec.kind == 4) {
            std::memcpy(change.after.data(), spec.replacement, spec.size);
        } else {
            const unsigned buffer_kind = spec.kind >= 5 ? spec.kind - 5 : spec.kind;
            if (spec.target_offset > ExpandedCount * Strides[buffer_kind] ||
                spec.disp_offset + 4 > spec.size) {
                log(L"patch: target outside storage"); release_buffers(); return false;
            }
            const auto target = reinterpret_cast<std::intptr_t>(buffers[buffer_kind] + spec.target_offset);
            const auto anchor = reinterpret_cast<std::intptr_t>(
                spec.kind >= 5 ? module : change.address + spec.size);
            const auto displacement = target - anchor;
            if (displacement < INT32_MIN || displacement > INT32_MAX) {
                log(L"patch: allocation beyond disp32 range"); release_buffers(); return false;
            }
            const auto signed_disp = static_cast<std::int32_t>(displacement);
            std::memcpy(change.after.data() + spec.disp_offset, &signed_disp, sizeof(signed_disp));
        }
        changes.push_back(change);
    }
    if (!append_aux_hooks(module, image_size, changes, log)) {
        release_buffers(); return false;
    }
    for (std::size_t index = 0; index < changes.size(); ++index) {
        if (!write_bytes(changes[index].address, changes[index].after.data(), changes[index].size)) {
            log(L"patch: write failed; rolling back");
            for (std::size_t back = index + 1; back > 0; --back)
                write_bytes(changes[back - 1].address, changes[back - 1].before.data(),
                            changes[back - 1].size);
            release_buffers();
            return false;
        }
    }
    installed = std::move(changes);
    wchar_t addresses[256]{};
    swprintf_s(addresses, L"patch: clients=%p connections=%p players=%p aux=%p",
               buffers[0], buffers[1], buffers[2], buffers[3]);
    log(addresses);
    log(L"patch: storage sites installed; four records per array allocated");
    return true;
}

bool remove_storage_patch(PatchLog log) {
    if (installed.empty()) return false;
    bool restored = true;
    for (auto it = installed.rbegin(); it != installed.rend(); ++it)
        restored &= write_bytes(it->address, it->before.data(), it->size);
    if (!restored) { log(L"patch: rollback incomplete; buffers retained"); return false; }
    installed.clear();
    release_buffers();
    log(L"patch: all sites restored and buffers freed");
    return true;
}
