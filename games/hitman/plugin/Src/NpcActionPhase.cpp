#include "NpcActionPhase.h"
#include "NpcActionProbe.h"
#include "NpcHoldArbiter.h"
#include <Windows.h>
#include <algorithm>
#include <atomic>
#include <cstring>
#include <mutex>

namespace {
using Loop=void (*)(void*,const void*);
using ActorControl=void (*)(void*,bool);
constexpr std::array<uint8_t,15> Prologue{
    0x40,0x56,0x41,0x54,0x41,0x55,0x41,0x56,0x48,0x83,0xec,0x38,0x4c,0x8b,0x02
};
std::mutex g_Install;
std::atomic<bool> g_Installed=false,g_Healthy=true;
std::atomic<uint32_t> g_Callers=0;
std::atomic<Loop> g_Original=nullptr;
CrashGuards::NpcBehaviorSites g_Sites;
NpcActionPhase::Config g_Config;
void* g_Entry=nullptr;
uint8_t* g_Trampoline=nullptr;
bool Read(const void* at,void* out,size_t size) {
    SIZE_T count=0;
    return at && ReadProcessMemory(GetCurrentProcess(),at,out,size,&count) && count==size;
}
uintptr_t Stack() {
    return reinterpret_cast<uintptr_t>(reinterpret_cast<const NT_TIB*>(NtCurrentTeb())->StackBase);
}
bool Executable(void* entry) {
    MEMORY_BASIC_INFORMATION info{};
    return entry && VirtualQuery(entry,&info,sizeof(info)) && info.State==MEM_COMMIT &&
        !(info.Protect&(PAGE_GUARD|PAGE_NOACCESS)) &&
        (info.Protect&(PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY));
}
std::array<uint8_t,14> Jump(const void* target) {
    std::array<uint8_t,14> bytes{0xff,0x25,0,0,0,0};
    std::memcpy(bytes.data()+6,&target,sizeof(target)); return bytes;
}
bool Write(void* at,const void* bytes,size_t size) {
    DWORD old=0;
    if (!VirtualProtect(at,size,PAGE_EXECUTE_READWRITE,&old)) return false;
    std::memcpy(at,bytes,size); FlushInstructionCache(GetCurrentProcess(),at,size);
    DWORD ignored=0; VirtualProtect(at,size,old,&ignored); return true;
}
struct NativeState {
    int32_t runtime=-1;
    uint8_t actorFlags=0;
    std::array<uint8_t,0xf0> unit;
    bool held() const { return (actorFlags&0x40) && (unit[0x2c]&1); }
    bool enabled() const { return !(actorFlags&0x40) && !(unit[0x2c]&1); }
    uintptr_t Pointer(size_t at) const { uintptr_t p=0; std::memcpy(&p,unit.data()+at,sizeof(p)); return p; }
    bool drained() const { return !Pointer(0xa0) && !Pointer(0xe0) && !Pointer(0xe8); }
};
bool ReadState(const NpcMutationGate::Ticket& ticket,int32_t id,void* unit,NativeState& out) {
    const auto* actor=static_cast<const uint8_t*>(ticket.actor);
    return Read(actor+0x119c,&out.runtime,sizeof(out.runtime)) && out.runtime==id &&
        Read(actor+0x117d,&out.actorFlags,1) && Read(unit,out.unit.data(),out.unit.size()) &&
        out.Pointer(8)==reinterpret_cast<uintptr_t>(ticket.actor);
}
}

