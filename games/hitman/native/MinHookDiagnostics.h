#pragma once
#include <cstdint>

// Only observes our private MinHook instance during hook creation. This never
// allocates memory, patches native code itself or changes the allocation policy.
namespace MinHookDiagnostics {
// Encode shared structure sizes into linker symbols. A stale translation unit
// must fail to link instead of silently copying a differently sized report.
template<uint64_t Bytes> struct LayoutTag {};
struct Report {
    uint64_t queries=0,freeRegions=0,queryFailures=0;
    uint64_t virtualAttempts=0,virtualSuccesses=0,virtualFailures=0;
    uint64_t heapAttempts=0,heapFailures=0;
    uint64_t lastVirtualBytes=0;
    uint32_t virtualError=0,queryError=0;
    uint64_t nearFreeBytes=0,nearAlignedCandidates=0,nearLargestFreeExtent=0;
    uint32_t nearScanError=0;
    bool nearScanComplete=false;
};
int Create(void* target,void* detour,void** original,Report& report,LayoutTag<sizeof(Report)> = {});
const char* AllocationPath(const Report&);
#ifdef COOP_MINHOOK_ALLOC_TESTS
enum class Fault { None, VirtualAllocation, HeapAllocation, NoFreeRegion };
// Thread-local fixture injection; omitted from player builds.
void SetFault(Fault);
#endif
}
