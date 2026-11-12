#include "NpcReasoningFence.h"
#include "CrashGuards.h"
#include "TestFramework.h"
#include <Windows.h>
#include <array>
#include <atomic>
#include <cstring>
#include <thread>
#include <chrono>

namespace {
using Loop=void (*)(void*,const void*,uint32_t);
struct State {
    const void* observed=nullptr;
    uint32_t tick=0;
    uint32_t padding=0;
    void (*callback)(State*,const void*,uint32_t)=nullptr;
    std::atomic<uint32_t> calls=0;
    std::atomic<bool> block=false,release=false,attemptHold=false,heldInside=false;
};
void Callback(State* state,const void*,uint32_t) {
    ++state->calls;
    if (state->attemptHold) state->heldInside=bool(NpcReasoningFence::TryHold());
    while (state->block && !state->release) std::this_thread::yield();
}
// One executable entry for the process, matching the verified native prologue.
// The body records the three ABI arguments and calls a controllable fixture.
uint8_t* Entry() {
    static auto* code=[] {
        auto* data=static_cast<uint8_t*>(VirtualAlloc(nullptr,4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
        if (!data) return data;
        const uint8_t bytes[] {
            0x48,0x89,0x5c,0x24,0x10, 0x48,0x89,0x6c,0x24,0x18, 0x48,0x89,0x74,0x24,0x20,
            0x48,0x89,0x11, // mov [rcx],rdx
            0x44,0x89,0x41,0x08, // mov [rcx+8],r8d
            0x48,0x8b,0x41,0x10, // mov rax,[rcx+16] callback
            0x48,0x83,0xec,0x28, 0xff,0xd0, 0x48,0x83,0xc4,0x28, 0xc3
        };
        std::memcpy(data,bytes,sizeof(bytes));
        DWORD previous=0;
        if (!VirtualProtect(data,4096,PAGE_EXECUTE_READ,&previous)) { VirtualFree(data,0,MEM_RELEASE); return static_cast<uint8_t*>(nullptr); }
        FlushInstructionCache(GetCurrentProcess(),data,sizeof(bytes));
        return data;
    }();
    return code;
}
struct Fixture {
    State state;
    Fixture() { state.callback=&Callback; }
    ~Fixture() { NpcReasoningFence::Remove(); }
    void Run(const void* ids,uint32_t tick) { reinterpret_cast<Loop>(Entry())(&state,ids,tick); }
};
template<class F> bool Wait(F check) {
    const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(2);
    while (!check() && std::chrono::steady_clock::now()<end) std::this_thread::yield();
    return check();
}
}

TEST(NpcReasoningFence_RealDetourPreservesAllThreeNativeArgumentsAndRestoresEntry) {
    Fixture fixture;
    REQUIRE(Entry());
    CHECK(!NpcReasoningFence::Install(nullptr));
    CHECK(!NpcReasoningFence::Install(reinterpret_cast<void*>(1)));
    auto* wrong=VirtualAlloc(nullptr,4096,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE);
    REQUIRE(wrong);
    std::memset(wrong,0x90,15);
    CHECK(!NpcReasoningFence::Install(wrong));
    VirtualFree(wrong,0,MEM_RELEASE);
    REQUIRE(NpcReasoningFence::Install(Entry()));
    REQUIRE(NpcReasoningFence::Installed());
    const int ids=17;
    fixture.Run(&ids,0x87654321);
    CHECK(fixture.state.observed==&ids);
    CHECK(fixture.state.tick==0x87654321);
    CHECK(fixture.state.calls==1);
    const auto stats=NpcReasoningFence::Diagnostics();
    CHECK(stats.entered==stats.completed);
    CHECK(stats.callers==0);
    {
        auto hold=NpcReasoningFence::TryHold(); CHECK(bool(hold));
        CHECK(NpcReasoningFence::HeldByCurrentThread());
        CHECK(!NpcReasoningFence::TryHold());
        CHECK(!NpcReasoningFence::Remove());
    }
    CHECK(!NpcReasoningFence::HeldByCurrentThread());
    CHECK(NpcReasoningFence::Remove());
    CHECK(!NpcReasoningFence::TryHold());
    fixture.Run(&ids,88);
    CHECK(fixture.state.tick==88);
    CHECK(fixture.state.calls==2);
}

TEST(NpcReasoningFence_ActiveNativeCallCannotBeAcknowledgedAsHeldOrRemoved) {
    Fixture fixture;
    REQUIRE(Entry()); REQUIRE(NpcReasoningFence::Install(Entry()));
    fixture.state.block=true;
    std::thread worker([&] { fixture.Run(nullptr,42); });
    CHECK(Wait([&] { return fixture.state.calls.load()==1; }));
    CHECK(!NpcReasoningFence::TryHold());
    CHECK(!NpcReasoningFence::Remove());
    fixture.state.release=true;
    worker.join();
    { auto hold=NpcReasoningFence::TryHold(); CHECK(bool(hold)); }
    CHECK(NpcReasoningFence::Remove());
}

TEST(NpcReasoningFence_ExclusiveHoldPreventsTheNextNativeCallFromStarting) {
    Fixture fixture;
    REQUIRE(Entry()); REQUIRE(NpcReasoningFence::Install(Entry()));
    auto hold=NpcReasoningFence::TryHold();
    REQUIRE(bool(hold));
    const auto entered=NpcReasoningFence::Diagnostics().entered;
    std::thread worker([&] { fixture.Run(nullptr,43); });
    CHECK(Wait([&] { return NpcReasoningFence::Diagnostics().entered>entered; }));
    CHECK(fixture.state.calls==0);
    hold={};
    worker.join();
    CHECK(fixture.state.calls==1);
    CHECK(NpcReasoningFence::Remove());
}

TEST(NpcReasoningFence_ReentrantHoldRefusesWithoutDeadlockingNativeCall) {
    Fixture fixture;
    REQUIRE(Entry()); REQUIRE(NpcReasoningFence::Install(Entry()));
    fixture.state.attemptHold=true;
    fixture.Run(nullptr,44);
    CHECK(!fixture.state.heldInside);
    CHECK(NpcReasoningFence::Remove());
}

TEST(NpcReasoningFence_UnreadableOrDifferentEntryCannotBePatched) {
    CHECK(!NpcReasoningFence::Install(nullptr));
    CHECK(!NpcReasoningFence::Install(reinterpret_cast<void*>(1)));
    std::array<uint8_t,15> wrong {};
    CHECK(!NpcReasoningFence::Install(wrong.data()));
    CHECK(!NpcReasoningFence::Installed());
}

TEST(NpcReasoningFence_ConfirmedRegistryHoldAndResumeUseThePublishedSkipList) {
    Fixture fixture;
    std::array<uint8_t,1024> units {};
    std::array<uint8_t,64> actor {};
    int32_t runtimeId=3;
    std::memcpy(actor.data()+8,&runtimeId,4);
    CrashGuardsTest::Configure(units.data());
    REQUIRE(NpcReasoningFence::Install(Entry()));
    CHECK(CrashGuards::TrySetNpcReasoningHeld(actor.data(),true));
    const auto list=CrashGuardsTest::SkippedUnits();
    CHECK(list.size()==1);
    CHECK(!list.empty() && list[0]==units.data()+3*32);
    CHECK(CrashGuards::TrySetNpcReasoningHeld(actor.data(),false));
    CHECK(CrashGuardsTest::SkippedUnits().empty());
    CHECK(NpcReasoningFence::Remove());
    CHECK(!CrashGuards::TrySetNpcReasoningHeld(actor.data(),true));
    CrashGuardsTest::Configure(nullptr);
}

TEST(NpcReasoningFence_QueuedLegacyRegistryChangeIsNotACompletedFreeze) {
    Fixture fixture;
    std::array<uint8_t,1024> units {};
    std::array<uint8_t,64> actor {};
    int32_t runtimeId=2;
    std::memcpy(actor.data()+8,&runtimeId,4);
    CrashGuardsTest::Configure(units.data());
    REQUIRE(NpcReasoningFence::Install(Entry()));
    fixture.state.block=true;
    std::thread worker([&] { fixture.Run(nullptr,55); });
    CHECK(Wait([&] { return fixture.state.calls.load()==1; }));
    CHECK(!CrashGuards::TrySetNpcReasoningHeld(actor.data(),true));
    CrashGuards::SetPuppet(actor.data(),true); // Existing API records/queues it.
    CHECK(CrashGuards::IsPuppet(actor.data()));
    CHECK(CrashGuardsTest::SkippedUnits().empty()); // Actual publication not complete.
    CHECK(!CrashGuards::FlushNpcReasoning());
    fixture.state.release=true;
    worker.join();
    CHECK(CrashGuards::FlushNpcReasoning());
    CHECK(CrashGuardsTest::SkippedUnits().size()==1);
    CHECK(CrashGuards::TrySetNpcReasoningHeld(actor.data(),false));
    CHECK(NpcReasoningFence::Remove());
    CrashGuardsTest::Configure(nullptr);
}

TEST(NpcReasoningFence_NpcRegistryCannotControlRegisteredPlayerActors) {
    Fixture fixture;
    std::array<uint8_t,1024> units {};
    std::array<uint8_t,64> actor {};
    int32_t runtimeId=1;
    std::memcpy(actor.data()+8,&runtimeId,4);
    CrashGuardsTest::Configure(units.data());
    REQUIRE(NpcReasoningFence::Install(Entry()));
    REQUIRE(CrashGuards::SetPlayerActor(3,103,actor.data()));
    CHECK(!CrashGuards::TrySetNpcReasoningHeld(actor.data(),true));
    CHECK(!CrashGuards::IsPuppet(actor.data()));
    CHECK(CrashGuards::ClearPlayer(3,103));
    CHECK(CrashGuards::TrySetNpcReasoningHeld(actor.data(),true));
    CHECK(CrashGuards::TrySetNpcReasoningHeld(actor.data(),false));
    CHECK(NpcReasoningFence::Remove());
    CrashGuardsTest::Configure(nullptr);
}
