#include "NpcHoldArbiter.h"
#include <Windows.h>
#include <MinHook.h>
#include <array>
#include <atomic>
#include <mutex>

namespace {
using Native=void (*)(void*,bool);
std::mutex g_Install;
std::atomic<Native> g_Original=nullptr;
std::atomic<void*> g_Watched=nullptr;
std::atomic<uint64_t> g_External=0;
std::atomic<bool> g_Installed=false,g_Owned=false,g_Busy=false,g_Armed=false;
std::atomic<uintptr_t> g_Stack=0;
void* g_Entry=nullptr;
bool g_Attempted=false;
NpcHoldArbiter::Report g_Report;
uintptr_t Stack() { return reinterpret_cast<uintptr_t>(reinterpret_cast<const NT_TIB*>(NtCurrentTeb())->StackBase); }
void Worker(void* actor,bool held) {
    if (actor==g_Watched.load(std::memory_order_acquire)) {
        // Consume exactly one entry on the marked stack. A script recursively
        // invoking control inside the original is external, even on that stack.
        const bool own=g_Busy.load(std::memory_order_acquire) && Stack()==g_Stack.load() &&
            g_Armed.exchange(false,std::memory_order_acq_rel);
        if (!own) ++g_External;
    }
    if (const auto original=g_Original.load(std::memory_order_acquire)) original(actor,held);
}
}
bool NpcHoldArbiter::Install(const CrashGuards::NpcBehaviorSites& sites) {
    std::lock_guard lock(g_Install);
    if (g_Attempted || !sites) return false;
    const std::array<uint8_t,10> expected{0x48,0x89,0x5c,0x24,0x08,0x57,0x48,0x83,0xec,0x20};
    std::array<uint8_t,10> bytes{}; SIZE_T read=0;
    if (!ReadProcessMemory(GetCurrentProcess(),sites.actorControl,bytes.data(),bytes.size(),&read) || read!=bytes.size() || bytes!=expected) {
        g_Report.stage="entry-mismatch"; return false;
    }
    HMODULE module=nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
        reinterpret_cast<LPCWSTR>(&Worker),&module)) { g_Report.stage="module-pin"; return false; }
    g_Attempted=true;
    const auto init=MH_Initialize();
    if (init!=MH_OK && init!=MH_ERROR_ALREADY_INITIALIZED) { g_Report.stage="library-init";g_Report.status=init;return false; }
    void* original=nullptr;
    g_Report.status=MinHookDiagnostics::Create(sites.actorControl,reinterpret_cast<void*>(&Worker),&original,g_Report.allocations);
    if (g_Report.status!=MH_OK || !original) { g_Report.stage="hook-create";return false; }
    g_Entry=sites.actorControl; g_Original.store(reinterpret_cast<Native>(original),std::memory_order_release);
    g_Report.status=MH_EnableHook(g_Entry);
    if (g_Report.status!=MH_OK) { g_Report.stage="hook-enable";return false; }
    g_Installed=true; g_Report.stage="installed"; return true;
}
bool NpcHoldArbiter::Watch(void* actor) {
    if (!g_Installed || !actor) return false;
    void* empty=nullptr;
    return g_Watched.compare_exchange_strong(empty,actor);
}
bool NpcHoldArbiter::Invoke(void* actor,bool held) {
    if (!g_Installed || !actor || g_Watched.load()!=actor || g_External.load() || (!held && !g_Owned)) return false;
    bool idle=false;
    if (!g_Busy.compare_exchange_strong(idle,true)) return false;
    // No yielding game function runs between priming the exact-stack marker
    // and entering the watched native entry; no TLS or mutex spans that call.
    g_Stack=Stack(); g_Armed=true;
    reinterpret_cast<Native>(g_Entry)(actor,held);
    const bool consumed=!g_Armed.exchange(false);
    if (consumed) g_Owned=held;
    g_Busy=false;
    return consumed;
}
bool NpcHoldArbiter::InstalledFor(const void* entry) { return g_Installed && entry==g_Entry; }
uint64_t NpcHoldArbiter::ExternalCalls() { return g_External; }
bool NpcHoldArbiter::OwnsHold() { return g_Owned; }
NpcHoldArbiter::Report NpcHoldArbiter::Installation(MinHookDiagnostics::LayoutTag<sizeof(Report)>) {
    std::lock_guard lock(g_Install); return g_Report;
}
