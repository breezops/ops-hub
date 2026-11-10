#include "NpcActionProbe.h"
#include "CrashGuards.h"
#include "NativeAlternateStack.h"
#include "TestFramework.h"
#include <array>
#include <atomic>
#include <chrono>
#include <thread>

namespace {
struct Unit;
struct Action {
    void** vtable=nullptr;
    Unit* unit=nullptr;
    void (*callback)(Action*)=nullptr;
    uint64_t result=0;
};
struct Unit {
    std::array<uint8_t,0xd8> padding{};
    uint64_t result=0;
    Action* action=nullptr;
};
static_assert(offsetof(Unit,action)==0xe0);
uint64_t Step(Action* action) {
    if (action->callback) action->callback(action);
    return action->result;
}
uint8_t* Entry() {
    static auto* code=[] {
        auto* data=static_cast<uint8_t*>(VirtualAlloc(nullptr,4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
        if (!data) return data;
        const uint8_t bytes[]{
            0x40,0x53,0x48,0x83,0xec,0x20,0x48,0x8b,0xd9,0x48,0x8b,0x89,0xe0,0,0,0,
            0x48,0x8b,0x01,0xff,0x50,0x08, // action's virtual step
            0x48,0x89,0x83,0xd8,0,0,0, // store result in original unit
            0x48,0x83,0xc4,0x20,0x5b,0xc3
        };
        std::memcpy(data,bytes,sizeof(bytes));
        DWORD old=0;
        if (!VirtualProtect(data,4096,PAGE_EXECUTE_READ,&old)) { VirtualFree(data,0,MEM_RELEASE); return static_cast<uint8_t*>(nullptr); }
        FlushInstructionCache(GetCurrentProcess(),data,sizeof(bytes)); return data;
    }();
    return code;
}
struct Fixture {
    void* table[2]{nullptr,reinterpret_cast<void*>(&Step)};
    Unit unit;
    Action action{table,&unit,nullptr,0x9876543210abcdef};
    Fixture() { unit.action=&action; }
    ~Fixture() { NpcActionProbe::Remove(); }
    void Run() { reinterpret_cast<void(*)(void*)>(Entry())(&unit); }
};
template<class F> bool Wait(F check) {
    const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(2);
    while (!check() && std::chrono::steady_clock::now()<end) std::this_thread::yield();
    return check();
}
std::atomic<bool> entered=false,release=false;
void Block(Action*) { entered=true; while (!release) std::this_thread::yield(); }
std::atomic<uint32_t> concurrent=0;
void BlockConcurrent(Action*) { ++concurrent; while (!release) std::this_thread::yield(); }
bool reentrantObserved=false,removalRefused=false;
void InspectInside(Action* action) {
    const auto state=NpcActionProbe::Inspect(action->unit);
    reentrantObserved=state.healthy && state.observed && state.outstanding==1;
    removalRefused=!NpcActionProbe::Remove();
}
struct StackState {
    Fixture* nested=nullptr;
    Unit* outer=nullptr;
    bool outerPending=false,innerPending=false,refused=false;
};
StackState* stackState=nullptr;
AlternateStack* alternateStack=nullptr;
void InspectNested(Action* action) {
    const auto outer=NpcActionProbe::Inspect(stackState->outer);
    const auto inner=NpcActionProbe::Inspect(action->unit);
    stackState->outerPending=outer.healthy && outer.outstanding==1;
    stackState->innerPending=inner.healthy && inner.outstanding==1;
    stackState->refused=!NpcActionProbe::Remove();
}
void OnAlternate(void* opaque) { static_cast<StackState*>(opaque)->nested->Run(); }
void SwitchDuringAction(Action*) { alternateStack->Run(&OnAlternate,stackState); }
}

TEST(NpcActionProbe_CheckedWholeInstructionTrampolinePreservesUnitAndNativeResult) {
    Fixture f; REQUIRE(Entry());
    CHECK(!NpcActionProbe::Install(nullptr));
    CHECK(!NpcActionProbe::Install(reinterpret_cast<void*>(1)));
    CHECK(!NpcActionProbe::Diagnostics().healthy);
    std::array<uint8_t,16> before{}; std::memcpy(before.data(),Entry(),before.size());
    REQUIRE(NpcActionProbe::Install(Entry()));
    REQUIRE(NpcActionProbe::Install(Entry()));
    const auto start=NpcActionProbe::Diagnostics();
    f.Run();
    CHECK(f.unit.result==f.action.result);
    const auto end=NpcActionProbe::Diagnostics();
    CHECK(end.entered==start.entered+1); CHECK(end.returned==start.returned+1);
    CHECK(end.outstanding==0); CHECK(end.healthy);
    CHECK(!NpcActionProbe::Inspect(&f.unit).observed);
    REQUIRE(NpcActionProbe::Remove());
    CHECK(std::memcmp(before.data(),Entry(),before.size())==0);
    f.action.result=77; f.Run(); CHECK(f.unit.result==77);
    CHECK(NpcActionProbe::Diagnostics().entered==end.entered);
}

TEST(NpcActionProbe_NativeCallRemainsOutstandingUntilOriginalReturns) {
    Fixture f; REQUIRE(NpcActionProbe::Install(Entry()));
    entered=false; release=false; f.action.callback=&Block;
    std::thread worker([&]{f.Run();});
    CHECK(Wait([]{return entered.load();}));
    const auto live=NpcActionProbe::Inspect(&f.unit);
    CHECK(live.healthy); CHECK(live.outstanding==1);
    CHECK(NpcActionProbe::Diagnostics().outstanding==1);
    CHECK(!NpcActionProbe::Remove());
    release=true; worker.join();
    CHECK(NpcActionProbe::Inspect(&f.unit).outstanding==0);
    CHECK(NpcActionProbe::Diagnostics().outstanding==0);
}

TEST(NpcActionProbe_ReentrantDiagnosticsDoNotDeadlockNativeCallback) {
    Fixture f; REQUIRE(NpcActionProbe::Install(Entry()));
    reentrantObserved=false; removalRefused=false;
    f.action.callback=&InspectInside; f.Run();
    CHECK(reentrantObserved); CHECK(removalRefused);
}

TEST(NpcActionProbe_ConcurrentJobsHaveIndependentPendingCounts) {
    std::array<Fixture,3> jobs;
    REQUIRE(NpcActionProbe::Install(Entry()));
    release=false; concurrent=0;
    std::array<std::thread,3> workers;
    for (size_t i=0;i<jobs.size();++i) {
        jobs[i].action.callback=&BlockConcurrent;
        workers[i]=std::thread([&,i]{jobs[i].Run();});
    }
    CHECK(Wait([]{return concurrent.load()==3;}));
    for (const auto& job:jobs) CHECK(NpcActionProbe::Inspect(&job.unit).outstanding==1);
    CHECK(NpcActionProbe::Diagnostics().outstanding==3);
    CHECK(!NpcActionProbe::Remove());
    release=true; for (auto& worker:workers) worker.join();
    const auto done=NpcActionProbe::Diagnostics();
    CHECK(done.outstanding==0); CHECK(done.entered==done.returned); CHECK(done.healthy);
}

TEST(NpcActionProbe_CustomStackCanRunAnotherJobWhileOuterJobIsStillPending) {
    Fixture outer,inner; AlternateStack alternate;
    REQUIRE(alternate.stack && alternate.code);
    REQUIRE(NpcActionProbe::Install(Entry()));
    StackState state{&inner,&outer.unit};
    stackState=&state; alternateStack=&alternate;
    outer.action.callback=&SwitchDuringAction; inner.action.callback=&InspectNested;
    const auto start=NpcActionProbe::Diagnostics(); outer.Run();
    CHECK(state.outerPending); CHECK(state.innerPending); CHECK(state.refused);
    const auto end=NpcActionProbe::Diagnostics();
    CHECK(end.entered==start.entered+2); CHECK(end.returned==start.returned+2);
    CHECK(end.outstanding==0); CHECK(end.healthy);
    CHECK(outer.unit.result==outer.action.result); CHECK(inner.unit.result==inner.action.result);
    stackState=nullptr; alternateStack=nullptr;
}

TEST(NpcActionProbe_RejectsDifferentOrTamperedEntries) {
    Fixture f; REQUIRE(NpcActionProbe::Install(Entry()));
    auto* wrong=static_cast<uint8_t*>(VirtualAlloc(nullptr,4096,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE));
    REQUIRE(wrong); std::memset(wrong,0x90,16);
    CHECK(!NpcActionProbe::Install(wrong));
    VirtualFree(wrong,0,MEM_RELEASE);
    DWORD old=0; REQUIRE(VirtualProtect(Entry(),16,PAGE_EXECUTE_READWRITE,&old));
    const uint8_t saved=Entry()[15]; Entry()[15]=0xcc;
    CHECK(!NpcActionProbe::Install(Entry()));
    CHECK(!NpcActionProbe::Remove()); CHECK(NpcActionProbe::Installed());
    Entry()[15]=saved;
    FlushInstructionCache(GetCurrentProcess(),Entry(),16);
    DWORD ignored=0; VirtualProtect(Entry(),16,old,&ignored);
    CHECK(NpcActionProbe::Remove());
}

namespace {
struct Sites {
    std::array<uint8_t,4096> text{};
    static constexpr size_t Caller=64,ActionCaller=104,Loop=512,Descriptor=640,Job=1536;
    static constexpr size_t Cancel=2048,Cleanup=2304,Dispatch=2560;
    void Put(size_t at,std::initializer_list<uint8_t> bytes) {
        std::copy(bytes.begin(),bytes.end(),text.begin()+at);
    }
    void Rel(size_t at,size_t target) {
        const auto disp=static_cast<int32_t>(target)-static_cast<int32_t>(at+4);
        std::memcpy(text.data()+at,&disp,4);
    }
    Sites() {
        text.fill(0xcc);
        Put(Caller,{0x48,0x8d,0x0d,0,0,0,0,0x48,0x89,0x54,0x24,0x38,0x48,0x8d,0x54,0x24,0x30,0xe8,0,0,0,0,0x8b,0x87,0,0,0,0,0x48,0x8d});
        Rel(Caller+3,1900);
        Put(ActionCaller,{0x48,0x8d,0x0d,0,0,0,0,0x48,0x8d,0x14,0x82,0x48,0x89,0x54,0x24,0x38,0x48,0x8d,0x54,0x24,0x30,0xe8,0,0,0,0});
        Rel(ActionCaller+3,1900); Rel(ActionCaller+22,Loop);
        Put(Loop+32,{0x4b,0x63,0x04,0x07,0x49,0x8d,0x5d,0x08,0x48,0x69,0xc8,0xf8,0,0,0,0x48,0x03,0xd9});
        Put(Descriptor,{0x48,0x8d,0x0d,0,0,0,0,0x48,0x89,0x58,0x10,0x48,0x89,0x08,0x48,0x8d,0x0d,0,0,0,0,0x48,0x89,0x48,0x08,0x48,0x89,0x70,0x20,0x89,0x70,0x28,0x48,0x89,0x70,0x30,0x48,0x89,0x70,0x38});
        Rel(Descriptor+17,Job);
        Put(Job,{0x40,0x53,0x48,0x83,0xec,0x20,0x48,0x8b,0xd9,0x48,0x8b,0x89,0xe0,0,0,0,0x48,0x8b,0x01,0xff,0x50,0x08,0x83,0xf8,0x01});
    }
    void* Resolve() {
        const auto from=reinterpret_cast<uintptr_t>(text.data());
        return CrashGuardsTest::ActionJobInText(from,from+text.size());
    }
    void AddCancellation() {
        Put(Loop+96,{0x8b,0x10,0x48,0x8b,0xcb,0xe8,0,0,0,0,0x48,0x8b,0x83,0xa0,0,0,0,0x48,0x85,0xc0});
        Rel(Loop+96+6,Cancel);
        Put(Loop+600,{0xba,4,0,0,0,0x48,0x8b,0xcb,0xe8,0,0,0,0});
        Rel(Loop+600+9,Cleanup);
        Put(Cancel,{0x48,0x89,0x5c,0x24,0x10,0x57,0x48,0x83,0xec,0x20,0x48,0x8b,0xd9,0x8b,0xfa});
        Put(Cancel+15,{0x48,0x8b,0x89,0xe8,0,0,0,0x48,0x85,0xc9,0x74,0x2c,0x83,0x79,0x28,1});
        Put(Cancel+45,{0xc7,0x41,0x28,2,0,0,0,0x48,0x89,0x44,0x24,0x30,0xe8,0,0,0,0});
        Rel(Cancel+58,Dispatch);
        Put(Cancel+64,{0xc7,0x41,0x28,3,0,0,0});
        Put(Cancel+71,{0x8b,0xd7,0x48,0x8b,0xcb,0x48,0x8b,0x5c,0x24,0x38,0x48,0x83,0xc4,0x20,0x5f,0xe9,0,0,0,0});
        Rel(Cancel+87,Cleanup);
        Put(Cleanup,{0x48,0x89,0x5c,0x24,0x18,0x55,0x48,0x83,0xec,0x20,0x48,0x8b,0xd9,0x48,0x89,0x74,0x24,0x30,0x48,0x8b,0x89,0xe0,0,0,0});
        Put(Dispatch,{0x89,0x54,0x24,0x10,0x53,0x48,0x83,0xec,0x20,0x49,0x8b,0,0x48,0x8b,0xd9,0x48,0x3b,0x41,0x20});
        for (const size_t at:{Loop+500,Loop+540}) { Put(at,{0xe8,0,0,0,0}); Rel(at+1,Dispatch); }
    }
    CrashGuards::NpcActionSites ResolveNative() {
        const auto from=reinterpret_cast<uintptr_t>(text.data());
        return CrashGuardsTest::ActionSitesInText(from,from+text.size());
    }
};
}

TEST(NpcActionProbe_ResolvesCallerTableLoopDescriptorAndJobRelationships) {
    Sites sites;
    CHECK(sites.Resolve()==sites.text.data()+Sites::Job);
    sites.Rel(Sites::ActionCaller+3,1896); CHECK(!sites.Resolve());
    sites.Rel(Sites::ActionCaller+3,1900);
    sites.text[Sites::Loop+32+11]=0xf0; CHECK(!sites.Resolve());
    sites.text[Sites::Loop+32+11]=0xf8;
    sites.Rel(Sites::Descriptor+17,4096); CHECK(!sites.Resolve());
    sites.Rel(Sites::Descriptor+17,Sites::Job);
    CHECK(sites.Resolve()==sites.text.data()+Sites::Job);
}

TEST(NpcActionProbe_ResolverRejectsAmbiguousTruncatedOrUnknownCode) {
    Sites sites;
    std::memcpy(sites.text.data()+Sites::Job+64,sites.text.data()+Sites::Job,25);
    CHECK(!sites.Resolve());
    sites.text[Sites::Job+64]=0xcc;
    sites.text[Sites::Job+16]=0x90; CHECK(!sites.Resolve());
    CHECK(!CrashGuardsTest::ActionJobInText(0,0));
    const auto from=reinterpret_cast<uintptr_t>(sites.text.data());
    CHECK(!CrashGuardsTest::ActionJobInText(from,from+Sites::Caller+20));
    CHECK(!CrashGuardsTest::ActionJobInText(from,from));
}

TEST(NpcActionSites_CancellationBundleMustResolveTheSameCleanupAndDispatcher) {
    Sites sites; CHECK(!sites.ResolveNative());
    sites.AddCancellation();
    const auto result=sites.ResolveNative(); REQUIRE(bool(result));
    CHECK(result.job==sites.text.data()+Sites::Job);
    CHECK(result.loop==sites.text.data()+Sites::Loop);
    CHECK(result.cancel==sites.text.data()+Sites::Cancel);
    CHECK(result.cleanup==sites.text.data()+Sites::Cleanup);
    CHECK(result.dispatch==sites.text.data()+Sites::Dispatch);
    sites.Rel(Sites::Cancel+87,Sites::Cleanup+1); CHECK(!sites.ResolveNative());
    sites.Rel(Sites::Cancel+87,Sites::Cleanup);
    sites.Rel(Sites::Loop+500+1,Sites::Dispatch+1); CHECK(!sites.ResolveNative());
    sites.Rel(Sites::Loop+500+1,Sites::Dispatch); CHECK(bool(sites.ResolveNative()));
}

TEST(NpcActionSites_UnknownCancellationStatesOrAmbiguousCodeCannotBeUsed) {
    Sites sites; sites.AddCancellation(); REQUIRE(bool(sites.ResolveNative()));
    sites.text[Sites::Cancel+48]=4; CHECK(!sites.ResolveNative());
    sites.text[Sites::Cancel+48]=2;
    sites.Rel(Sites::Cancel+58,sites.text.size()+100); CHECK(!sites.ResolveNative());
    sites.Rel(Sites::Cancel+58,Sites::Dispatch);
    std::memcpy(sites.text.data()+Sites::Dispatch+64,sites.text.data()+Sites::Dispatch,19);
    CHECK(!sites.ResolveNative());
    sites.text[Sites::Dispatch+64]=0xcc; CHECK(bool(sites.ResolveNative()));
    sites.text[Sites::Cleanup]=0xcc; CHECK(!sites.ResolveNative());
}
