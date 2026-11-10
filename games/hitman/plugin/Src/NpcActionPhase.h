#pragma once
#include "CrashGuards.h"
#include "NpcMutationGate.h"
#include "NpcBehaviorReadback.h"
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
// Observation validates SDK identity/birth without enabling a mutation scope.
using ObserveValidate=bool (*)(void*,const void* actor,uint64_t npc,uint64_t birth,int32_t& runtimeId);
struct Observation {
    bool valid=false;
    int32_t runtimeId=-1;
    NpcBehaviorReadback::Snapshot native;
};
class Boundary {
public:
    Boundary(const Boundary&)=delete;
    Boundary& operator=(const Boundary&)=delete;
    Result SetHeld(const NpcMutationGate::Ticket&,bool held);
    Observation Observe(const void* actor,uint64_t npc,uint64_t birth) const;
private:
    Boundary()=default;
    bool ActiveHere() const;
    bool m_Active=false;
    uintptr_t m_Stack=0;
    size_t m_Count=0;
    std::array<int32_t,500> m_Ids; // SDK 4.2.1 native actor slots; crowd proxies are separate.
    friend struct Bridge;
};
using Callback=void (*)(void*,Boundary&);
struct Config {
    void* context=nullptr;
    Validate validate=nullptr;
    Callback callback=nullptr;
    uint32_t gameThread=0; // Captured from the SDK game update, not a network/render thread.
    ObserveValidate observe=nullptr;
    bool allowControl=true; // False: observation only; SetHeld always refuses.
};
// Install/remove only at an externally established native-quiescent point.
// Control mode requires the corresponding job probe installed first. The
// observation-only mode does not install a job probe or enable mutation gates.
// Config lifetime spans
// every native invocation; stop producers and drain before removing/unloading.
// Resolve the complete site bundle before patching its job entry with the probe.
bool Install(const CrashGuards::NpcBehaviorSites&,Config);
bool Remove();
bool Installed();
bool Healthy();
}
