#pragma once
#include "CrashGuards.h"
#include "NpcBehaviorReadback.h"
#include "MinHookDiagnostics.h"
#include "NpcActionPhase.h"
#include <array>
#include <span>

// Process-lifetime native bridge. Observation by default; an explicitly wired
// private control phase may use the existing validated NpcActionPhase boundary.
// Never filters native scheduling. No controls in public/default builds.
namespace NpcActionObserver {
using Validate=bool (*)(void*,const void*,uint64_t,uint64_t,int32_t&);
struct Observation { bool valid=false; int32_t runtimeId=-1; NpcBehaviorReadback::Snapshot native; };
class View {
public:
    View(const View&)=delete;
    View& operator=(const View&)=delete;
    bool Contains(int32_t runtimeId) const;
    Observation Observe(const void* actor,uint64_t npc,uint64_t birth) const;
    size_t Count() const { return m_Count; }
private:
    View()=default;
    bool ActiveHere() const;
    bool m_Active=false;
    uintptr_t m_Stack=0;
    size_t m_Count=0;
    std::array<int32_t,500> m_Ids{};
    friend struct Bridge;
};
using Callback=void (*)(void*,const View&);
struct Config {
    void* context=nullptr; Validate validate=nullptr; Callback callback=nullptr; uint32_t gameThread=0;
    // Optional private control phase. Runs after releasing the observer mutex.
    NpcActionPhase::Validate controlValidate=nullptr;
    NpcActionPhase::Callback control=nullptr;
    bool arbitrateHolds=false;
};
struct Stats { uint64_t entered=0,callbacks=0,busy=0; bool installed=false,active=false,healthy=false; };
enum class InstallStage { NotAttempted, InvalidConfig, AlreadyAttempted, EntryRead, EntryMismatch, ModulePin, LibraryInit, HookCreate, MissingOriginal, HookEnable, Installed };
struct InstallReport { InstallStage stage=InstallStage::NotAttempted; int hookStatus=0; uint32_t lastError=0; MinHookDiagnostics::Report allocations; };
InstallReport Installation(MinHookDiagnostics::LayoutTag<sizeof(InstallReport)> = {});
const char* StageName(InstallStage);
// Uses MinHook's thread-aware entry patching. Requires the complete untouched
// verified site bundle and the already observed SDK game-update thread.
// One installation attempt per process. Pins this module and retains original
// native execution/trampoline until process exit, including after SDK unload.
bool Install(const CrashGuards::NpcBehaviorSites&,Config,MinHookDiagnostics::LayoutTag<sizeof(Config)> = {});
// Called outside either callback. Drains the read callback and any optional
// control callback, including its native continuations, before client teardown.
// Never waits for the original loop. Leaves a pinned,
// pass-through hook; no live code removal or trampoline freeing is attempted.
void Stop();
Stats Diagnostics();
}
