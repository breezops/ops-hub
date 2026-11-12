#pragma once
#include "NpcActorLifetimeSites.h"
#include "MinHookDiagnostics.h"

// Pointer-only destructor ENTRY observation. The body and optional deleting
// wrapper/free still execute normally. No actor retirement acknowledgement.
namespace NpcActorDestructionObserver {
using Invalidate=uint64_t (*)(void* context,void* actor) noexcept;
using Complete=bool (*)(void* context,const void* actor,uint64_t birth) noexcept;
struct Stats {
    uint64_t entered=0,returned=0,invalidated=0,unknown=0;
    bool installed=false,active=false;
    uint64_t completed=0,completionRejected=0;
};
struct Report {
    const char* stage="not-attempted";
    int status=0;
    MinHookDiagnostics::Report allocations;
};
bool Install(const NpcActorLifetimeSites::Sites&,void* context,Invalidate,Complete=nullptr);
// Outside client callback. Drains only pointer callbacks, not native bodies.
// Retains the bridge/trampoline/module until process exit.
void Stop();
Stats Diagnostics(MinHookDiagnostics::LayoutTag<sizeof(Stats)> = {});
Report Installation(MinHookDiagnostics::LayoutTag<sizeof(Report)> = {});
}
