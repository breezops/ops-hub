#pragma once
#include <cstdint>
#include "MinHookDiagnostics.h"

// Observation only: this does not block scheduling, cancel a job or freeze an NPC.
// The native action may yield onto another stack before its function returns.
namespace NpcActionProbe {
struct Stats {
    uint64_t entered=0, returned=0;
    uint32_t outstanding=0;
    bool installed=false, healthy=false;
};
struct UnitStats {
    uint32_t outstanding=0;
    bool observed=false, healthy=false;
};
// Verified 3.290.1 action-job entry; installation/removal additionally require
// external native quiescence. No automatic installation in the player preview.
bool Install(void* verifiedEntry);
// Thread-aware installation for live observation. Pins the bridge, hook and
// trampoline until process exit, including when a native continuation yields.
// No world/actor pointers are retained. Remove deliberately refuses this mode.
// A late attachment cannot account for jobs that entered before installation.
bool InstallRetained(void* verifiedEntry);
struct InstallReport {
    const char* stage="not-attempted";
    int hookStatus=0;
    uint32_t lastError=0;
    MinHookDiagnostics::Report allocations;
};
InstallReport Installation(MinHookDiagnostics::LayoutTag<sizeof(InstallReport)> = {});
bool Installed();
bool InstalledFor(const void* entry);
Stats Diagnostics();
// Only compares unit addresses; never reads native actor/unit memory. A zero
// count is NOT preparation proof: scheduling and other action paths remain open.
UnitStats Inspect(const void* unit);
// Cuts the observed entry stream under the records mutex. Later jobs, even
// at a reused unit address, cannot conceal an older invocation's return.
// This covers only entries seen by this probe, NOT pre-attachment work,
// unscheduled producers, actor destruction or an engine retirement barrier.
struct FenceState { uint32_t pending=0; bool valid=false,healthy=false; };
class Fence {
public:
    Fence()=default;
private:
    uint64_t m_Through=0;
    bool m_Captured=false;
    friend Fence CaptureFence();
    friend FenceState InspectFence(const Fence&);
};
Fence CaptureFence();
FenceState InspectFence(const Fence&);
bool Remove();
}
