#pragma once
#include "CrashGuards.h"
#include "MinHookDiagnostics.h"
#include "NpcMutationGate.h"
#include <cstdint>

// Private native hold-control observation: legacy one-actor mode and an opt-in
// fixed-scope pool. Observed interference permanently refuses controller calls
// for that actor. Game calls always pass through; only our marked call may be
// refused. This does not establish all hold writers or cross-thread ordering.
namespace NpcHoldArbiter {
bool Install(const CrashGuards::NpcBehaviorSites&);
bool Watch(void* actor); // One actor per process; never rebind a reused address.
bool Invoke(void* actor,bool held); // Only our marked call may bypass collision tracking.
bool InstalledFor(const void* entry);
uint64_t ExternalCalls();
bool OwnsHold();
// Private pool prototype: fixed scope/lease and immutable process-lifetime
// watches. Does not mix with Watch(void*), replace an address or follow a new
// scene. Callers still validate native lifetime/phase before every operation.
constexpr size_t Capacity=500;
bool Watch(const NpcMutationGate::Ticket&);
bool Invoke(const NpcMutationGate::Ticket&,bool held);
uint64_t ExternalCalls(const NpcMutationGate::Ticket&);
bool OwnsHold(const NpcMutationGate::Ticket&);
void Invalidate(const void* actor,uint64_t birth); // Pointer-only destructor ingress.
struct Report { const char* stage="not-attempted"; int status=0; MinHookDiagnostics::Report allocations; };
Report Installation(MinHookDiagnostics::LayoutTag<sizeof(Report)> = {});
}
