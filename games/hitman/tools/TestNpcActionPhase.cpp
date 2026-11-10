#include "NpcActionPhase.h"
#include "NpcActionProbe.h"
#include "NpcActionObserver.h"
#include "NpcHoldArbiter.h"
#include "NativeAlternateStack.h"
#include "TestFramework.h"
#include <Windows.h>
#include <array>
#include <cstring>
#include <thread>

namespace {
using namespace NpcActionPhase;
struct Fixture;
void PhaseBody(void*,const void*);
struct Task { void** table; Fixture* fixture; };
uint64_t Step(Task*);
uint8_t* Code(const uint8_t* bytes,size_t size) {
    auto* out=static_cast<uint8_t*>(VirtualAlloc(nullptr,4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
    if (!out) return out;
    std::memcpy(out,bytes,size); DWORD old=0;
    if (!VirtualProtect(out,4096,PAGE_EXECUTE_READ,&old)) { VirtualFree(out,0,MEM_RELEASE); return nullptr; }
    FlushInstructionCache(GetCurrentProcess(),out,size); return out;
}
uint8_t* PhaseEntry() {
    static auto* code=[] {
        std::array<uint8_t,40> bytes{
            0x40,0x56,0x41,0x54,0x41,0x55,0x41,0x56,0x48,0x83,0xec,0x38,0x4c,0x8b,0x02,
            0x48,0xb8,0,0,0,0,0,0,0,0,0xff,0xd0,
            0x48,0x83,0xc4,0x38,0x41,0x5e,0x41,0x5d,0x41,0x5c,0x5e,0xc3
        };
        auto* target=reinterpret_cast<void*>(&PhaseBody);
        std::memcpy(bytes.data()+17,&target,8); return Code(bytes.data(),39);
    }(); return code;
}
uint8_t* JobEntry() {
    static auto* code=[] {
        const uint8_t bytes[]{
            0x40,0x53,0x48,0x83,0xec,0x20,0x48,0x8b,0xd9,0x48,0x8b,0x89,0xe0,0,0,0,
            0x48,0x8b,0x01,0xff,0x50,0x08,0x48,0x89,0x83,0xd8,0,0,0,
            0x48,0x83,0xc4,0x20,0x5b,0xc3
        }; return Code(bytes,sizeof(bytes));
    }(); return code;
}
template<class T> void Put(uint8_t* p,size_t at,T value) { std::memcpy(p+at,&value,sizeof(value)); }
void NativeToggle(void*,bool);
void* ActorEntry() {
    static auto* code=[] {
        std::array<uint8_t,33> bytes{0x48,0x89,0x5c,0x24,0x08,0x57,0x48,0x83,0xec,0x20,
            0x48,0xb8,0,0,0,0,0,0,0,0,0xff,0xd0,0x48,0x8b,0x5c,0x24,0x30,0x48,0x83,0xc4,0x20,0x5f,0xc3};
        auto* body=reinterpret_cast<void*>(&NativeToggle); std::memcpy(bytes.data()+12,&body,8);
        return Code(bytes.data(),bytes.size());
    }(); return code;
}
struct Fixture {
    std::array<uint8_t,0x1200> actor{};
    std::array<uint8_t,8+4*0xf8> table{};
    std::array<int32_t,2> ids{0,1};
    struct Span { const int32_t* begin; const int32_t* end; } span{ids.data(),ids.data()+ids.size()};
    void* taskTable[2]{nullptr,reinterpret_cast<void*>(&Step)};
    Task task{taskTable,this};
    NpcMutationGate::Scope scope=0;
    NpcMutationGate::Ticket ticket;
    unsigned nativeCalls=0,loopCalls=0,callbacks=0;
    int32_t validationId=0;
    bool valid=true,hold=true,pinObserved=false,removeRefused=false;
    bool failCleanup=false,noNativeChange=false,throwCallback=false,wrongStack=false,invalidateOnCall=false;
    bool canceled=false,converted=false;
    bool observeOnly=false,observationIdentity=true,changeObservationIdentity=false;
    unsigned observationChecks=0;
    Observation observation;
    void* mainFiber=nullptr;
    void* jobFiber=nullptr;
    Result result;
    Boundary* active=nullptr;
    bool stopInside=false,stopWaited=false;
    bool arbitrate=false;
    bool externalToggle=false;
    std::atomic<bool> stopDone=false;
    std::thread stopper;
    Fixture() {
        Put(actor.data(),0,this); Put(table.data(),0,this);
        Put<int32_t>(actor.data(),0x119c,0);
        Put<uintptr_t>(Unit(),8,reinterpret_cast<uintptr_t>(actor.data()));
        scope=NpcMutationGate::Begin();
        if (scope && NpcMutationGate::Bind(scope,55,actor.data(),1,1)) ticket=NpcMutationGate::Find(scope,55,1,1);
    }
    ~Fixture() {
#ifdef COOP_RETAINED_CONTROL_TESTS
        NpcActionObserver::Stop();
        if (stopper.joinable()) stopper.join();
#endif
        if (jobFiber) {
            if (!canceled) { canceled=true; SwitchToFiber(jobFiber); }
            DeleteFiber(jobFiber);
        }
        if (converted) ConvertFiberToThread();
        NpcActionPhase::Remove(); NpcActionProbe::Remove();
        NpcMutationGate::Clear(scope);
    }
    uint8_t* Unit() { return table.data()+8; }
    CrashGuards::NpcBehaviorSites Sites() {
        // Synthetic native targets. Real production installation must use the
        // full verified resolver bundle and a live SDK lifetime validator.
        return {{PhaseEntry(),JobEntry(),reinterpret_cast<void*>(&NativeToggle),reinterpret_cast<void*>(&NativeToggle),reinterpret_cast<void*>(&NativeToggle)},arbitrate?ActorEntry():reinterpret_cast<void*>(&NativeToggle),table.data()};
    }
    static bool Validate(void* opaque,const NpcMutationGate::Ticket& ticket,int32_t& id) {
        auto& f=*static_cast<Fixture*>(opaque); id=f.validationId;
        return f.valid && ticket.actor==f.actor.data();
    }
    static void WrongStack(void* opaque) {
        auto& f=*static_cast<Fixture*>(opaque); f.result=f.active->SetHeld(f.ticket,f.hold);
        if (f.observeOnly) f.observation=f.active->Observe(f.actor.data(),55,1);
    }
    static bool ObserveIdentity(void* opaque,const void* actor,uint64_t npc,uint64_t birth,int32_t& id) {
        auto& f=*static_cast<Fixture*>(opaque); id=f.validationId;
        ++f.observationChecks;
        return f.observationIdentity && actor==f.actor.data() && npc==55 && birth==1 &&
            (!f.changeObservationIdentity || f.observationChecks%2==1);
    }
    static void Callback(void* opaque,Boundary& boundary) {
        auto& f=*static_cast<Fixture*>(opaque); ++f.callbacks;
#ifdef COOP_RETAINED_CONTROL_TESTS
        if (f.stopInside) {
            f.stopper=std::thread([&f]{NpcActionObserver::Stop(); f.stopDone=true;});
            const auto until=GetTickCount64()+2000;
            while (NpcActionObserver::Diagnostics().active && GetTickCount64()<until) std::this_thread::yield();
            f.stopWaited=!NpcActionObserver::Diagnostics().active && !f.stopDone;
            return;
        }
#endif
        f.removeRefused=!NpcActionPhase::Remove();
        if (f.throwCallback) throw 1;
        if (f.wrongStack) {
            AlternateStack other;
            if (!other.code || !other.stack) return;
            f.active=&boundary; other.Run(&WrongStack,&f); f.active=nullptr;
        } else {
            f.result=boundary.SetHeld(f.ticket,f.hold);
            if (f.observeOnly) f.observation=boundary.Observe(f.actor.data(),55,1);
        }
    }
    bool Install() {
#ifdef COOP_RETAINED_CONTROL_TESTS
        if (arbitrate && (!NpcHoldArbiter::Install(Sites()) || !NpcHoldArbiter::Watch(actor.data()))) return false;
        return ticket.actor && PhaseEntry() && JobEntry() && NpcActionProbe::InstallRetained(JobEntry()) &&
            NpcActionObserver::Install(Sites(),{this,&ObserveIdentity,
                [](void*,const NpcActionObserver::View&){},GetCurrentThreadId(),&Validate,&Callback,arbitrate});
#else
        return ticket.actor && PhaseEntry() && JobEntry() && NpcActionProbe::Install(JobEntry()) &&
            NpcActionPhase::Install(Sites(),{this,&Validate,&Callback,GetCurrentThreadId()});
#endif
    }
    void Run() { reinterpret_cast<void(*)(void*,const void*)>(PhaseEntry())(table.data(),&span); }
    static void WINAPI JobFiber(void* opaque) {
        auto& f=*static_cast<Fixture*>(opaque);
        reinterpret_cast<void(*)(void*)>(JobEntry())(f.Unit());
        SwitchToFiber(f.mainFiber);
    }
    bool StartSuspendedJob() {
        const bool wasFiber=IsThreadAFiber()!=FALSE;
        mainFiber=wasFiber?GetCurrentFiber():ConvertThreadToFiber(nullptr);
        converted=mainFiber && !wasFiber;
        if (!mainFiber) return false;
        jobFiber=CreateFiber(0,&JobFiber,this);
        if (!jobFiber) return false;
        Put<uintptr_t>(Unit(),0xa0,1);
        Put<uintptr_t>(Unit(),0xe0,reinterpret_cast<uintptr_t>(&task));
        Put<uintptr_t>(Unit(),0xe8,reinterpret_cast<uintptr_t>(jobFiber));
        SwitchToFiber(jobFiber);
        return NpcActionProbe::Inspect(Unit()).outstanding==1;
    }
};
void PhaseBody(void* table,const void*) {
    Fixture* f=nullptr; std::memcpy(&f,table,sizeof(f)); ++f->loopCalls;
}
uint64_t Step(Task* task) {
    auto& f=*task->fixture;
    SwitchToFiber(f.mainFiber); // Native job stays pending while its scheduler regains control.
    f.pinObserved=f.pinObserved && NpcMutationGate::Begin()==0;
    return f.canceled?1:0;
}
void NativeToggle(void* actor,bool held) {
    Fixture* f=nullptr; std::memcpy(&f,actor,sizeof(f)); ++f->nativeCalls;
    if (!f->externalToggle) f->pinObserved=NpcMutationGate::Begin()==0 && !NpcMutationGate::SetLocalOwner(f->scope,55,1,1,true);
    if (f->noNativeChange) return;
    f->actor[0x117d]=(f->actor[0x117d]&~0x40)|(held?0x40:0);
    f->Unit()[0x2c]=(f->Unit()[0x2c]&~1)|(held?1:0);
    if (held && f->jobFiber && !f->canceled) { f->canceled=true; SwitchToFiber(f->jobFiber); }
    if (held && !f->failCleanup) for (size_t at:{size_t(0xa0),size_t(0xe0),size_t(0xe8)}) Put<uintptr_t>(f->Unit(),at,0);
    if (f->invalidateOnCall) f->valid=false;
}
}

#ifdef COOP_RETAINED_CONTROL_TESTS
TEST(RetainedControl_CancelsYieldedJobAndResumes) {
    Fixture f; REQUIRE(f.Install()); REQUIRE(f.StartSuspendedJob());
    f.Run(); CHECK(f.result.state==State::Complete); CHECK(f.result.changed);
    CHECK(f.canceled && f.pinObserved); CHECK(NpcActionProbe::Diagnostics().outstanding==0);
    REQUIRE(NpcMutationGate::SetLocalOwner(f.scope,55,1,1,true));
    f.hold=false; f.Run(); CHECK(f.result.state==State::Complete);
    CHECK(!(f.actor[0x117d]&0x40)); CHECK(!(f.Unit()[0x2c]&1));
    CHECK(f.loopCalls==2); CHECK(NpcActionObserver::Diagnostics().healthy);
}
TEST(RetainedControl_RefusesIdentityAndWrongStack) {
    Fixture f; REQUIRE(f.Install());
    f.valid=false; f.Run(); CHECK(f.result.state==State::Refused); CHECK(f.nativeCalls==0);
    f.valid=true; f.wrongStack=true; f.Run(); CHECK(f.result.state==State::Refused); CHECK(f.nativeCalls==0);
    CHECK(f.loopCalls==2);
}
TEST(RetainedControl_StopDrainsWithoutHoldingObserverMutex) {
    Fixture f; REQUIRE(f.Install()); f.stopInside=true; f.Run();
    if (f.stopper.joinable()) f.stopper.join();
    CHECK(f.stopWaited); CHECK(f.stopDone); CHECK(f.callbacks==1);
    f.Run(); CHECK(f.callbacks==1); CHECK(f.loopCalls==2);
}
TEST(RetainedControl_ArbitratedYieldedCycle) {
    Fixture f; f.arbitrate=true; REQUIRE(f.Install()); REQUIRE(f.StartSuspendedJob());
    f.Run(); CHECK(f.result.state==State::Complete); CHECK(f.canceled && f.pinObserved);
    CHECK(NpcHoldArbiter::OwnsHold()); CHECK(NpcHoldArbiter::ExternalCalls()==0);
    REQUIRE(NpcMutationGate::SetLocalOwner(f.scope,55,1,1,true));
    f.hold=false; f.Run(); CHECK(f.result.state==State::Complete);
    CHECK(!NpcHoldArbiter::OwnsHold()); CHECK(f.nativeCalls==2);
}
TEST(RetainedControl_ArbitratedScriptCollisionRefusesResume) {
    Fixture f; f.arbitrate=true; REQUIRE(f.Install()); f.Run();
    REQUIRE(f.result.state==State::Complete);
    f.externalToggle=true;
    reinterpret_cast<void(*)(void*,bool)>(ActorEntry())(f.actor.data(),true);
    f.externalToggle=false;
    REQUIRE(NpcMutationGate::SetLocalOwner(f.scope,55,1,1,true));
    f.hold=false; f.Run(); CHECK(f.result.state==State::Refused);
    CHECK(f.nativeCalls==2); CHECK(f.actor[0x117d]&0x40);
    CHECK(NpcHoldArbiter::ExternalCalls()==1);
}
#else
TEST(NpcActionPhase_RequiresMatchingProbeAndRestoresTheNativeLoop) {
    Fixture f; REQUIRE(PhaseEntry()); REQUIRE(JobEntry());
    std::array<uint8_t,15> before{}; std::memcpy(before.data(),PhaseEntry(),before.size());
    CHECK(!NpcActionPhase::Install(f.Sites(),{&f,&Fixture::Validate,&Fixture::Callback,GetCurrentThreadId()}));
    REQUIRE(NpcActionProbe::Install(JobEntry()));
    CHECK(!NpcActionPhase::Install(f.Sites(),{&f,&Fixture::Validate,&Fixture::Callback}));
    CHECK(!NpcActionPhase::Install(f.Sites(),{&f,&Fixture::Validate,&Fixture::Callback,GetCurrentThreadId()+1}));
    REQUIRE(f.Install()); CHECK(NpcActionPhase::Healthy());
    f.Run(); CHECK(f.result.state==State::Complete); CHECK(f.result.changed);
    CHECK(f.nativeCalls==1); CHECK(f.loopCalls==1); CHECK(f.pinObserved); CHECK(f.removeRefused);
    CHECK(f.actor[0x117d]&0x40); CHECK(f.Unit()[0x2c]&1);
    REQUIRE(NpcActionPhase::Remove());
    CHECK(std::memcmp(before.data(),PhaseEntry(),before.size())==0);
    f.Run(); CHECK(f.loopCalls==2); CHECK(f.callbacks==1);
}

TEST(NpcActionPhase_RefusesInvalidIdentityAndDoesNotSkipNativeScheduling) {
    Fixture f; REQUIRE(f.Install());
    f.valid=false; f.Run(); CHECK(f.result.state==State::Refused); CHECK(f.nativeCalls==0);
    f.valid=true; ++f.ticket.birth; f.Run(); CHECK(f.result.state==State::Refused); CHECK(f.nativeCalls==0);
    --f.ticket.birth;
    Put<int32_t>(f.actor.data(),0x119c,1); f.Run(); CHECK(f.result.state==State::Refused);
    Put<int32_t>(f.actor.data(),0x119c,0);
    f.ids={2,3}; f.Run(); CHECK(f.result.state==State::Refused);
    f.ids={0,1}; f.actor[0x117d]=0x40; f.Run(); CHECK(f.result.state==State::Refused);
    f.actor[0x117d]=0; Put<uintptr_t>(f.Unit(),8,1); f.Run(); CHECK(f.result.state==State::Refused);
    f.validationId=500; f.ids={500,1}; Put<int32_t>(f.actor.data(),0x119c,500);
    f.Run(); CHECK(f.result.state==State::Refused);
    CHECK(f.nativeCalls==0); CHECK(f.loopCalls==7);
}

TEST(NpcActionPhase_NativeReturnWithoutMatchingStateOrCleanupRemainsPending) {
    Fixture f; REQUIRE(f.Install());
    f.noNativeChange=true; f.Run(); CHECK(f.result.state==State::Pending);
    f.noNativeChange=false; f.failCleanup=true; Put<uintptr_t>(f.Unit(),0xa0,1);
    f.Run(); CHECK(f.result.state==State::Pending);
    f.failCleanup=false; f.Run(); CHECK(f.result.state==State::Complete);
    f.Run(); CHECK(f.result.state==State::Complete); CHECK(!f.result.changed); CHECK(f.result.previouslyHeld);
    REQUIRE(NpcMutationGate::SetLocalOwner(f.scope,55,1,1,true));
    f.hold=false; f.invalidateOnCall=true; f.Run();
    CHECK(f.result.state==State::Refused); CHECK(f.result.changed);
}

TEST(NpcActionPhase_ResumeRequiresLocalOwnershipAndAnAlreadyDrainedAction) {
    Fixture f; REQUIRE(f.Install()); f.Run(); REQUIRE(f.result.state==State::Complete);
    f.hold=false; f.Run(); CHECK(f.result.state==State::Refused);
    REQUIRE(NpcMutationGate::SetLocalOwner(f.scope,55,1,1,true));
    Put<uintptr_t>(f.Unit(),0xe8,1); f.Run(); CHECK(f.result.state==State::Pending);
    Put<uintptr_t>(f.Unit(),0xe8,0); f.Run(); CHECK(f.result.state==State::Complete);
    CHECK(f.result.changed); CHECK(f.result.previouslyHeld);
    CHECK(!(f.actor[0x117d]&0x40)); CHECK(!(f.Unit()[0x2c]&1));
}

TEST(NpcActionPhase_OtherExecutionStackCannotUseTheBoundaryPermission) {
    Fixture f; REQUIRE(f.Install()); f.wrongStack=true;
    f.Run(); CHECK(f.result.state==State::Refused); CHECK(f.nativeCalls==0); CHECK(f.loopCalls==1);
}

TEST(NpcActionPhase_CancellationResumesAndDrainsTheOriginalSuspendedJob) {
    Fixture f; REQUIRE(f.Install()); REQUIRE(f.StartSuspendedJob());
    CHECK(!NpcActionPhase::Remove());
    f.Run();
    CHECK(f.canceled); CHECK(f.pinObserved); CHECK(f.result.state==State::Complete);
    CHECK(NpcActionProbe::Inspect(f.Unit()).outstanding==0);
    CHECK(f.Unit()[0x2c]&1); CHECK(f.loopCalls==1);
}

TEST(NpcActionPhase_ReadOnlyBoundaryNeedsNoProbeOrMutationScopeAndCannotControl) {
    Fixture f;
    REQUIRE(NpcMutationGate::Clear(f.scope)); // No active mutation gate during this diagnostic.
    f.scope=0; f.observeOnly=true;
    REQUIRE(!NpcActionProbe::Installed());
    Config config{&f,nullptr,&Fixture::Callback,GetCurrentThreadId(),&Fixture::ObserveIdentity,false};
    REQUIRE(NpcActionPhase::Install(f.Sites(),config));
    CHECK(NpcActionPhase::Healthy());
    const auto actor=f.actor; const auto table=f.table;
    f.Run();
    CHECK(f.observation.valid && f.observation.runtimeId==0);
    CHECK(f.observation.native.coherent() && !f.observation.native.held());
    CHECK(f.result.state==State::Refused && f.nativeCalls==0 && f.loopCalls==1);
    CHECK(f.actor==actor && f.table==table && !NpcMutationGate::CurrentScope());
    CHECK(!NpcActionProbe::Installed());
    auto changed=config; changed.allowControl=true;
    CHECK(!NpcActionPhase::Install(f.Sites(),changed)); // Cannot silently upgrade a live diagnostic.
    f.actor[0x117d]=0x40; // Observation reports a mismatch; it never repairs it.
    f.Run(); CHECK(f.observation.valid && !f.observation.native.coherent());
    f.observationIdentity=false; f.Run(); CHECK(!f.observation.valid);
    f.observationIdentity=true; f.changeObservationIdentity=true; f.observationChecks=0;
    f.Run(); CHECK(!f.observation.valid); // Identity must still match after native reads.
    CHECK(f.nativeCalls==0 && f.loopCalls==4);
}

TEST(NpcActionPhase_ReadOnlyBoundaryRejectsOtherStacksAndUnscheduledActors) {
    Fixture f; f.observeOnly=true;
    Config config{&f,nullptr,&Fixture::Callback,GetCurrentThreadId(),&Fixture::ObserveIdentity,false};
    auto missing=config; missing.observe=nullptr;
    CHECK(!NpcActionPhase::Install(f.Sites(),missing));
    REQUIRE(NpcActionPhase::Install(f.Sites(),config));
    f.wrongStack=true; f.Run(); CHECK(!f.observation.valid && f.observationChecks==0);
    f.wrongStack=false; f.ids={2,3}; f.Run(); CHECK(!f.observation.valid);
    f.ids={0,1}; Put<uintptr_t>(f.Unit(),8,1); f.Run(); CHECK(!f.observation.valid);
    CHECK(f.nativeCalls==0 && f.loopCalls==3);
}

// Fault status deliberately latches for the process. Keep this after the other
// phase fixtures; a production fault requires teardown/restart, not silent reuse.
TEST(NpcActionPhase_CallbackFailureStillRunsTheNativeLoopAndLatchesUnhealthy) {
    Fixture f; REQUIRE(f.Install()); f.throwCallback=true;
    f.Run(); CHECK(!NpcActionPhase::Healthy()); CHECK(f.loopCalls==1);
    f.throwCallback=false; f.Run(); CHECK(f.loopCalls==2); CHECK(f.callbacks==1);
    CHECK(f.nativeCalls==0);
    REQUIRE(NpcActionPhase::Remove()); CHECK(!f.Install());
}
#endif
