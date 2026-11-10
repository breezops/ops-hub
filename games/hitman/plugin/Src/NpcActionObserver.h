#pragma once
#include "CrashGuards.h"
#include "NpcBehaviorReadback.h"
#include <array>
#include <span>

// Process-lifetime, observation-only native bridge. No mutation gate, action
// probe, actor control, cancellation or filtering of native scheduling.
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
struct Config { void* context=nullptr; Validate validate=nullptr; Callback callback=nullptr; uint32_t gameThread=0; };
struct Stats { uint64_t entered=0,callbacks=0,busy=0; bool installed=false,active=false,healthy=false; };
// Uses MinHook's thread-aware entry patching. Requires the complete untouched
// verified site bundle and the already observed SDK game-update thread.
// One installation attempt per process. Pins this module and retains original
// native execution/trampoline until process exit, including after SDK unload.
bool Install(const CrashGuards::NpcBehaviorSites&,Config);
// Waits only for an executing read-only callback, never for the original native
// loop/job. After return the client context can be destroyed. Leaves a pinned,
// pass-through hook; no live code removal or trampoline freeing is attempted.
void Stop();
Stats Diagnostics();
}
