#include "NpcBehaviorReadback.h"
#include "TestFramework.h"
#include <Windows.h>
#include <array>
#include <cstring>
#include <limits>

namespace {
template<class T,size_t N> void Put(std::array<uint8_t,N>& bytes,size_t at,T value) {
    std::memcpy(bytes.data()+at,&value,sizeof(value));
}
struct Fixture {
    std::array<uint8_t,0x11a0> actor{};
    std::array<uint8_t,8+500*0xf8> table{};
    void Bind(int32_t id) {
        Put(actor,0x119c,id); Put(table,8+static_cast<size_t>(id)*0xf8+8,reinterpret_cast<uintptr_t>(actor.data()));
    }
};
}
TEST(NpcBehaviorReadback_ObservesNativeFlagsAndJobsWithoutWriting) {
    Fixture f; f.Bind(499);
    const size_t unit=8+499*0xf8;
    f.actor[0x117d]=0x40; f.table[unit+0x2c]=1;
    Put<uintptr_t>(f.table,unit+0xa0,11); Put<uintptr_t>(f.table,unit+0xe0,22); Put<uintptr_t>(f.table,unit+0xe8,33);
    const auto beforeActor=f.actor; const auto beforeTable=f.table;
    NpcBehaviorReadback::Snapshot out;
    REQUIRE(NpcBehaviorReadback::Read(f.table.data(),f.actor.data(),499,out));
    CHECK(out.coherent()); CHECK(out.held()); CHECK(!out.drained());
    CHECK(out.action==11 && out.task==22 && out.job==33);
    CHECK(out.unit==reinterpret_cast<uintptr_t>(f.table.data()+unit));
    CHECK(f.actor==beforeActor && f.table==beforeTable);
    f.actor[0x117d]=0; f.table[unit+0x2c]=0;
    Put<uintptr_t>(f.table,unit+0xa0,0); Put<uintptr_t>(f.table,unit+0xe0,0); Put<uintptr_t>(f.table,unit+0xe8,0);
    REQUIRE(NpcBehaviorReadback::Read(f.table.data(),f.actor.data(),499,out));
    CHECK(out.coherent() && !out.held() && out.drained());
}
TEST(NpcBehaviorReadback_ReportsConflictingFlagsWithoutTreatingThemAsHeld) {
    Fixture f; f.Bind(0); f.actor[0x117d]=0x40;
    NpcBehaviorReadback::Snapshot out;
    REQUIRE(NpcBehaviorReadback::Read(f.table.data(),f.actor.data(),0,out));
    CHECK(!out.coherent() && !out.held());
}
TEST(NpcBehaviorReadback_RejectsWrongRuntimeSlotAndActorPointer) {
    Fixture f; f.Bind(2); NpcBehaviorReadback::Snapshot out;
    CHECK(!NpcBehaviorReadback::Read(f.table.data(),f.actor.data(),1,out));
    Put<uintptr_t>(f.table,8+2*0xf8+8,reinterpret_cast<uintptr_t>(f.actor.data())+8);
    CHECK(!NpcBehaviorReadback::Read(f.table.data(),f.actor.data(),2,out));
    CHECK(!out.held() && out.drained());
}
TEST(NpcBehaviorReadback_RejectsInvalidAndUnreadableMemoryClearsPriorOutput) {
    Fixture f; f.Bind(0); NpcBehaviorReadback::Snapshot out{true,true,1,2,3};
    CHECK(!NpcBehaviorReadback::Read(nullptr,f.actor.data(),0,out));
    CHECK(!out.held() && out.drained());
    CHECK(!NpcBehaviorReadback::Read(f.table.data(),f.actor.data(),-1,out));
    CHECK(!NpcBehaviorReadback::Read(f.table.data(),f.actor.data(),500,out));
    CHECK(!NpcBehaviorReadback::Read(reinterpret_cast<void*>(std::numeric_limits<uintptr_t>::max()),f.actor.data(),0,out));
    auto* page=VirtualAlloc(nullptr,4096,MEM_RESERVE|MEM_COMMIT,PAGE_NOACCESS);
    REQUIRE(page);
    CHECK(!NpcBehaviorReadback::Read(f.table.data(),page,0,out));
    CHECK(!NpcBehaviorReadback::Read(page,f.actor.data(),0,out));
    CHECK(VirtualFree(page,0,MEM_RELEASE));
}
