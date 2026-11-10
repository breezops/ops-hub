#pragma once
#include "NpcActorLifetimeSites.h"
#include "MinHookDiagnostics.h"

// Pointer-only destructor ENTRY observation. The body and optional deleting
// wrapper/free still execute normally. No actor retirement acknowledgement.
namespace NpcActorDestructionObserver {
using Invalidate=uint64_t (*)(void* context,void* actor) noexcept;
struct Stats {
    uint64_t entered=0,returned=0,invalidated=0,unknown=0;
    bool installed=false,active=false;
};
struct Report {
    const char* stage="not-attempted";
    int status=0;
    MinHookDiagnostics::Report allocations;
};
bool Install(const NpcActorLifetimeSites::Sites&,void* context,Invalidate);
// Outside client callback. Drains only pointer callbacks, not native bodies.
// Retains the bridge/trampoline/module until process exit.
void Stop();
Stats Diagnostics();
Report Installation(MinHookDiagnostics::LayoutTag<sizeof(Report)> = {});
}
