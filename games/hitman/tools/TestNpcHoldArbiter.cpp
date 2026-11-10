#include "NpcHoldArbiter.h"
#include "TestFramework.h"
#include <Windows.h>
#include <array>
#include <cstring>

namespace {
struct Actor { unsigned calls=0; bool held=false,reenter=false; void* entry=nullptr; Actor* recursiveTarget=nullptr; };
void Body(void* opaque,bool held) {
    auto& a=*static_cast<Actor*>(opaque); ++a.calls; a.held=held;
    if (a.reenter) { a.reenter=false; reinterpret_cast<void(*)(void*,bool)>(a.entry)(a.recursiveTarget?a.recursiveTarget:&a,true); }
}
void* Entry() {
    const uint8_t bytes[]{0x48,0x89,0x5c,0x24,0x08,0x57,0x48,0x83,0xec,0x20,
        0x48,0xb8,0,0,0,0,0,0,0,0,0xff,0xd0,0x48,0x8b,0x5c,0x24,0x30,0x48,0x83,0xc4,0x20,0x5f,0xc3};
    auto* code=static_cast<uint8_t*>(VirtualAlloc(nullptr,4096,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));
    if (!code) return nullptr;
    std::memcpy(code,bytes,sizeof(bytes)); auto body=&Body; std::memcpy(code+12,&body,8);
    DWORD old=0; VirtualProtect(code,4096,PAGE_EXECUTE_READ,&old); FlushInstructionCache(GetCurrentProcess(),code,sizeof(bytes));return code;
}
struct Fixture {
    void* entry=Entry(); Actor actor,other;
    NpcMutationGate::Scope scope=0;
    NpcMutationGate::Ticket first,second;
    Fixture() { actor.entry=other.entry=entry; }
    bool Install() {
        CrashGuards::NpcBehaviorSites sites{{entry,entry,entry,entry,entry},entry,entry};
        return entry && NpcHoldArbiter::Install(sites) && NpcHoldArbiter::Watch(&actor);
    }
    void External(Actor& a,bool held) { reinterpret_cast<void(*)(void*,bool)>(entry)(&a,held); }
    bool InstallPool() {
        scope=NpcMutationGate::Begin();
        if (!scope || !NpcMutationGate::Bind(scope,55,&actor,1,1) || !NpcMutationGate::Bind(scope,56,&other,2,1)) return false;
        if (!NpcMutationGate::SetLocalOwner(scope,55,1,1,true) || !NpcMutationGate::SetLocalOwner(scope,56,2,1,true)) return false;
        first=NpcMutationGate::Find(scope,55,1,1);second=NpcMutationGate::Find(scope,56,2,1);
        CrashGuards::NpcBehaviorSites sites{{entry,entry,entry,entry,entry},entry,entry};
        return entry && NpcHoldArbiter::Install(sites) && NpcHoldArbiter::Watch(first) && NpcHoldArbiter::Watch(second);
    }
    ~Fixture() { if(scope) NpcMutationGate::Clear(scope); }
};
}
TEST(HoldArbiter_OwnCallAndUnwatchedActors) {
    Fixture f; REQUIRE(f.Install());
    CHECK(!NpcHoldArbiter::Invoke(&f.actor,false));
    REQUIRE(NpcHoldArbiter::Invoke(&f.actor,true)); CHECK(f.actor.held);
    f.External(f.other,true); CHECK(f.other.held); CHECK(NpcHoldArbiter::ExternalCalls()==0);
    REQUIRE(NpcHoldArbiter::Invoke(&f.actor,false)); CHECK(!f.actor.held);
    CHECK(f.actor.calls==2); CHECK(NpcHoldArbiter::ExternalCalls()==0);
    CHECK(!NpcHoldArbiter::Watch(&f.other));
}
TEST(HoldArbiter_ExternalHoldCannotBeResumedByController) {
    Fixture f; REQUIRE(f.Install()); REQUIRE(NpcHoldArbiter::Invoke(&f.actor,true));
    f.External(f.actor,true); CHECK(f.actor.held);
    CHECK(NpcHoldArbiter::ExternalCalls()==1);
    CHECK(!NpcHoldArbiter::Invoke(&f.actor,false)); CHECK(f.actor.held); CHECK(f.actor.calls==2);
}
TEST(HoldArbiter_RecursiveScriptCallIsExternal) {
    Fixture f; REQUIRE(f.Install()); f.actor.reenter=true;
    REQUIRE(NpcHoldArbiter::Invoke(&f.actor,true));
    CHECK(NpcHoldArbiter::ExternalCalls()==1); CHECK(f.actor.calls==2);
    CHECK(!NpcHoldArbiter::Invoke(&f.actor,false)); CHECK(f.actor.held);
}
TEST(HoldArbiter_PoolKeepsIndependentOwnersAndScriptHolds) {
    Fixture f;REQUIRE(f.InstallPool());
    REQUIRE(NpcHoldArbiter::Invoke(f.first,true));REQUIRE(NpcHoldArbiter::Invoke(f.second,true));
    CHECK(NpcHoldArbiter::OwnsHold(f.first) && NpcHoldArbiter::OwnsHold(f.second));
    f.External(f.actor,true);
    CHECK(NpcHoldArbiter::ExternalCalls(f.first)==1);CHECK(!NpcHoldArbiter::ExternalCalls(f.second));
    CHECK(!NpcHoldArbiter::Invoke(f.first,false));CHECK(f.actor.held);
    CHECK(NpcHoldArbiter::Invoke(f.second,false));CHECK(!f.other.held);
    CHECK(!NpcHoldArbiter::OwnsHold(f.first));CHECK(NpcHoldArbiter::Watch(f.second));
    CHECK(!NpcHoldArbiter::Watch(&f.actor));CHECK(!NpcHoldArbiter::Invoke(&f.actor,false));
}
TEST(HoldArbiter_PoolNestedOtherActorIsExternalOnlyForThatActor) {
    Fixture f;REQUIRE(f.InstallPool());f.actor.reenter=true;f.actor.recursiveTarget=&f.other;
    REQUIRE(NpcHoldArbiter::Invoke(f.first,true));
    CHECK(!NpcHoldArbiter::ExternalCalls(f.first));CHECK(NpcHoldArbiter::ExternalCalls(f.second)==1);
    CHECK(f.other.held);CHECK(!NpcHoldArbiter::OwnsHold(f.second));
    CHECK(!NpcHoldArbiter::Invoke(f.second,false));CHECK(f.other.held);
    CHECK(NpcHoldArbiter::Invoke(f.first,false));CHECK(!f.actor.held);
}
TEST(HoldArbiter_PoolRefusesRetiredReusedAndStaleLeaseBindings) {
    Fixture f;REQUIRE(f.InstallPool());REQUIRE(NpcHoldArbiter::Invoke(f.first,true));
    NpcHoldArbiter::Invalidate(&f.actor,999);CHECK(NpcHoldArbiter::OwnsHold(f.first));
    NpcHoldArbiter::Invalidate(&f.actor,1);CHECK(!NpcHoldArbiter::OwnsHold(f.first));
    CHECK(!NpcHoldArbiter::Invoke(f.first,false));CHECK(f.actor.held);
    REQUIRE(NpcMutationGate::Retire(f.scope,55,1));REQUIRE(NpcMutationGate::Bind(f.scope,55,&f.actor,3,1));
    CHECK(!NpcHoldArbiter::Watch(NpcMutationGate::Find(f.scope,55,3,1)));
    REQUIRE(NpcMutationGate::Bind(f.scope,56,&f.other,2,2));
    CHECK(!NpcHoldArbiter::Invoke(f.second,true));
    CHECK(!NpcHoldArbiter::Watch(NpcMutationGate::Find(f.scope,56,2,2)));
    const auto next=NpcMutationGate::Begin();REQUIRE(next);Actor fresh;
    REQUIRE(NpcMutationGate::Bind(next,70,&fresh,4,1));
    CHECK(!NpcHoldArbiter::Watch(NpcMutationGate::Find(next,70,4,1)));NpcMutationGate::Clear(next);
}
TEST(HoldArbiter_PoolCapacityDoesNotEvictEarlierWatches) {
    Fixture f;REQUIRE(f.InstallPool());std::array<Actor,NpcHoldArbiter::Capacity-1> actors{};
    for(size_t i=0;i<actors.size();++i) {
        REQUIRE(NpcMutationGate::Bind(f.scope,100+i,&actors[i],100+i,1));
        const auto t=NpcMutationGate::Find(f.scope,100+i,100+i,1);
        CHECK(NpcHoldArbiter::Watch(t)==(i<actors.size()-1));
    }
    CHECK(NpcHoldArbiter::Invoke(f.first,true));CHECK(NpcHoldArbiter::Invoke(f.first,false));
}
TEST(HoldArbiter_PoolResumeRequiresCurrentOwnerGrant) {
    Fixture f;REQUIRE(f.InstallPool());REQUIRE(NpcHoldArbiter::Invoke(f.first,true));
    REQUIRE(NpcMutationGate::SetLocalOwner(f.scope,55,1,1,false));
    CHECK(!NpcHoldArbiter::Invoke(f.first,false));CHECK(f.actor.held && f.actor.calls==1);
    REQUIRE(NpcMutationGate::SetLocalOwner(f.scope,55,1,1,true));
    CHECK(NpcHoldArbiter::Invoke(f.first,false));CHECK(!f.actor.held && f.actor.calls==2);
}
TEST(HoldArbiter_PoolRecursiveCollisionRefusesOwnershipAndResume) {
    Fixture f;REQUIRE(f.InstallPool());f.actor.reenter=true;
    CHECK(!NpcHoldArbiter::Invoke(f.first,true));CHECK(f.actor.held && f.actor.calls==2);
    CHECK(NpcHoldArbiter::ExternalCalls(f.first)==1);CHECK(!NpcHoldArbiter::OwnsHold(f.first));
    CHECK(!NpcHoldArbiter::Invoke(f.first,false));CHECK(f.actor.held && f.actor.calls==2);
}
