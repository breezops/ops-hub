#include "MinHookDiagnostics.h"
#include "TestFramework.h"
#include <Windows.h>
#include <MinHook.h>
#include <array>
#include <cstring>
#include <thread>

namespace {
using namespace MinHookDiagnostics;
void Detour() {}
struct Fixture {
    uint8_t* entry=static_cast<uint8_t*>(VirtualAlloc(nullptr,4096,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));
    Fixture() {
        if (!entry) return;
        const uint8_t code[]{0x48,0x83,0xec,0x28,0xb8,23,0,0,0,0x48,0x83,0xc4,0x28,0xc3};
        std::memcpy(entry,code,sizeof(code));
        DWORD old=0; VirtualProtect(entry,4096,PAGE_EXECUTE_READ,&old);
        FlushInstructionCache(GetCurrentProcess(),entry,sizeof(code));
    }
    ~Fixture() { SetFault(Fault::None); MH_Uninitialize(); if (entry) VirtualFree(entry,0,MEM_RELEASE); }
};
void Retry(Fixture& f) {
    SetFault(Fault::None); Report retry; void* original=nullptr;
    REQUIRE(Create(f.entry,reinterpret_cast<void*>(&Detour),&original,retry)==MH_OK);
    REQUIRE(original); CHECK(retry.virtualFailures==0 && retry.heapFailures==0);
    CHECK(reinterpret_cast<int(*)()>(original)()==23);
    CHECK(reinterpret_cast<int(*)()>(f.entry)()==23); // Create did not enable.
}
}
TEST(Allocation_VirtualFailureAndRecovery) {
    Fixture f; REQUIRE(f.entry); REQUIRE(MH_Initialize()==MH_OK);
    SetFault(Fault::VirtualAllocation); Report report; void* original=nullptr;
    CHECK(Create(f.entry,reinterpret_cast<void*>(&Detour),&original,report)==MH_ERROR_MEMORY_ALLOC);
    CHECK(!original); CHECK(report.virtualAttempts>0); CHECK(report.virtualFailures==report.virtualAttempts);
    CHECK(report.virtualError==ERROR_NOT_ENOUGH_MEMORY); CHECK(report.heapAttempts==0);
    CHECK(report.nearScanComplete); CHECK(report.nearAlignedCandidates>0); CHECK(report.nearLargestFreeExtent>0);
    CHECK(std::strcmp(AllocationPath(report),"near-code-allocation-failure")==0);
    Retry(f);
}
TEST(Allocation_HeapFailureAndRecovery) {
    Fixture f; REQUIRE(f.entry); REQUIRE(MH_Initialize()==MH_OK);
    SetFault(Fault::HeapAllocation); Report report; void* original=nullptr;
    CHECK(Create(f.entry,reinterpret_cast<void*>(&Detour),&original,report)==MH_ERROR_MEMORY_ALLOC);
    CHECK(!original); CHECK(report.virtualSuccesses==1); CHECK(report.heapAttempts==1); CHECK(report.heapFailures==1);
    CHECK(std::strcmp(AllocationPath(report),"hook-table-heap-failure")==0);
    CHECK(report.nearScanComplete);
    Retry(f);
}
TEST(Allocation_NoCandidateAndRecovery) {
    Fixture f; REQUIRE(f.entry); REQUIRE(MH_Initialize()==MH_OK);
    SetFault(Fault::NoFreeRegion); Report report; void* original=nullptr;
    CHECK(Create(f.entry,reinterpret_cast<void*>(&Detour),&original,report)==MH_ERROR_MEMORY_ALLOC);
    CHECK(!original); CHECK(report.queries>0); CHECK(report.virtualAttempts==0); CHECK(report.heapAttempts==0);
    CHECK(std::strcmp(AllocationPath(report),"no-near-code-candidate")==0);
    Retry(f);
}
TEST(Allocation_ConcurrentCaptureAndReportReset) {
    Fixture first,second; REQUIRE(first.entry && second.entry); REQUIRE(MH_Initialize()==MH_OK);
    Report a,b; a.heapFailures=999; b.virtualFailures=999;
    int statusA=-1,statusB=-1; void* originalA=nullptr; void* originalB=nullptr;
    std::thread one([&]{statusA=Create(first.entry,reinterpret_cast<void*>(&Detour),&originalA,a);});
    std::thread two([&]{statusB=Create(second.entry,reinterpret_cast<void*>(&Detour),&originalB,b);});
    one.join(); two.join();
    CHECK(statusA==MH_OK && statusB==MH_OK); CHECK(originalA && originalB);
    CHECK(a.heapFailures==0 && b.virtualFailures==0);
    CHECK(a.heapAttempts+b.heapAttempts==1); // Shared table allocation counted once.
    CHECK(a.virtualSuccesses+b.virtualSuccesses>=1 && a.virtualSuccesses+b.virtualSuccesses<=2);
    CHECK(reinterpret_cast<int(*)()>(originalA)()==23); CHECK(reinterpret_cast<int(*)()>(originalB)()==23);
}