struct NpcActionPhase::Bridge {
    static void Run(void* table,const void* span) {
        const auto callers=g_Callers.fetch_add(1)+1;
        if (g_Installed && GetCurrentThreadId()!=g_Config.gameThread) g_Healthy=false;
        if (g_Installed && g_Healthy && g_Config.allowControl && !NpcActionProbe::InstalledFor(g_Sites.actions.job)) g_Healthy=false;
        if (g_Installed && g_Healthy && callers==1) {
            Boundary boundary;
            std::array<uintptr_t,2> bounds{};
            const bool valid=table==g_Sites.table && Read(span,bounds.data(),sizeof(bounds)) &&
                bounds[1]>=bounds[0] && (bounds[1]-bounds[0])%4==0 &&
                (bounds[1]-bounds[0])/4<=boundary.m_Ids.size();
            boundary.m_Count=valid ? (bounds[1]-bounds[0])/4 : 0;
            if (valid && (!boundary.m_Count || Read(reinterpret_cast<void*>(bounds[0]),boundary.m_Ids.data(),boundary.m_Count*4))) {
                boundary.m_Config=&g_Config; boundary.m_Sites=&g_Sites;
                boundary.m_Enabled=&g_Installed; boundary.m_Healthy=&g_Healthy;
                boundary.m_Stack=Stack(); boundary.m_Active=true;
                try { g_Config.callback(g_Config.context,boundary); }
                catch (...) { g_Healthy=false; }
                boundary.m_Active=false;
            } else g_Healthy=false;
        } else if (callers!=1) g_Healthy=false;
        // Native scheduling sees the engine's own held-unit flag. Never filter
        // the caller's span or skip the original loop, including on faults.
        if (const auto original=g_Original.load(std::memory_order_acquire)) original(table,span);
        else g_Healthy=false;
        --g_Callers;
    }
};
namespace {
auto Patch() {
    auto bytes=Prologue; bytes.fill(0x90);
    const auto jump=Jump(reinterpret_cast<void*>(&NpcActionPhase::Bridge::Run));
    std::memcpy(bytes.data(),jump.data(),jump.size()); return bytes;
}
}

bool NpcActionPhase::Boundary::ActiveHere() const {
    const auto* tib=reinterpret_cast<const NT_TIB*>(NtCurrentTeb());
    const auto self=reinterpret_cast<uintptr_t>(this);
    return m_Active && m_Config && m_Sites && m_Enabled && m_Healthy && *m_Enabled && *m_Healthy && GetCurrentThreadId()==m_Config->gameThread && Stack()==m_Stack &&
        self>=reinterpret_cast<uintptr_t>(tib->StackLimit) && self<m_Stack;
}
NpcActionPhase::Observation NpcActionPhase::Boundary::Observe(const void* actor,uint64_t npc,uint64_t birth) const {
    if (!ActiveHere() || !m_Config->observe || !actor || !npc || !birth) return {};
    int32_t id=-1;
    if (!m_Config->observe(m_Config->context,actor,npc,birth,id) || id<0 || id>=static_cast<int32_t>(m_Ids.size()) ||
        std::find(m_Ids.begin(),m_Ids.begin()+m_Count,id)==m_Ids.begin()+m_Count) return {};
    NpcBehaviorReadback::Snapshot state;
    if (!NpcBehaviorReadback::Read(m_Sites->table,actor,id,state)) return {};
    int32_t current=-1;
    if (!m_Config->observe(m_Config->context,actor,npc,birth,current) || current!=id) return {};
    return {true,id,state}; // Includes conflicting flags; observation grants no mutation permission.
}
NpcActionPhase::Result NpcActionPhase::Boundary::SetHeld(const NpcMutationGate::Ticket& ticket,bool held) {
    if (!ActiveHere() || !m_Config->allowControl || !m_Config->validate) return {};
    NpcMutationGate::Pin pin(ticket);
    if (!pin || (!held && NpcMutationGate::BlocksInteraction(ticket.actor)) ||
        !NpcActionProbe::InstalledFor(m_Sites->actions.job)) return {};
    int32_t id=-1;
    if (!m_Config->validate(m_Config->context,ticket,id) || id<0 || id>=static_cast<int32_t>(m_Ids.size()) ||
        std::find(m_Ids.begin(),m_Ids.begin()+m_Count,id)==m_Ids.begin()+m_Count) return {};
    auto* unit=static_cast<uint8_t*>(m_Sites->table)+8+static_cast<size_t>(id)*0xf8;
    NativeState before;
    if (!ReadState(ticket,id,unit,before)) return {};
    if (!before.held() && !before.enabled()) return {}; // Conflicting native hold sources/layout.
    auto jobs=NpcActionProbe::Inspect(unit);
    if (!jobs.healthy) return {};
    // Resuming may not revive an undrained operation. The owning native adapter
    // must also prove its canonical baseline and arbitrate pre-existing holds.
    if (!held && (!before.drained() || jobs.outstanding)) return {State::Pending,false,before.held()};
    if ((held && before.held() && before.drained() && !jobs.outstanding) || (!held && before.enabled()))
        return {State::Complete,false,before.held()};
    if (m_Config->arbitrateHolds) {
        if (!NpcHoldArbiter::InstalledFor(m_Sites->actorControl) || !NpcHoldArbiter::Invoke(ticket,held))
            return {State::Refused,true,before.held()};
    } else reinterpret_cast<ActorControl>(m_Sites->actorControl)(ticket.actor,held);
    int32_t current=-1;
    NativeState after;
    if (!ActiveHere() || !m_Config->validate(m_Config->context,ticket,current) || current!=id || !ReadState(ticket,id,unit,after))
        return {State::Refused,true,before.held()};
    jobs=NpcActionProbe::Inspect(unit);
    if (!jobs.healthy) return {State::Refused,true,before.held()};
    const bool complete=held ? after.held() && after.drained() && !jobs.outstanding : after.enabled() && !jobs.outstanding;
    return {complete?State::Complete:State::Pending,true,before.held()};
}

