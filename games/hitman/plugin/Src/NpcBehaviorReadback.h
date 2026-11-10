#pragma once
#include <cstdint>

// Read-only inspection of the verified 3.290.1 behavior table. This neither
// installs a hook nor calls actor control. Successful reads are observations,
// not worker-drain, object-lifetime or safe-mutation acknowledgements.
namespace NpcBehaviorReadback {
struct Snapshot {
    bool actorHeld=false, unitHeld=false;
    uintptr_t action=0, task=0, job=0;
    bool coherent() const { return actorHeld==unitHeld; }
    bool held() const { return actorHeld && unitHeld; }
    bool drained() const { return !action && !task && !job; }
};
// Caller supplies a freshly validated SDK actor and table from the verified
// resolver. The range is SDK native actor slots, not the crowd proxy range.
// Reads use ReadProcessMemory and refuse inaccessible or mismatched memory.
bool Read(const void* table,const void* actor,int32_t runtimeId,Snapshot& out);
}
