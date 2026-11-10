#include "NpcActionObserver.h"
#include <Windows.h>
#include <MinHook.h>
#include <algorithm>
#include <atomic>
#include <cstring>
#include <mutex>

namespace {
using Loop=void (*)(void*,const void*);
std::mutex g_Install,g_Callback;
std::atomic<bool> g_Attempted=false,g_Installed=false,g_Active=false,g_Healthy=true;
std::atomic<uint64_t> g_Entered=0,g_Callbacks=0,g_Busy=0;
std::atomic<Loop> g_Original=nullptr;
NpcActionObserver::Config g_Config;
void* g_Table=nullptr;
constexpr std::array<uint8_t,15> Prologue{
    0x40,0x56,0x41,0x54,0x41,0x55,0x41,0x56,0x48,0x83,0xec,0x38,0x4c,0x8b,0x02
};
bool Read(const void* at,void* out,size_t size) {
    SIZE_T count=0;
    return at && ReadProcessMemory(GetCurrentProcess(),at,out,size,&count) && count==size;
}
uintptr_t Stack() { return reinterpret_cast<uintptr_t>(reinterpret_cast<const NT_TIB*>(NtCurrentTeb())->StackBase); }
}
struct NpcActionObserver::Bridge {
    static void Run(void* table,const void* span) {
        ++g_Entered;
        if (g_Installed.load(std::memory_order_acquire) && g_Active && g_Healthy) {
            std::unique_lock lock(g_Callback,std::try_to_lock);
            if (!lock.owns_lock()) ++g_Busy;
            else if (g_Active && g_Healthy) {
                if (GetCurrentThreadId()!=g_Config.gameThread) g_Healthy=false;
                else {
                    View view;
                    std::array<uintptr_t,2> bounds{};
                    bool valid=table==g_Table && Read(span,bounds.data(),sizeof(bounds)) &&
                        bounds[1]>=bounds[0] && (bounds[1]-bounds[0])%4==0 &&
                        (bounds[1]-bounds[0])/4<=view.m_Ids.size();
                    view.m_Count=valid?(bounds[1]-bounds[0])/4:0;
                    valid=valid && (!view.m_Count || Read(reinterpret_cast<const void*>(bounds[0]),view.m_Ids.data(),view.m_Count*4));
                    std::array<bool,500> seen{};
                    if (valid) for (size_t i=0;i<view.m_Count;++i) {
                        const auto id=view.m_Ids[i];
                        if (id<0 || id>=500 || seen[id]) { valid=false; break; }
                        seen[id]=true;
                    }
                    if (!valid) g_Healthy=false;
                    else {
                        view.m_Stack=Stack(); view.m_Active=true;
                        ++g_Callbacks;
                        try { g_Config.callback(g_Config.context,view); }
                        catch (...) { g_Healthy=false; }
                        view.m_Active=false;
                    }
                }
            }
        }
        // No thread-owned lock spans native scheduling or a yielding job.
        if (const auto original=g_Original.load(std::memory_order_acquire)) original(table,span);
        else g_Healthy=false;
    }
};
bool NpcActionObserver::View::ActiveHere() const {
    const auto* tib=reinterpret_cast<const NT_TIB*>(NtCurrentTeb());
    const auto self=reinterpret_cast<uintptr_t>(this);
    return m_Active && g_Active && g_Healthy && GetCurrentThreadId()==g_Config.gameThread && Stack()==m_Stack &&
        self>=reinterpret_cast<uintptr_t>(tib->StackLimit) && self<m_Stack;
}
bool NpcActionObserver::View::Contains(int32_t id) const {
    return ActiveHere() && id>=0 && id<500 && std::find(m_Ids.begin(),m_Ids.begin()+m_Count,id)!=m_Ids.begin()+m_Count;
}
NpcActionObserver::Observation NpcActionObserver::View::Observe(const void* actor,uint64_t npc,uint64_t birth) const {
    if (!ActiveHere() || !actor || !npc || !birth) return {};
    int32_t id=-1;
    if (!g_Config.validate(g_Config.context,actor,npc,birth,id) || !Contains(id)) return {};
    NpcBehaviorReadback::Snapshot sample;
    if (!NpcBehaviorReadback::Read(g_Table,actor,id,sample)) return {};
    int32_t current=-1;
    if (!g_Config.validate(g_Config.context,actor,npc,birth,current) || current!=id) return {};
    return {true,id,sample};
}
bool NpcActionObserver::Install(const CrashGuards::NpcBehaviorSites& sites,Config config) {
    std::lock_guard install(g_Install);
    if (!sites || !config.callback || !config.validate || !config.gameThread || config.gameThread!=GetCurrentThreadId() || g_Attempted) return false;
    std::array<uint8_t,15> bytes{};
    if (!Read(sites.actions.loop,bytes.data(),bytes.size()) || bytes!=Prologue) return false;
    // A late native continuation can still return through Bridge after Stop.
    // Keep executable code and its immutable configuration alive until exit.
    HMODULE module=nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
        reinterpret_cast<LPCWSTR>(&Bridge::Run),&module)) return false;
    g_Attempted=true;
    const auto init=MH_Initialize();
    if (init!=MH_OK && init!=MH_ERROR_ALREADY_INITIALIZED) return false;
    void* original=nullptr;
    if (MH_CreateHook(sites.actions.loop,reinterpret_cast<void*>(&Bridge::Run),&original)!=MH_OK || !original) return false;
    g_Config=config; g_Table=sites.table;
    g_Original.store(reinterpret_cast<Loop>(original),std::memory_order_release);
    // Publish before enabling. A worker that reaches the hook early can refuse
    // observation by its thread ID but always has a valid native continuation.
    g_Active=true; g_Installed.store(true,std::memory_order_release);
    if (MH_EnableHook(sites.actions.loop)!=MH_OK) { g_Active=false; g_Installed=false; return false; }
    return true;
}
void NpcActionObserver::Stop() {
    std::lock_guard lock(g_Callback);
    g_Active=false;
    // Do not remove/uninitialize MinHook or free original/trampoline/module.
}
NpcActionObserver::Stats NpcActionObserver::Diagnostics() {
    return {g_Entered.load(),g_Callbacks.load(),g_Busy.load(),g_Installed.load(),g_Active.load(),g_Healthy.load()};
}