bool NpcActionPhase::Install(const CrashGuards::NpcBehaviorSites& sites,Config config,MinHookDiagnostics::LayoutTag<sizeof(Config)>) {
    std::lock_guard lock(g_Install);
    if (!g_Healthy || !sites || !config.callback || !config.gameThread || config.gameThread!=GetCurrentThreadId()) return false;
    if (config.allowControl ? (!config.validate || !NpcActionProbe::InstalledFor(sites.actions.job)) : !config.observe) return false;
    if (g_Installed) {
        std::array<uint8_t,15> actual;
        return sites.actions.loop==g_Entry && sites.actorControl==g_Sites.actorControl && sites.table==g_Sites.table &&
            config.context==g_Config.context && config.validate==g_Config.validate && config.callback==g_Config.callback && config.gameThread==g_Config.gameThread &&
            config.observe==g_Config.observe && config.allowControl==g_Config.allowControl && config.arbitrateHolds==g_Config.arbitrateHolds &&
            Read(g_Entry,actual.data(),actual.size()) && actual==Patch();
    }
    if ((g_Entry && sites.actions.loop!=g_Entry) || g_Callers.load() || !Executable(sites.actions.loop) || !Executable(sites.actorControl)) return false;
    std::array<uint8_t,15> bytes;
    if (!Read(sites.actions.loop,bytes.data(),bytes.size()) || bytes!=Prologue) return false;
    if (!g_Trampoline) {
        auto* code=static_cast<uint8_t*>(VirtualAlloc(nullptr,64,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
        if (!code) return false;
        std::memcpy(code,bytes.data(),bytes.size());
        const auto back=Jump(static_cast<uint8_t*>(sites.actions.loop)+bytes.size());
        std::memcpy(code+bytes.size(),back.data(),back.size());
        DWORD old=0;
        if (!VirtualProtect(code,64,PAGE_EXECUTE_READ,&old)) { VirtualFree(code,0,MEM_RELEASE); return false; }
        FlushInstructionCache(GetCurrentProcess(),code,64);
        g_Trampoline=code; g_Entry=sites.actions.loop;
        g_Original.store(reinterpret_cast<Loop>(code),std::memory_order_release);
    }
    g_Sites=sites; g_Config=config;
    const auto patch=Patch();
    if (!Write(g_Entry,patch.data(),patch.size())) return false;
    g_Installed=true; return true;
}
bool NpcActionPhase::Remove() {
    std::lock_guard lock(g_Install);
    if (!g_Installed) return true;
    if (g_Callers.load() || (g_Config.allowControl && NpcActionProbe::Diagnostics().outstanding)) return false;
    std::array<uint8_t,15> actual;
    if (!Read(g_Entry,actual.data(),actual.size()) || actual!=Patch() || !Write(g_Entry,Prologue.data(),Prologue.size())) return false;
    g_Installed=false; return true;
}
bool NpcActionPhase::Installed() { return g_Installed; }
bool NpcActionPhase::Healthy() {
    std::lock_guard lock(g_Install);
    return g_Installed && g_Healthy && (!g_Config.allowControl || NpcActionProbe::InstalledFor(g_Sites.actions.job));
}
