#include "MinHookDiagnostics.h"
#include "MinHookAllocationCalls.h"
#include <MinHook.h>
#include <algorithm>
#include <limits>
#include <array>
#include <cstring>

namespace {
// Hook creation is synchronous and does not call the game's yielding jobs.
// Per-thread capture avoids mixing concurrent hook installations and requires
// neither a heap allocation nor a lock held inside MinHook.
thread_local MinHookDiagnostics::Report* g_Report=nullptr;
#ifdef COOP_MINHOOK_ABSOLUTE_ENTRIES
bool ApprovedPrefix(void* target) {
    constexpr uint8_t job[]{0x40,0x53,0x48,0x83,0xec,0x20,0x48,0x8b,0xd9,0x48,0x8b,0x89,0xe0,0,0,0};
    constexpr uint8_t loop[]{0x40,0x56,0x41,0x54,0x41,0x55,0x41,0x56,0x48,0x83,0xec,0x38,0x4c,0x8b,0x02};
    constexpr uint8_t actor[]{0x48,0x89,0x5c,0x24,0x08,0x57,0x48,0x83,0xec,0x20,0x44,0x0f,0xb6,0x81,0x7d,0x11,0,0};
    constexpr uint8_t destructor[]{0x48,0x89,0x5c,0x24,0x10,0x48,0x89,0x74,0x24,0x18,0x48,0x89,0x7c,0x24,0x20,0x41,0x56,0x48,0x83,0xec,0x20};
    std::array<uint8_t,21> bytes{}; SIZE_T count=0;
    if (!target || !ReadProcessMemory(GetCurrentProcess(),target,bytes.data(),bytes.size(),&count) || count!=bytes.size()) return false;
    if (!std::memcmp(bytes.data(),job,sizeof(job)) || !std::memcmp(bytes.data(),loop,sizeof(loop)) || !std::memcmp(bytes.data(),actor,sizeof(actor)) || !std::memcmp(bytes.data(),destructor,sizeof(destructor))) return true;
#ifdef COOP_MINHOOK_ALLOC_TESTS
    // Fixture-only actor thunk replaces the native movzx with an absolute mov.
    if (!std::memcmp(bytes.data(),actor,10) && bytes[10]==0x48 && bytes[11]==0xb8) return true;
#endif
    return false;
}
#endif
void InspectNearRange(void* target,MinHookDiagnostics::Report& report) {
    SYSTEM_INFO system{}; GetSystemInfo(&system);
    const auto origin=reinterpret_cast<uintptr_t>(target);
    const auto minimum=reinterpret_cast<uintptr_t>(system.lpMinimumApplicationAddress);
    const auto maximum=reinterpret_cast<uintptr_t>(system.lpMaximumApplicationAddress);
    constexpr uintptr_t range=0x40000000; // Pinned MinHook MAX_MEMORY_RANGE.
    const uintptr_t granularity=system.dwAllocationGranularity;
    if (!origin || origin<minimum || origin>maximum || !granularity || maximum==std::numeric_limits<uintptr_t>::max()) return;
    uintptr_t cursor=origin>range?std::max(minimum,origin-range):minimum;
    const uintptr_t end=(maximum-origin<range?maximum:origin+range)+1;
    for (size_t steps=0;cursor<end && steps<1048576;++steps) {
        MEMORY_BASIC_INFORMATION region{};
        if (!VirtualQuery(reinterpret_cast<void*>(cursor),&region,sizeof(region))) {
            report.nearScanError=GetLastError(); return;
        }
        const auto base=reinterpret_cast<uintptr_t>(region.BaseAddress);
        if (region.RegionSize>std::numeric_limits<uintptr_t>::max()-base) return;
        const auto next=std::min(end,base+region.RegionSize);
        if (next<=cursor) return;
        if (region.State==MEM_FREE) {
            const auto bytes=next-cursor;
            report.nearFreeBytes+=bytes;
            report.nearLargestFreeExtent=std::max(report.nearLargestFreeExtent,static_cast<uint64_t>(bytes));
            const auto aligned=cursor+(granularity-cursor%granularity)%granularity;
            if (next>=4096 && aligned<=next-4096) report.nearAlignedCandidates+=1+(next-4096-aligned)/granularity;
        }
        cursor=next;
    }
    report.nearScanComplete=cursor==end;
}
#ifdef COOP_MINHOOK_ALLOC_TESTS
thread_local MinHookDiagnostics::Fault g_Fault=MinHookDiagnostics::Fault::None;
bool FailVirtual() { return g_Fault==MinHookDiagnostics::Fault::VirtualAllocation; }
bool FailHeap() { return g_Fault==MinHookDiagnostics::Fault::HeapAllocation; }
#else
constexpr bool FailVirtual() { return false; }
constexpr bool FailHeap() { return false; }
#endif
}
extern "C" LPVOID WINAPI CoopMhVirtualAlloc(LPVOID address,SIZE_T bytes,DWORD type,DWORD protection) {
    if (g_Report) { ++g_Report->virtualAttempts; g_Report->lastVirtualBytes=bytes; }
    LPVOID result=nullptr;
    if (FailVirtual()) SetLastError(ERROR_NOT_ENOUGH_MEMORY);
    else result=VirtualAlloc(address,bytes,type,protection);
    const auto error=GetLastError();
    if (g_Report) {
        if (result) ++g_Report->virtualSuccesses;
        else { ++g_Report->virtualFailures; g_Report->virtualError=error; }
    }
    SetLastError(error); return result;
}
extern "C" SIZE_T WINAPI CoopMhVirtualQuery(LPCVOID address,PMEMORY_BASIC_INFORMATION info,SIZE_T bytes) {
    const auto result=VirtualQuery(address,info,bytes);
    const auto error=GetLastError();
#ifdef COOP_MINHOOK_ALLOC_TESTS
    if (result && g_Fault==MinHookDiagnostics::Fault::NoFreeRegion && info->State==MEM_FREE) {
        info->State=MEM_RESERVE; info->AllocationBase=info->BaseAddress;
    }
#endif
    if (g_Report) {
        ++g_Report->queries;
        if (!result) { ++g_Report->queryFailures; g_Report->queryError=error; }
        else if (info->State==MEM_FREE) ++g_Report->freeRegions;
    }
    SetLastError(error); return result;
}
extern "C" LPVOID WINAPI CoopMhHeapAlloc(HANDLE heap,DWORD flags,SIZE_T bytes) {
    const auto result=FailHeap()?nullptr:HeapAlloc(heap,flags,bytes);
    const auto error=GetLastError();
    // HeapAlloc/ReAlloc do not promise a useful GetLastError on failure.
    if (g_Report) { ++g_Report->heapAttempts; if (!result) ++g_Report->heapFailures; }
    SetLastError(error); return result;
}
extern "C" LPVOID WINAPI CoopMhHeapReAlloc(HANDLE heap,DWORD flags,LPVOID memory,SIZE_T bytes) {
    const auto result=FailHeap()?nullptr:HeapReAlloc(heap,flags,memory,bytes);
    const auto error=GetLastError();
    if (g_Report) { ++g_Report->heapAttempts; if (!result) ++g_Report->heapFailures; }
    SetLastError(error); return result;
}
int MinHookDiagnostics::Create(void* target,void* detour,void** original,Report& report,LayoutTag<sizeof(Report)>) {
    report={};
#ifdef COOP_MINHOOK_ABSOLUTE_ENTRIES
    if (!ApprovedPrefix(target)) return MH_ERROR_UNSUPPORTED_FUNCTION;
#endif
    auto* previous=g_Report; g_Report=&report;
    const auto status=MH_CreateHook(target,detour,original);
    g_Report=previous;
    const auto error=GetLastError();
    if (status==MH_ERROR_MEMORY_ALLOC) InspectNearRange(target,report);
    SetLastError(error);
    return status;
}
const char* MinHookDiagnostics::AllocationPath(const Report& report) {
    if (report.heapFailures) return "hook-table-heap-failure";
    if (report.virtualFailures) return "near-code-allocation-failure";
    if (report.queryFailures) return "address-query-failure";
    if (report.queries && !report.virtualAttempts && !report.heapAttempts) return "no-near-code-candidate";
    return "no-allocation-failure-recorded";
}
const char* MinHookDiagnostics::EntryMode() {
#ifdef COOP_MINHOOK_ABSOLUTE_ENTRIES
    return "absolute-14-verified-prefix";
#else
    return "near-relay-5";
#endif
}
#ifdef COOP_MINHOOK_ALLOC_TESTS
void MinHookDiagnostics::SetFault(Fault fault) { g_Fault=fault; }
#endif
