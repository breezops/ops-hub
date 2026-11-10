#include "NpcActorDestructionObserver.h"
#include <Windows.h>
#include <MinHook.h>
#include <array>
#include <atomic>
#include <cstring>
#include <mutex>

namespace {
using Body=void (*)(void*);
std::mutex installMutex,callbackMutex;
bool attempted=false;
std::atomic<bool> installed=false;
std::atomic<Body> original=nullptr;
std::atomic<uint64_t> entered=0,returned=0,invalidated=0,unknown=0;
void* context=nullptr;
NpcActorDestructionObserver::Invalidate invalidate=nullptr;
NpcActorDestructionObserver::Report report;
constexpr uint8_t prefix[]{
    0x48,0x89,0x5c,0x24,0x10,0x48,0x89,0x74,0x24,0x18,
    0x48,0x89,0x7c,0x24,0x20,0x41,0x56,0x48,0x83,0xec,0x20
};
void Observe(void* actor) {
    ++entered;
    {
        std::lock_guard lock(callbackMutex);
        // No game memory read; invalidation must remain a short pointer-only
        // operation. It must not reenter Stop, install or execute native code.
        if (invalidate) {
            if (invalidate(context,actor)) ++invalidated;
            else ++unknown;
        }
    }
    // No client pointer/book/session mutex survives into the native body.
    if (const auto body=original.load(std::memory_order_acquire)) body(actor);
    ++returned;
}
}
bool NpcActorDestructionObserver::Install(const NpcActorLifetimeSites::Sites& sites,void* client,Invalidate callback) {
    std::lock_guard lock(installMutex);
    const auto refuse=[](const char* stage,int status=0) { report.stage=stage; report.status=status; return false; };
    if (attempted) return refuse("already-attempted");
    if (!sites || !client || !callback) return refuse("invalid-config");
    std::array<uint8_t,sizeof(prefix)> bytes{}; SIZE_T count=0;
    if (!ReadProcessMemory(GetCurrentProcess(),sites.destructor,bytes.data(),bytes.size(),&count) ||
        count!=bytes.size() || std::memcmp(bytes.data(),prefix,bytes.size())) return refuse("entry-mismatch");
    HMODULE module=nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
        reinterpret_cast<LPCWSTR>(&Observe),&module)) return refuse("module-pin");
    attempted=true;
    const auto init=MH_Initialize();
    if (init!=MH_OK && init!=MH_ERROR_ALREADY_INITIALIZED) return refuse("library-init",init);
    void* body=nullptr;
    const auto create=MinHookDiagnostics::Create(const_cast<void*>(sites.destructor),
        reinterpret_cast<void*>(&Observe),&body,report.allocations);
    if (create!=MH_OK) return refuse("hook-create",create);
    if (!body) return refuse("missing-original");
    original.store(reinterpret_cast<Body>(body),std::memory_order_release);
    { std::lock_guard callbacks(callbackMutex); context=client; invalidate=callback; }
    const auto enable=MH_EnableHook(const_cast<void*>(sites.destructor));
    if (enable!=MH_OK) { Stop(); return refuse("hook-enable",enable); }
    installed=true; report.stage="installed"; report.status=0; return true;
}
void NpcActorDestructionObserver::Stop() {
    std::lock_guard lock(callbackMutex);
    invalidate=nullptr; context=nullptr;
}
NpcActorDestructionObserver::Stats NpcActorDestructionObserver::Diagnostics() {
    std::lock_guard lock(callbackMutex);
    return {entered.load(),returned.load(),invalidated.load(),unknown.load(),installed.load(),invalidate!=nullptr};
}
NpcActorDestructionObserver::Report NpcActorDestructionObserver::Installation(MinHookDiagnostics::LayoutTag<sizeof(Report)>) {
    std::lock_guard lock(installMutex); return report;
}
