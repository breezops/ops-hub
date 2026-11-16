#include "MinHookDiagnostics.h"
#include "TestFramework.h"
#include <Windows.h>
#include <MinHook.h>
#include <array>
#include <cstring>

namespace {
using Native=void (*)(void*);
Native original=nullptr;
unsigned calls=0;
void Detour(void* unit) { ++calls; original(unit); }
uint64_t Step(void*) { return 123; }
struct Unit { std::array<uint8_t,0xd8> pad{}; uint64_t result=0; void* action=nullptr; };
static_assert(offsetof(Unit,action)==0xe0);
struct Arena {
    uint8_t* base=static_cast<uint8_t*>(VirtualAlloc(nullptr,0x100000000ull,MEM_RESERVE,PAGE_NOACCESS));
    uint8_t* entry=nullptr;
    Arena() {
        if (base) entry=static_cast<uint8_t*>(VirtualAlloc(base+0x80000000ull,4096,MEM_COMMIT,PAGE_READWRITE));
    }
    ~Arena() { MH_Uninitialize(); if (base) VirtualFree(base,0,MEM_RELEASE); }
};
}
TEST(Absolute_NoNearSpacePreservesOriginalAndRestoresEntry) {
    Arena arena; REQUIRE(arena.entry); REQUIRE(MH_Initialize()==MH_OK);
    const uint8_t code[]{0x40,0x53,0x48,0x83,0xec,0x20,0x48,0x8b,0xd9,0x48,0x8b,0x89,0xe0,0,0,0,
        0x48,0x8b,0x01,0xff,0x50,0x08,0x48,0x89,0x83,0xd8,0,0,0,0x48,0x83,0xc4,0x20,0x5b,0xc3};
    std::memcpy(arena.entry,code,sizeof(code)); DWORD old=0;
    REQUIRE(VirtualProtect(arena.entry,4096,PAGE_EXECUTE_READ,&old));
    FlushInstructionCache(GetCurrentProcess(),arena.entry,sizeof(code));
    void* table[]{nullptr,reinterpret_cast<void*>(&Step)}; void** action=table;
    Unit unit; unit.action=&action;
    reinterpret_cast<Native>(arena.entry)(&unit); CHECK(unit.result==123);
    void* trampoline=nullptr; MinHookDiagnostics::Report report;
    REQUIRE(MinHookDiagnostics::Create(arena.entry,reinterpret_cast<void*>(&Detour),&trampoline,report)==MH_OK);
    const auto entry=reinterpret_cast<uintptr_t>(arena.entry),allocated=reinterpret_cast<uintptr_t>(trampoline);
    const auto distance=entry>allocated?entry-allocated:allocated-entry;
    CHECK(distance>0x7fffffff); // The whole +/-2GiB range is our reserved arena.
    CHECK(report.virtualAttempts==1 && report.virtualSuccesses==1);
    CHECK(std::strcmp(MinHookDiagnostics::EntryMode(),"absolute-14-verified-prefix")==0);
    original=reinterpret_cast<Native>(trampoline);
    REQUIRE(MH_EnableHook(arena.entry)==MH_OK);
    CHECK(arena.entry[0]==0xff && arena.entry[1]==0x25);
    unit.result=0; reinterpret_cast<Native>(arena.entry)(&unit);
    CHECK(unit.result==123); CHECK(calls==1);
    REQUIRE(MH_DisableHook(arena.entry)==MH_OK);
    CHECK(std::memcmp(arena.entry,code,sizeof(code))==0);
    unit.result=0; reinterpret_cast<Native>(arena.entry)(&unit);
    CHECK(unit.result==123); CHECK(calls==1);
}
TEST(Absolute_UnknownPrefixRefusedWithoutModification) {
    Arena arena; REQUIRE(arena.entry); REQUIRE(MH_Initialize()==MH_OK);
    std::memset(arena.entry,0x90,32); arena.entry[32]=0xc3;
    DWORD old=0; REQUIRE(VirtualProtect(arena.entry,4096,PAGE_EXECUTE_READ,&old));
    void* trampoline=nullptr; MinHookDiagnostics::Report report;
    CHECK(MinHookDiagnostics::Create(arena.entry,reinterpret_cast<void*>(&Detour),&trampoline,report)==MH_ERROR_UNSUPPORTED_FUNCTION);
    CHECK(!trampoline); CHECK(report.virtualAttempts==0); CHECK(arena.entry[0]==0x90);
}
TEST(Absolute_RelativeInstructionsRefusedByDecoder) {
    Arena arena; REQUIRE(arena.entry); REQUIRE(MH_Initialize()==MH_OK);
    for (bool rip:{false,true}) {
        DWORD old=0; REQUIRE(VirtualProtect(arena.entry,4096,PAGE_READWRITE,&old));
        std::array<uint8_t,32> code{}; code.fill(0x90);
        if (rip) { code[0]=0x48;code[1]=0x8b;code[2]=0x05; for (int i=3;i<7;++i) code[i]=0; }
        else { code[0]=0xe8;for (int i=1;i<5;++i) code[i]=0; }
        std::memcpy(arena.entry,code.data(),code.size());
        REQUIRE(VirtualProtect(arena.entry,4096,PAGE_EXECUTE_READ,&old));
        void* trampoline=nullptr;
        CHECK(MH_CreateHook(arena.entry,reinterpret_cast<void*>(&Detour),&trampoline)==MH_ERROR_UNSUPPORTED_FUNCTION);
        CHECK(!trampoline); CHECK(std::memcmp(arena.entry,code.data(),code.size())==0);
    }
}
TEST(Absolute_ShortEntryRefusedWithoutHotpatch) {
    Arena arena; REQUIRE(arena.entry); REQUIRE(MH_Initialize()==MH_OK);
    std::memset(arena.entry,0x90,32); arena.entry[0]=0xc3;
    DWORD old=0; REQUIRE(VirtualProtect(arena.entry,4096,PAGE_EXECUTE_READ,&old));
    void* trampoline=nullptr;
    CHECK(MH_CreateHook(arena.entry,reinterpret_cast<void*>(&Detour),&trampoline)==MH_ERROR_UNSUPPORTED_FUNCTION);
    CHECK(!trampoline); CHECK(arena.entry[0]==0xc3 && arena.entry[1]==0x90);
}
