#pragma once
#include "CrashGuards.h"
#include "MinHookDiagnostics.h"
#include <cstdint>

// Private one-actor diagnostic arbitration. Observes every native hold-control
// call for the watched actor; never suppresses the game's calls. A collision
// permanently refuses controller resume, preserving any game/script-owned hold.
namespace NpcHoldArbiter {
bool Install(const CrashGuards::NpcBehaviorSites&);
bool Watch(void* actor); // One actor per process; never rebind a reused address.
bool Invoke(void* actor,bool held); // Only our marked call may bypass collision tracking.
bool InstalledFor(const void* entry);
uint64_t ExternalCalls();
bool OwnsHold();
struct Report { const char* stage="not-attempted"; int status=0; MinHookDiagnostics::Report allocations; };
Report Installation(MinHookDiagnostics::LayoutTag<sizeof(Report)> = {});
}
