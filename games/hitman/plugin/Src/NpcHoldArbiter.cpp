#include "NpcHoldArbiter.h"
#include <Windows.h>
#include <MinHook.h>
#include <array>
#include <atomic>
#include <mutex>

namespace {
using Native=void (*)(void*,bool);
std::mutex g_Install,g_Watch;
std::atomic<Native> g_Original=nullptr;
struct Record {
    std::atomic<void*> actor=nullptr;
    uint64_t scope=0,npc=0,birth=0,lease=0;
    std::atomic<uint64_t> external=0;
    std::atomic<bool> owned=false,retired=false;
    bool destructionReturned=false; // Watch mutex only.
};
std::array<Record,NpcHoldArbiter::Capacity> g_Records{};
enum class Mode { None, Legacy, Pool };
std::atomic<Mode> g_Mode=Mode::None;
uint64_t g_PoolScope=0; // Watch mutex only.
std::atomic<Record*> g_Current=nullptr;
std::atomic<bool> g_Installed=false,g_Busy=false,g_Armed=false,g_Executed=false;
std::atomic<uintptr_t> g_Stack=0;
void* g_Entry=nullptr;
bool g_Attempted=false;
NpcHoldArbiter::Report g_Report;
uintptr_t Stack() { return reinterpret_cast<uintptr_t>(reinterpret_cast<const NT_TIB*>(NtCurrentTeb())->StackBase); }
Record* FindActor(const void* actor) {
    // Watch mutex held. Invoke retains its selected record through g_Busy.
    if (!actor) return nullptr;
    for(auto& record:g_Records) if (record.actor.load(std::memory_order_acquire)==actor) return &record;
    return nullptr;
}
Record* Find(const NpcMutationGate::Ticket& ticket) {
    auto* record=FindActor(ticket.actor);
    return record && ticket.scope && record->scope==ticket.scope && record->npc==ticket.npc &&
        record->birth==ticket.birth && record->lease==ticket.lease ? record : nullptr;
}
void Worker(void* actor,bool held) {
    {
        std::lock_guard lock(g_Watch);
        if (auto* record=FindActor(actor)) {
            // Consume exactly one entry on the marked stack. A script recursively
            // invoking control inside the original is external, even on that stack.
            const bool own=g_Busy.load(std::memory_order_acquire) && g_Current.load()==record && Stack()==g_Stack.load() &&
                g_Armed.exchange(false,std::memory_order_acq_rel);
            if (!own) ++record->external;
            else {
                // Refuse our own marked operation if interference was observed
                // after Invoke's precheck. Every game/script call still runs.
                if (record->retired || record->external) return;
                g_Executed=true;
            }
        }
    }
    if (const auto original=g_Original.load(std::memory_order_acquire)) original(actor,held);
}
bool ArmRecord(Record* record,bool held) { // Watch mutex held.
    if (!g_Installed || !record || record->retired || record->external || (!held && !record->owned)) return false;
    bool idle=false;
    if (!g_Busy.compare_exchange_strong(idle,true)) return false;
    g_Current=record; g_Stack=Stack(); g_Executed=false; g_Armed=true;
    return true;
}
bool InvokeArmedRecord(Record* record,void* actor,bool held,bool strict) {
    struct Finish {
        ~Finish() { g_Armed=false; g_Current=nullptr; g_Busy=false; }
    } finish;
    // The busy flag pins all record metadata across a yielding native call.
    // ReleaseRetiredScope refuses reclamation until this call finishes.
    reinterpret_cast<Native>(g_Entry)(actor,held);
    const bool consumed=!g_Armed.exchange(false) && g_Executed.load();
    if (consumed) record->owned=held;
    return consumed && (!strict || (!record->retired && !record->external));
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
    std::lock_guard lock(g_Watch);
    if (g_Mode!=Mode::None) return false;
    g_Records[0].actor.store(actor,std::memory_order_release);
    g_Mode=Mode::Legacy; return true;
}
bool NpcHoldArbiter::Invoke(void* actor,bool held) {
    {
        std::lock_guard lock(g_Watch);
        if (g_Mode!=Mode::Legacy || g_Records[0].actor.load()!=actor || !ArmRecord(&g_Records[0],held)) return false;
    }
    return InvokeArmedRecord(&g_Records[0],actor,held,false);
}
bool NpcHoldArbiter::InstalledFor(const void* entry) { return g_Installed && entry==g_Entry; }
uint64_t NpcHoldArbiter::ExternalCalls() { std::lock_guard lock(g_Watch); return g_Records[0].external; }
bool NpcHoldArbiter::OwnsHold() { std::lock_guard lock(g_Watch); return g_Mode==Mode::Legacy && g_Records[0].owned; }
bool NpcHoldArbiter::Watch(const NpcMutationGate::Ticket& ticket) {
    NpcMutationGate::Pin pin(ticket);
    if (!g_Installed || !pin) return false;
    std::lock_guard lock(g_Watch);
    if (g_Mode==Mode::Legacy || (g_PoolScope && g_PoolScope!=ticket.scope)) return false;
    if (auto* existing=FindActor(ticket.actor)) return Find(ticket)==existing && !existing->retired;
    for(auto& record:g_Records) if (!record.actor.load(std::memory_order_acquire)) {
        record.scope=ticket.scope; record.npc=ticket.npc; record.birth=ticket.birth; record.lease=ticket.lease;
        g_PoolScope=ticket.scope; g_Mode=Mode::Pool;
        record.actor.store(ticket.actor,std::memory_order_release); return true;
    }
    return false;
}
bool NpcHoldArbiter::Invoke(const NpcMutationGate::Ticket& ticket,bool held) {
    NpcMutationGate::Pin pin(ticket);
    if (!pin) return false;
    Record* record=nullptr;
    bool strict=true;
    {
        std::lock_guard lock(g_Watch);
        if (g_Mode==Mode::Legacy && g_Records[0].actor.load()==ticket.actor) {
            record=&g_Records[0];
            strict=false;
        } else {
            if (g_Mode!=Mode::Pool || (!held && NpcMutationGate::BlocksInteraction(ticket.actor))) return false;
            record=Find(ticket);
        }
        if (!ArmRecord(record,held)) return false;
    }
    return InvokeArmedRecord(record,ticket.actor,held,strict);
}
uint64_t NpcHoldArbiter::ExternalCalls(const NpcMutationGate::Ticket& ticket) {
    std::lock_guard lock(g_Watch);
    const auto* record=Find(ticket);
    return record?record->external.load():UINT64_MAX;
}
bool NpcHoldArbiter::OwnsHold(const NpcMutationGate::Ticket& ticket) {
    NpcMutationGate::Pin pin(ticket);
    std::lock_guard lock(g_Watch);
    const auto* record=Find(ticket);
    return pin && record && record->owned && !record->retired && !record->external;
}
void NpcHoldArbiter::Invalidate(const void* actor,uint64_t birth) {
    std::lock_guard lock(g_Watch);
    if (auto* record=FindActor(actor);record && birth && record->birth==birth) record->retired=true;
}
void NpcHoldArbiter::ObserveDestructionReturn(const void* actor,uint64_t birth) {
    std::lock_guard lock(g_Watch);
    if (auto* record=FindActor(actor);record && birth && record->birth==birth && record->retired)
        record->destructionReturned=true;
}
NpcMutationGate::Scope NpcHoldArbiter::PoolScope() {
    std::lock_guard lock(g_Watch);
    return g_PoolScope;
}
bool NpcHoldArbiter::ReleaseRetiredScope(NpcMutationGate::Scope scope) {
    std::lock_guard lock(g_Watch);
    if (!scope || scope!=g_PoolScope || g_Mode!=Mode::Pool || g_Busy ||
        NpcMutationGate::CurrentScope()==scope) return false;
    for (const auto& record:g_Records)
        if (record.actor.load() && (!record.retired || !record.destructionReturned)) return false;
    // Every metadata reader holds g_Watch. Invoke pins its record with g_Busy
    // until after its final read, without retaining a mutex across native code.
    // Reclaim only our bookkeeping. No native actor is read, resumed or freed.
    for (auto& record:g_Records) {
        record.actor=nullptr;
        record.scope=record.npc=record.birth=record.lease=0;
        record.external=0; record.owned=false; record.retired=false;
        record.destructionReturned=false;
    }
    g_PoolScope=0;
    g_Mode=Mode::None;
    return true;
}
NpcHoldArbiter::Report NpcHoldArbiter::Installation(MinHookDiagnostics::LayoutTag<sizeof(Report)>) {
    std::lock_guard lock(g_Install); return g_Report;
}
