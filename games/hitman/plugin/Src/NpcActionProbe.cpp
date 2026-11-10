#include "NpcActionProbe.h"
#include <Windows.h>
#include <array>
#include <atomic>
#include <cstring>
#include <mutex>

namespace {
using Job=void (*)(void*);
// Four whole, relative-address-free instructions. In particular, the last mov
// is seven bytes: copying only 15 bytes would split it.
constexpr std::array<uint8_t,16> Prologue {
    0x40,0x53, 0x48,0x83,0xec,0x20, 0x48,0x8b,0xd9,
    0x48,0x8b,0x89,0xe0,0,0,0
};
constexpr size_t Capacity=1024;
std::mutex g_Install,g_Records;
std::atomic<bool> g_Installed=false;
std::atomic<Job> g_Original=nullptr;
std::atomic<uint32_t> g_Callers=0;
uint64_t g_Entered=0,g_Returned=0;
bool g_Healthy=true;
struct Record { const void* unit=nullptr; uint32_t outstanding=0; };
std::array<Record,Capacity> g_Active{};
void* g_Entry=nullptr;
uint8_t* g_Trampoline=nullptr;

void Worker(void* unit) {
    // No thread-owned lock or TLS spans the original native call. If native
    // cancellation abandons this stack, the outstanding count stays nonzero.
    ++g_Callers;
    bool tracked=false;
    {
        std::lock_guard lock(g_Records);
        ++g_Entered;
        Record* found=nullptr;
        Record* spare=nullptr;
        for (auto& record:g_Active) {
            if (record.unit==unit && unit) { found=&record; break; }
            if (!record.unit && !spare) spare=&record;
        }
        if (!found) found=spare;
        if (!unit || !found) g_Healthy=false;
        else { found->unit=unit; ++found->outstanding; tracked=true; }
    }
    const auto original=g_Original.load(std::memory_order_acquire);
    if (original) original(unit);
    {
        std::lock_guard lock(g_Records);
        if (!original) g_Healthy=false;
        else ++g_Returned;
        if (tracked) {
            bool matched=false;
            for (auto& record:g_Active) if (record.unit==unit && record.outstanding) {
                matched=true;
                if (!--record.outstanding) record.unit=nullptr;
                break;
            }
            if (!matched) g_Healthy=false;
        }
    }
    --g_Callers;
}
std::array<uint8_t,14> Jump(const void* target) {
    std::array<uint8_t,14> bytes{0xff,0x25,0,0,0,0};
    std::memcpy(bytes.data()+6,&target,sizeof(target)); return bytes;
}
auto Patch() {
    auto bytes=Prologue; bytes.fill(0x90);
    const auto jump=Jump(reinterpret_cast<void*>(&Worker));
    std::memcpy(bytes.data(),jump.data(),jump.size()); return bytes;
}
bool Read(void* at,std::array<uint8_t,16>& bytes) {
    MEMORY_BASIC_INFORMATION info{};
    if (!at || !VirtualQuery(at,&info,sizeof(info)) || info.State!=MEM_COMMIT ||
        (info.Protect&(PAGE_GUARD|PAGE_NOACCESS)) ||
        !(info.Protect&(PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY))) return false;
    const auto address=reinterpret_cast<uintptr_t>(at);
    const auto end=reinterpret_cast<uintptr_t>(info.BaseAddress)+info.RegionSize;
    if (address>end || end-address<bytes.size()) return false;
    SIZE_T count=0;
    return ReadProcessMemory(GetCurrentProcess(),at,bytes.data(),bytes.size(),&count) && count==bytes.size();
}
bool Write(void* at,const void* bytes,size_t size) {
    DWORD old=0;
    if (!VirtualProtect(at,size,PAGE_EXECUTE_READWRITE,&old)) return false;
    std::memcpy(at,bytes,size); FlushInstructionCache(GetCurrentProcess(),at,size);
    DWORD ignored=0; VirtualProtect(at,size,old,&ignored); return true;
}
}

bool NpcActionProbe::Install(void* entry) {
    std::lock_guard install(g_Install);
    if (g_Installed) {
        std::array<uint8_t,16> actual;
        return entry==g_Entry && Read(entry,actual) && actual==Patch();
    }
    if ((g_Entry && entry!=g_Entry) || g_Callers.load()) return false;
    std::array<uint8_t,16> bytes;
    if (!Read(entry,bytes) || bytes!=Prologue) return false;
    if (!g_Trampoline) {
        auto* code=static_cast<uint8_t*>(VirtualAlloc(nullptr,64,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
        if (!code) return false;
        std::memcpy(code,bytes.data(),bytes.size());
        const auto back=Jump(static_cast<uint8_t*>(entry)+bytes.size());
        std::memcpy(code+bytes.size(),back.data(),back.size());
        DWORD old=0;
        if (!VirtualProtect(code,64,PAGE_EXECUTE_READ,&old)) { VirtualFree(code,0,MEM_RELEASE); return false; }
        FlushInstructionCache(GetCurrentProcess(),code,64);
        g_Trampoline=code; g_Entry=entry;
        g_Original.store(reinterpret_cast<Job>(code),std::memory_order_release);
    }
    const auto patch=Patch();
    if (!Write(entry,patch.data(),patch.size())) return false;
    g_Installed=true; return true;
}
bool NpcActionProbe::Installed() { return g_Installed; }
NpcActionProbe::Stats NpcActionProbe::Diagnostics() {
    std::lock_guard lock(g_Records);
    return {g_Entered,g_Returned,g_Callers.load(),g_Installed.load(),g_Healthy && g_Installed.load()};
}
NpcActionProbe::UnitStats NpcActionProbe::Inspect(const void* unit) {
    std::lock_guard lock(g_Records);
    for (const auto& record:g_Active) if (unit && record.unit==unit)
        return {record.outstanding,true,g_Healthy && g_Installed.load()};
    return {0,false,g_Healthy && g_Installed.load()};
}
bool NpcActionProbe::Remove() {
    std::lock_guard install(g_Install);
    if (!g_Installed) return true;
    if (g_Callers.load()) return false;
    std::array<uint8_t,16> bytes;
    if (!Read(g_Entry,bytes) || bytes!=Patch() || !Write(g_Entry,Prologue.data(),Prologue.size())) return false;
    g_Installed=false;
    // Retain the RX trampoline for a previously fetched jump, like the existing
    // reasoning hook. Caller must still establish external native quiescence.
    return true;
}
