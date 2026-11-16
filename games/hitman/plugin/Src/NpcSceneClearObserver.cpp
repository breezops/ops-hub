#include "NpcSceneClearObserver.h"
#include <Windows.h>
#include <array>
#include <atomic>
#include <mutex>

namespace {
using Clear = void (*)(void*, bool);
constexpr uintptr_t ClearRva = 0xb1d30;
constexpr uintptr_t TailRva = 0xb232b;
constexpr uintptr_t SlotRva = 0x1d51b08;
constexpr std::array<uint8_t, 39> ClearTail{
    0x48,0x8b,0x03,0xba,0x02,0x00,0x00,0x00,0x48,0x8b,0xcb,
    0x48,0x8b,0x5c,0x24,0x58,0x48,0x8b,0x74,0x24,0x60,
    0x48,0x8b,0x7c,0x24,0x68,0x48,0x83,0xc4,0x40,0x41,0x5e,
    0x48,0xff,0xa0,0xd0,0x00,0x00,0x00
};
std::mutex installMutex, callbackMutex;
std::atomic<Clear> original = nullptr;
void** installedSlot = nullptr;
void* client = nullptr;
NpcSceneClearObserver::ReturnCallback callback = nullptr;

struct Dispatch {
    const void* scene;
    bool fullUnload;
    uint64_t token = 0;
};
thread_local Dispatch* currentDispatch = nullptr;

void Observe(void* scene, bool fullUnload) {
    Dispatch call{scene, fullUnload};
    struct RestoreDispatch {
        Dispatch* previous;
        ~RestoreDispatch() { currentDispatch = previous; }
    } restore{currentDispatch};
    currentDispatch = &call;

    // Call the normal SDK-patched game entry. No observer lock spans dispatch.
    original.load(std::memory_order_acquire)(scene, fullUnload);
    currentDispatch = restore.previous;
    std::lock_guard lock(callbackMutex);
    if (callback && call.token) callback(client, call.token);
}

bool Replace(void** slot, void* expected, void* replacement) {
    DWORD protection = 0;
    if (!VirtualProtect(slot, sizeof(*slot), PAGE_READWRITE, &protection)) return false;
    const auto prior = InterlockedCompareExchangePointer(slot, replacement, expected);
    DWORD ignored = 0;
    VirtualProtect(slot, sizeof(*slot), protection, &ignored);
    return prior == expected;
}

// Caller holds the installation mutex.
bool InstallSlot(void** slot, void* entry, void* context, NpcSceneClearObserver::ReturnCallback completion) {
    std::lock_guard callbacks(callbackMutex);
    original.store(reinterpret_cast<Clear>(entry), std::memory_order_release);
    if (!Replace(slot, entry, reinterpret_cast<void*>(&Observe))) {
        original.store(nullptr, std::memory_order_release);
        return false;
    }
    installedSlot = slot;
    client = context;
    callback = completion;
    return true;
}
}

bool NpcSceneClearObserver::Install(void* context, ReturnCallback completion) {
    std::lock_guard lock(installMutex);
    if (!context || !completion || original.load()) return false;
    const auto base = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    if (!base) return false;
    auto** slot = reinterpret_cast<void**>(base + SlotRva);
    auto* entry = reinterpret_cast<void*>(base + ClearRva);
    void* target = nullptr;
    std::array<uint8_t, ClearTail.size()> bytes{};
    if (!ReadProcessMemory(GetCurrentProcess(), slot, &target, sizeof(target), nullptr) || target != entry ||
        !ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(base + TailRva), bytes.data(), bytes.size(), nullptr) ||
        bytes != ClearTail) return false;

    // A caller may already have loaded the old slot when Stop restores it.
    // As with the destructor probe, retain the bridge until process exit.
    HMODULE module = nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
        reinterpret_cast<LPCWSTR>(&Observe), &module)) return false;

    return InstallSlot(slot, entry, context, completion);
}

void NpcSceneClearObserver::Stop() {
    std::lock_guard lock(installMutex);
    {
        std::lock_guard callbacks(callbackMutex);
        callback = nullptr;
        client = nullptr;
    }
    if (installedSlot && Replace(installedSlot, reinterpret_cast<void*>(&Observe),
        reinterpret_cast<void*>(original.load()))) installedSlot = nullptr;
}

bool NpcSceneClearObserver::AssociatePreClear(const void* scene, bool fullUnload, uint64_t token) {
    auto* call = currentDispatch;
    if (!token || !call || call->scene != scene || call->fullUnload != fullUnload || call->token) return false;
    call->token = token;
    return true;
}

#ifdef COOP_SCENE_CLEAR_TESTS
bool NpcSceneClearObserver::InstallFixture(void** slot, void* entry, void* context, ReturnCallback completion) {
    std::lock_guard lock(installMutex);
    return !original.load() && InstallSlot(slot, entry, context, completion);
}
#endif
