#include "NpcActionObserver.h"
#include "NativeAlternateStack.h"
#include "TestFramework.h"
#include <Windows.h>
#include <atomic>
#include <cstring>
#include <thread>

namespace {
using namespace NpcActionObserver;
struct Fixture;
void Body(void*,const void*);
struct Fixture {
    std::array<uint8_t,0x11a0> actor{};
    std::array<uint8_t,8+2*0xf8> table{};
    std::array<int32_t,2> ids{0,1};
    struct Span { const int32_t* begin; const int32_t* end; } span{ids.data(),ids.data()+ids.size()};
    uint8_t* entry=nullptr;
    unsigned callbacks=0,checks=0;
    std::atomic<unsigned> originals=0;
    bool wrongIdentity=false,changedIdentity=false,otherStack=false,throwCallback=false;
    bool blockNative=false,blockCallback=false;
    HANDLE entered=CreateEventW(nullptr,TRUE,FALSE,nullptr),release=CreateEventW(nullptr,TRUE,FALSE,nullptr);
    Observation observation;
    const View* active=nullptr;
    Fixture() {
        auto* self=this;
        std::memcpy(table.data(),&self,sizeof(self));
        auto* object=actor.data(); std::memcpy(table.data()+16,&object,sizeof(object));
        std::array<uint8_t,39> code{
            0x40,0x56,0x41,0x54,0x41,0x55,0x41,0x56,0x48,0x83,0xec,0x38,0x4c,0x8b,0x02,
            0x48,0xb8,0,0,0,0,0,0,0,0,0xff,0xd0,
            0x48,0x83,0xc4,0x38,0x41,0x5e,0x41,0x5d,0x41,0x5c,0x5e,0xc3
        };
        auto* body=reinterpret_cast<void*>(&Body); std::memcpy(code.data()+17,&body,8);
        entry=static_cast<uint8_t*>(VirtualAlloc(nullptr,4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
        if (entry) {
            std::memcpy(entry,code.data(),code.size()); DWORD old=0;
            if (!VirtualProtect(entry,4096,PAGE_EXECUTE_READ,&old)) entry=nullptr;
            else FlushInstructionCache(GetCurrentProcess(),entry,code.size());
        }
    }
    ~Fixture() {
        Stop();
        // Entry/trampoline intentionally remain executable until process exit.
        CloseHandle(entered); CloseHandle(release);
    }
    static bool Validate(void* context,const void* actor,uint64_t npc,uint64_t birth,int32_t& id) {
        auto& f=*static_cast<Fixture*>(context); ++f.checks; id=0;
        return actor==f.actor.data() && npc==55 && birth==1 && !f.wrongIdentity && (!f.changedIdentity || f.checks%2==1);
    }
    static void OtherStack(void* context) {
        auto& f=*static_cast<Fixture*>(context); f.observation=f.active->Observe(f.actor.data(),55,1);
    }
    static void Callback(void* context,const View& view) {
        auto& f=*static_cast<Fixture*>(context); ++f.callbacks;
        if (f.throwCallback) throw 1;
        if (f.blockCallback) { SetEvent(f.entered); CHECK(WaitForSingleObject(f.release,5000)==WAIT_OBJECT_0); }
        CHECK(view.Count()==2); CHECK(view.Contains(0)); CHECK(!view.Contains(500));
        if (f.otherStack) {
            AlternateStack stack; REQUIRE(stack.code && stack.stack);
            f.active=&view; stack.Run(&OtherStack,&f); f.active=nullptr;
        } else f.observation=view.Observe(f.actor.data(),55,1);
    }
    bool Install() {
        CrashGuards::NpcBehaviorSites sites{{entry,entry,entry,entry,entry},entry,table.data()};
        return entry && NpcActionObserver::Install(sites,{this,&Validate,&Callback,GetCurrentThreadId()});
    }
    void Run() { reinterpret_cast<void(*)(void*,const void*)>(entry)(table.data(),&span); }
};
void Body(void* table,const void*) {
    Fixture* f=nullptr; std::memcpy(&f,table,sizeof(f)); ++f->originals;
    if (f->blockNative) { SetEvent(f->entered); CHECK(WaitForSingleObject(f->release,5000)==WAIT_OBJECT_0); }
}
}
// Each test runs in its own CTest process: the production observer permits a
// single installation and retains its code/trampoline for that process.
TEST(Observer_Lifetime) {
    Fixture f; REQUIRE(f.Install());
    CHECK(Installation().stage==InstallStage::Installed);
    f.Run(); CHECK(f.observation.valid && f.observation.native.coherent());
    CHECK(f.callbacks==1 && f.originals==1);
    f.wrongIdentity=true; f.Run(); CHECK(!f.observation.valid);
    f.wrongIdentity=false; f.changedIdentity=true; f.checks=0;
    f.Run(); CHECK(!f.observation.valid);
    f.changedIdentity=false; f.otherStack=true; f.Run(); CHECK(!f.observation.valid);
    CHECK(Diagnostics().healthy);
    Stop(); const auto callbacks=f.callbacks;
    f.Run(); CHECK(f.callbacks==callbacks && f.originals==5);
    CHECK(Diagnostics().installed && !Diagnostics().active);
    CHECK(!f.Install());
    CHECK(Installation().stage==InstallStage::AlreadyAttempted);
}
TEST(Observer_WrongThread) {
    Fixture f; REQUIRE(f.Install());
    std::thread worker([&] { f.Run(); }); worker.join();
    CHECK(f.callbacks==0 && f.originals==1 && !Diagnostics().healthy);
    f.Run(); CHECK(f.callbacks==0 && f.originals==2);
}
TEST(Observer_InvalidSpan) {
    Fixture f; REQUIRE(f.Install()); f.ids={0,0};
    f.Run(); CHECK(f.callbacks==0 && f.originals==1 && !Diagnostics().healthy);
}
TEST(Observer_CallbackFault) {
    Fixture f; REQUIRE(f.Install()); f.throwCallback=true;
    f.Run(); CHECK(f.originals==1 && !Diagnostics().healthy);
    f.Run(); CHECK(f.originals==2 && f.callbacks==1);
    Stop();
}
TEST(Observer_StopDuringNative) {
    Fixture f; REQUIRE(f.Install()); f.blockNative=true;
    std::atomic<bool> stopped=false;
    std::thread worker([&] {
        if (WaitForSingleObject(f.entered,5000)==WAIT_OBJECT_0) { Stop(); stopped=true; }
        SetEvent(f.release);
    });
    f.Run(); worker.join(); CHECK(stopped && f.originals==1);
    CHECK(!Diagnostics().active && f.callbacks==1);
}
TEST(Observer_StopDuringCallback) {
    Fixture f; REQUIRE(f.Install()); f.blockCallback=true;
    HANDLE stopStarted=CreateEventW(nullptr,TRUE,FALSE,nullptr),stopDone=CreateEventW(nullptr,TRUE,FALSE,nullptr);
    std::thread stopper([&] {
        if (WaitForSingleObject(f.entered,5000)==WAIT_OBJECT_0) { SetEvent(stopStarted); Stop(); }
        SetEvent(stopDone);
    });
    std::thread releaser([&] {
        if (WaitForSingleObject(stopStarted,5000)==WAIT_OBJECT_0) CHECK(WaitForSingleObject(stopDone,20)==WAIT_TIMEOUT);
        SetEvent(f.release);
    });
    f.Run(); stopper.join(); releaser.join();
    CHECK(WaitForSingleObject(stopDone,0)==WAIT_OBJECT_0 && !Diagnostics().active);
    CHECK(f.originals==1 && f.callbacks==1);
    CloseHandle(stopStarted); CloseHandle(stopDone);
}
TEST(Observer_InstallWhileExecuting) {
    Fixture f; REQUIRE(f.entry);
    std::atomic<bool> running=true;
    std::thread worker([&] { while (running.load()) f.Run(); });
    // The synthetic native entry is already executing when MinHook patches it.
    for (unsigned i=0;i<10000 && f.originals.load()<20;++i) SwitchToThread();
    const bool installed=f.Install();
    running=false; worker.join();
    REQUIRE(installed); CHECK(f.originals.load()>0);
    // Any worker observation must refuse its unexpected thread, but every
    // native invocation continues. No SDK or actor-control callback can run.
    CHECK(f.callbacks==0);
    Stop(); const auto count=f.originals.load(); f.Run(); CHECK(f.originals.load()==count+1);
}
TEST(Observer_RefusalDiagnostics) {
    Fixture f; REQUIRE(f.entry);
    CrashGuards::NpcBehaviorSites sites{{f.entry,f.entry,f.entry,f.entry,f.entry},f.entry,f.table.data()};
    Config config{&f,&Fixture::Validate,&Fixture::Callback,0};
    CHECK(!Install(sites,config)); CHECK(Installation().stage==InstallStage::InvalidConfig);
    CHECK(!Diagnostics().installed);
    config.gameThread=GetCurrentThreadId();
    DWORD old=0; REQUIRE(VirtualProtect(f.entry,4096,PAGE_EXECUTE_READWRITE,&old));
    f.entry[0]=0xcc;
    CHECK(!Install(sites,config)); CHECK(Installation().stage==InstallStage::EntryMismatch);
    f.entry[0]=0x40; DWORD ignored=0; REQUIRE(VirtualProtect(f.entry,4096,old,&ignored));
    REQUIRE(Install(sites,config)); CHECK(Installation().stage==InstallStage::Installed);
    f.Run(); CHECK(f.observation.valid && f.originals==1);
}
