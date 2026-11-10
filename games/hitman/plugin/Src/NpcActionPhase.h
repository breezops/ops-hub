#pragma once
#include "CrashGuards.h"
#include "NpcMutationGate.h"
#include <array>

// Internal native operation primitive, not an IPartyNpcWorld implementation.
// No automatic installation. Full scene preparation also needs pool admission,
// damage/interaction control, external script-hold arbitration and retirement.
namespace NpcActionPhase {
enum class State { Refused, Pending, Complete };
struct Result { State state=State::Refused; bool changed=false, previouslyHeld=false; };
// Must validate a fresh SDK actor identity/birth and pin its native lifetime for
// this boundary. It must reject players, wrong scenes and unlocked actor lists.
using Validate=bool (*)(void*,const NpcMutationGate::Ticket&,int32_t& runtimeId);
class Boundary {
public:
    Boundary(const Boundary&)=delete;
    Boundary& operator=(const Boundary&)=delete;
    Result SetHeld(const NpcMutationGate::Ticket&,bool held);
private:
    Boundary()=default;
    bool m_Active=false;
    uintptr_t m_Stack=0;
    size_t m_Count=0;
    std::array<int32_t,8192> m_Ids;
    friend struct Bridge;
};
using Callback=void (*)(void*,Boundary&);
struct Config { void* context=nullptr; Validate validate=nullptr; Callback callback=nullptr; };
// Install/remove only at an externally established native-quiescent point.
// Requires the corresponding job probe installed first. Config lifetime spans
// every native invocation; stop producers and drain before removing/unloading.
// Resolve the complete site bundle before patching its job entry with the probe.
bool Install(const CrashGuards::NpcBehaviorSites&,Config);
bool Remove();
bool Installed();
bool Healthy();
}
