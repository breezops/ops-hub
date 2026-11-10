#include "NpcActorDestructionObserver.h"
#include "NpcActorBirths.h"
#include "TestFramework.h"
#include <Windows.h>
#include <array>
#include <atomic>
#include <chrono>
#include <cstring>
#include <thread>

namespace {
struct Fixture {
    void* actor=nullptr;
    uint8_t* code=nullptr;
    explicit Fixture(void (*native)(void*)) {
        code=static_cast<uint8_t*>(VirtualAlloc(nullptr,4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
        if (!code) return;
        const uint8_t bytes[]{
            0x48,0x89,0x5c,0x24,0x10,0x48,0x89,0x74,0x24,0x18,
            0x48,0x89,0x7c,0x24,0x20,0x41,0x56,0x48,0x83,0xec,0x20,
            0x48,0x8d,0x05,0xe4,0x02,0,0,0x48,0x8b,0xf9,0x48,0x89,0x01,
            0x48,0xb8,0,0,0,0,0,0,0,0,0xff,0xd0,
            0x48,0x83,0xc4,0x20,0x41,0x5e,
            0x48,0x8b,0x5c,0x24,0x10,0x48,0x8b,0x74,0x24,0x18,
            0x48,0x8b,0x7c,0x24,0x20,0xc3
        };
        std::memcpy(code,bytes,sizeof(bytes));std::memcpy(code+36,&native,8);
        DWORD old=0;
        if (!VirtualProtect(code,4096,PAGE_EXECUTE_READ,&old)) { code=nullptr; return; }
        FlushInstructionCache(GetCurrentProcess(),code,sizeof(bytes));
    }
    NpcActorLifetimeSites::Sites Sites() { return {code,code+0x300,code,code}; }
    void Run(void* target=nullptr) { reinterpret_cast<void(*)(void*)>(code)(target?target:&actor); }
    ~Fixture() { NpcActorDestructionObserver::Stop(); } // Retained bridge never unhooked/freed.
};
uint64_t Invalidate(void* book,void* actor) noexcept { return static_cast<NpcActorBirths::Book*>(book)->ObserveDestructionEntry(actor); }
NpcActorBirths::Book* current=nullptr;
bool invalidBeforeBody=false;
void CheckBody(void* actor) { invalidBeforeBody=!current->Find(actor); }
std::atomic<bool> inside=false,release=false;
void BlockBody(void*) { inside=true;while(!release) std::this_thread::yield(); }
template<class F> bool Wait(F check) {
    const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(2);
    while(!check() && std::chrono::steady_clock::now()<end) std::this_thread::yield();
    return check();
}
Fixture* nested=nullptr;
void* innerActor=nullptr;
unsigned nativeCalls=0;
void NestedBody(void*) { if(++nativeCalls==1) nested->Run(innerActor); }
}
TEST(DestructionObserver_InvalidatesBeforeBodyAndAllowsFreshAddressReuse) {
    Fixture f(&CheckBody);NpcActorBirths::Book book;current=&book;REQUIRE(f.code);
    REQUIRE(book.ObserveConstruction(&f.actor));
    REQUIRE(NpcActorDestructionObserver::Install(f.Sites(),&book,&Invalidate));
    f.Run();CHECK(invalidBeforeBody);CHECK(!book.Find(&f.actor));
    const auto replacement=book.ObserveConstruction(&f.actor);REQUIRE(replacement);
    CHECK(book.Find(&f.actor)==replacement);f.Run();CHECK(!book.Find(&f.actor));
    const auto stats=NpcActorDestructionObserver::Diagnostics();
    CHECK(stats.entered==2 && stats.returned==2 && stats.invalidated==2 && stats.unknown==0);
}
TEST(DestructionObserver_StopDuringBodyDetachesDestroyedClient) {
    Fixture f(&BlockBody);REQUIRE(f.code);
    auto* book=new NpcActorBirths::Book;
    REQUIRE(book->ObserveConstruction(&f.actor));
    REQUIRE(NpcActorDestructionObserver::Install(f.Sites(),book,&Invalidate));
    inside=false;release=false;
    std::thread worker([&]{f.Run();});const bool running=Wait([]{return inside.load();});
    NpcActorDestructionObserver::Stop();delete book;
    const auto pending=NpcActorDestructionObserver::Diagnostics();
    release=true;worker.join();
    CHECK(running);CHECK(pending.entered==1 && pending.returned==0 && !pending.active);
    f.Run(); // Native code passes through with no access to the destroyed book.
    const auto done=NpcActorDestructionObserver::Diagnostics();
    CHECK(done.entered==2 && done.returned==2 && done.invalidated==1 && !done.active);
}
TEST(DestructionObserver_NestedNativeBodyDoesNotHoldClientMutex) {
    Fixture f(&NestedBody);NpcActorBirths::Book book;void* second=nullptr;REQUIRE(f.code);
    nested=&f;innerActor=&second;nativeCalls=0;
    REQUIRE(book.ObserveConstruction(&f.actor));REQUIRE(book.ObserveConstruction(&second));
    REQUIRE(NpcActorDestructionObserver::Install(f.Sites(),&book,&Invalidate));
    f.Run();const auto s=NpcActorDestructionObserver::Diagnostics();
    CHECK(nativeCalls==2 && s.entered==2 && s.returned==2 && s.invalidated==2);
}
TEST(DestructionObserver_RejectsWrongEntryAndUnknownActorRemainsUnknown) {
    Fixture f(&BlockBody);NpcActorBirths::Book book;REQUIRE(f.code);
    auto wrong=f.Sites();wrong.destructor=f.code+1;
    CHECK(!NpcActorDestructionObserver::Install(wrong,&book,&Invalidate));
    REQUIRE(NpcActorDestructionObserver::Install(f.Sites(),&book,&Invalidate));
    release=true;f.Run();CHECK(!book.Find(&f.actor));CHECK(!book.Mark());
    const auto s=NpcActorDestructionObserver::Diagnostics();CHECK(s.unknown==1 && !s.invalidated);
    CHECK(!NpcActorDestructionObserver::Install(f.Sites(),&book,&Invalidate));
}
