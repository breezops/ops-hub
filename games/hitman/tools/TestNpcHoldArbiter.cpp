#include "NpcHoldArbiter.h"
#include "TestFramework.h"
#include <Windows.h>
#include <array>
#include <cstring>

namespace {
struct Actor { unsigned calls=0; bool held=false,reenter=false; void* entry=nullptr; };
void Body(void* opaque,bool held) {
    auto& a=*static_cast<Actor*>(opaque); ++a.calls; a.held=held;
    if (a.reenter) { a.reenter=false; reinterpret_cast<void(*)(void*,bool)>(a.entry)(&a,true); }
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
    Fixture() { actor.entry=other.entry=entry; }
    bool Install() {
        CrashGuards::NpcBehaviorSites sites{{entry,entry,entry,entry,entry},entry,entry};
        return entry && NpcHoldArbiter::Install(sites) && NpcHoldArbiter::Watch(&actor);
    }
    void External(Actor& a,bool held) { reinterpret_cast<void(*)(void*,bool)>(entry)(&a,held); }
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
