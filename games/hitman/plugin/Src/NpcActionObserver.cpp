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
NpcActionObserver::InstallReport g_InstallReport;
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
    const auto refuse=[](InstallStage stage,int status=0,uint32_t error=0) {
        g_InstallReport.stage=stage; g_InstallReport.hookStatus=status; g_InstallReport.lastError=error; return false;
    };
    if (!sites || !config.callback || !config.validate || !config.gameThread || config.gameThread!=GetCurrentThreadId()) return refuse(InstallStage::InvalidConfig);
    if (g_Attempted) return refuse(InstallStage::AlreadyAttempted);
    std::array<uint8_t,15> bytes{};
    if (!Read(sites.actions.loop,bytes.data(),bytes.size())) return refuse(InstallStage::EntryRead,0,GetLastError());
    if (bytes!=Prologue) return refuse(InstallStage::EntryMismatch);
    // A late native continuation can still return through Bridge after Stop.
    // Keep executable code and its immutable configuration alive until exit.
    HMODULE module=nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
        reinterpret_cast<LPCWSTR>(&Bridge::Run),&module)) return refuse(InstallStage::ModulePin,0,GetLastError());
    g_Attempted=true;
    const auto init=MH_Initialize();
    if (init!=MH_OK && init!=MH_ERROR_ALREADY_INITIALIZED) return refuse(InstallStage::LibraryInit,init,GetLastError());
    void* original=nullptr;
    const auto created=MinHookDiagnostics::Create(sites.actions.loop,reinterpret_cast<void*>(&Bridge::Run),&original,g_InstallReport.allocations);
    if (created!=MH_OK) return refuse(InstallStage::HookCreate,created,GetLastError());
    if (!original) return refuse(InstallStage::MissingOriginal);
    g_Config=config; g_Table=sites.table;
    g_Original.store(reinterpret_cast<Loop>(original),std::memory_order_release);
    // Publish before enabling. A worker that reaches the hook early can refuse
    // observation by its thread ID but always has a valid native continuation.
    g_Active=true; g_Installed.store(true,std::memory_order_release);
    const auto enabled=MH_EnableHook(sites.actions.loop);
    if (enabled!=MH_OK) { g_Active=false; g_Installed=false; return refuse(InstallStage::HookEnable,enabled,GetLastError()); }
    g_InstallReport.stage=InstallStage::Installed; g_InstallReport.hookStatus=0; g_InstallReport.lastError=0;
    return true;
}
NpcActionObserver::InstallReport NpcActionObserver::Installation() {
    std::lock_guard lock(g_Install); return g_InstallReport;
}
const char* NpcActionObserver::StageName(InstallStage stage) {
    switch (stage) {
    case InstallStage::NotAttempted: return "not-attempted";
    case InstallStage::InvalidConfig: return "invalid-config";
    case InstallStage::AlreadyAttempted: return "already-attempted";
    case InstallStage::EntryRead: return "entry-read";
    case InstallStage::EntryMismatch: return "entry-mismatch";
    case InstallStage::ModulePin: return "module-pin";
    case InstallStage::LibraryInit: return "library-init";
    case InstallStage::HookCreate: return "hook-create";
    case InstallStage::MissingOriginal: return "missing-original";
    case InstallStage::HookEnable: return "hook-enable";
    case InstallStage::Installed: return "installed";
    }
    return "unknown";
}
void NpcActionObserver::Stop() {
    std::lock_guard lock(g_Callback);
    g_Active=false;
    // Do not remove/uninitialize MinHook or free original/trampoline/module.
}
NpcActionObserver::Stats NpcActionObserver::Diagnostics() {
    return {g_Entered.load(),g_Callbacks.load(),g_Busy.load(),g_Installed.load(),g_Active.load(),g_Healthy.load()};
}
