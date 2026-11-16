#include "NpcReasoningFence.h"
#include <Windows.h>
#include <array>
#include <atomic>
#include <cstring>

namespace {
using Loop = void (*)(void*, const void*, uint32_t);
// Three whole, relative-address-free instructions from the verified 3.290.1
// loop: save rbx/rbp/rsi to caller home space. Native loop uses rcx,rdx,r8d.
constexpr std::array<uint8_t,15> Prologue {
    0x48,0x89,0x5c,0x24,0x10, 0x48,0x89,0x6c,0x24,0x18, 0x48,0x89,0x74,0x24,0x20
};
std::shared_mutex g_Fence;
std::mutex g_Install;
std::atomic<bool> g_Installed=false;
std::atomic<Loop> g_Original=nullptr;
std::atomic<uint64_t> g_Entered=0,g_Completed=0;
std::atomic<uint32_t> g_Callers=0;
void* g_Entry=nullptr;
uint8_t* g_Trampoline=nullptr;
thread_local uint32_t g_Depth=0;
thread_local bool g_Held=false;

void Worker(void* table,const void* ids,uint32_t tick) {
    struct Count {
        Count() { ++g_Entered; ++g_Callers; }
        ~Count() { ++g_Completed; --g_Callers; }
    } count;
    const auto original=g_Original.load(std::memory_order_acquire);
    if (!original) return;
    // The native loop is not recursive. Preserve correctness if a callback
    // nevertheless re-enters it: the outer call already holds the read lock.
    if (g_Depth) { original(table,ids,tick); return; }
    std::shared_lock lock(g_Fence);
    struct Depth { Depth(){++g_Depth;} ~Depth(){--g_Depth;} } depth;
    original(table,ids,tick);
}
std::array<uint8_t,14> Jump(const void* target) {
    std::array<uint8_t,14> bytes {0xff,0x25,0,0,0,0};
    std::memcpy(bytes.data()+6,&target,sizeof(target));
    return bytes;
}
bool Write(void* at,const void* bytes,size_t size) {
    DWORD old=0;
    if (!VirtualProtect(at,size,PAGE_EXECUTE_READWRITE,&old)) return false;
    std::memcpy(at,bytes,size);
    FlushInstructionCache(GetCurrentProcess(),at,size);
    DWORD ignored=0;
    VirtualProtect(at,size,old,&ignored);
    return true;
}
bool ReadPrologue(void* entry,std::array<uint8_t,15>& bytes) {
    MEMORY_BASIC_INFORMATION info {};
    if (!entry || !VirtualQuery(entry,&info,sizeof(info)) || info.State!=MEM_COMMIT ||
        (info.Protect&(PAGE_GUARD|PAGE_NOACCESS)) ||
        !(info.Protect&(PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY))) return false;
    const auto address=reinterpret_cast<uintptr_t>(entry);
    const auto end=reinterpret_cast<uintptr_t>(info.BaseAddress)+info.RegionSize;
    if (address>end || end-address<bytes.size()) return false;
    SIZE_T count=0;
    return ReadProcessMemory(GetCurrentProcess(),entry,bytes.data(),bytes.size(),&count) && count==bytes.size();
}
}

bool NpcReasoningFence::Install(void* verifiedEntry) {
    std::lock_guard install(g_Install);
    if (g_Installed) return verifiedEntry==g_Entry;
    if (g_Entry && verifiedEntry!=g_Entry) return false;
    std::unique_lock fence(g_Fence,std::try_to_lock);
    if (!fence.owns_lock() || g_Callers.load()) return false;
    std::array<uint8_t,15> original;
    if (!ReadPrologue(verifiedEntry,original) || original!=Prologue) return false;
    if (!g_Trampoline) {
        auto* code=static_cast<uint8_t*>(VirtualAlloc(nullptr,64,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
        if (!code) return false;
        std::memcpy(code,original.data(),original.size());
        const auto back=Jump(static_cast<uint8_t*>(verifiedEntry)+original.size());
        std::memcpy(code+original.size(),back.data(),back.size());
        DWORD old=0;
        if (!VirtualProtect(code,64,PAGE_EXECUTE_READ,&old)) { VirtualFree(code,0,MEM_RELEASE); return false; }
        FlushInstructionCache(GetCurrentProcess(),code,64);
        g_Trampoline=code;
        g_Entry=verifiedEntry;
        g_Original.store(reinterpret_cast<Loop>(code),std::memory_order_release);
    }
    std::array<uint8_t,15> patch; patch.fill(0x90);
    const auto jump=Jump(reinterpret_cast<void*>(&Worker));
    std::memcpy(patch.data(),jump.data(),jump.size());
    if (!Write(verifiedEntry,patch.data(),patch.size())) return false;
    g_Installed=true;
    return true;
}
bool NpcReasoningFence::Installed() { return g_Installed; }
NpcReasoningFence::Hold NpcReasoningFence::TryHold() {
    if (g_Depth || g_Held || !g_Installed) return {};
    std::unique_lock lock(g_Fence,std::try_to_lock);
    if (!lock.owns_lock() || !g_Installed) return {};
    g_Held=true;
    return Hold(std::move(lock),&g_Held);
}
bool NpcReasoningFence::HeldByCurrentThread() { return g_Held; }
NpcReasoningFence::Stats NpcReasoningFence::Diagnostics() {
    return {g_Entered.load(),g_Completed.load(),g_Callers.load()};
}
bool NpcReasoningFence::Remove() {
    std::lock_guard install(g_Install);
    if (!g_Installed) return true;
    if (g_Depth || g_Held) return false;
    std::unique_lock fence(g_Fence,std::try_to_lock);
    if (!fence.owns_lock() || g_Callers.load()) return false;
    std::array<uint8_t,15> expected; expected.fill(0x90);
    const auto jump=Jump(reinterpret_cast<void*>(&Worker));
    std::memcpy(expected.data(),jump.data(),jump.size());
    std::array<uint8_t,15> actual;
    if (!ReadPrologue(g_Entry,actual) || actual!=expected || !Write(g_Entry,Prologue.data(),Prologue.size())) return false;
    g_Installed=false;
    return true;
}
