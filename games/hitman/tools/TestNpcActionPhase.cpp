#include "NpcActionPhase.h"
#include "NpcActionProbe.h"
#include "NativeAlternateStack.h"
#include "TestFramework.h"
#include <Windows.h>
#include <array>
#include <cstring>

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
    bool valid=true,hold=true,pinObserved=false,removeRefused=false;
    bool failCleanup=false,noNativeChange=false,throwCallback=false,wrongStack=false,invalidateOnCall=false;
    bool canceled=false,converted=false;
    void* mainFiber=nullptr;
    void* jobFiber=nullptr;
    Result result;
    Boundary* active=nullptr;
    Fixture() {
        Put(actor.data(),0,this); Put(table.data(),0,this);
        Put<int32_t>(actor.data(),0x119c,0);
        Put<uintptr_t>(Unit(),8,reinterpret_cast<uintptr_t>(actor.data()));
        scope=NpcMutationGate::Begin();
        if (scope && NpcMutationGate::Bind(scope,55,actor.data(),1,1)) ticket=NpcMutationGate::Find(scope,55,1,1);
    }
    ~Fixture() {
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
        return {{PhaseEntry(),JobEntry(),reinterpret_cast<void*>(&NativeToggle),reinterpret_cast<void*>(&NativeToggle),reinterpret_cast<void*>(&NativeToggle)},reinterpret_cast<void*>(&NativeToggle),table.data()};
    }
    static bool Validate(void* opaque,const NpcMutationGate::Ticket& ticket,int32_t& id) {
        auto& f=*static_cast<Fixture*>(opaque); id=0;
        return f.valid && ticket.actor==f.actor.data();
    }
    static void WrongStack(void* opaque) {
        auto& f=*static_cast<Fixture*>(opaque); f.result=f.active->SetHeld(f.ticket,f.hold);
    }
    static void Callback(void* opaque,Boundary& boundary) {
        auto& f=*static_cast<Fixture*>(opaque); ++f.callbacks;
        f.removeRefused=!NpcActionPhase::Remove();
        if (f.throwCallback) throw 1;
        if (f.wrongStack) {
            AlternateStack other;
            if (!other.code || !other.stack) return;
            f.active=&boundary; other.Run(&WrongStack,&f); f.active=nullptr;
        } else f.result=boundary.SetHeld(f.ticket,f.hold);
    }
    bool Install() {
        return ticket.actor && PhaseEntry() && JobEntry() && NpcActionProbe::Install(JobEntry()) &&
            NpcActionPhase::Install(Sites(),{this,&Validate,&Callback});
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
    f->pinObserved=NpcMutationGate::Begin()==0 && !NpcMutationGate::SetLocalOwner(f->scope,55,1,1,true);
    if (f->noNativeChange) return;
    f->actor[0x117d]=(f->actor[0x117d]&~0x40)|(held?0x40:0);
    f->Unit()[0x2c]=(f->Unit()[0x2c]&~1)|(held?1:0);
    if (held && f->jobFiber && !f->canceled) { f->canceled=true; SwitchToFiber(f->jobFiber); }
    if (held && !f->failCleanup) for (size_t at:{size_t(0xa0),size_t(0xe0),size_t(0xe8)}) Put<uintptr_t>(f->Unit(),at,0);
    if (f->invalidateOnCall) f->valid=false;
}
}

TEST(NpcActionPhase_RequiresMatchingProbeAndRestoresTheNativeLoop) {
    Fixture f; REQUIRE(PhaseEntry()); REQUIRE(JobEntry());
    std::array<uint8_t,15> before{}; std::memcpy(before.data(),PhaseEntry(),before.size());
    CHECK(!NpcActionPhase::Install(f.Sites(),{&f,&Fixture::Validate,&Fixture::Callback}));
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
    CHECK(f.nativeCalls==0); CHECK(f.loopCalls==6);
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

// Fault status deliberately latches for the process. Keep this after the other
// phase fixtures; a production fault requires teardown/restart, not silent reuse.
TEST(NpcActionPhase_CallbackFailureStillRunsTheNativeLoopAndLatchesUnhealthy) {
    Fixture f; REQUIRE(f.Install()); f.throwCallback=true;
    f.Run(); CHECK(!NpcActionPhase::Healthy()); CHECK(f.loopCalls==1);
    f.throwCallback=false; f.Run(); CHECK(f.loopCalls==2); CHECK(f.callbacks==1);
    CHECK(f.nativeCalls==0);
    REQUIRE(NpcActionPhase::Remove()); CHECK(!f.Install());
}
