#include "NpcActorLifetimeSites.h"
#include <Windows.h>
#include <array>
#include <cstring>
#include <vector>

namespace {
bool Fits(uintptr_t at,size_t count,uintptr_t from,uintptr_t to) {
    return from && from<to && at>=from && at<=to && count<=to-at;
}
bool Read(uintptr_t at,void* out,size_t size,uintptr_t from,uintptr_t to) {
    SIZE_T count=0;
    return Fits(at,size,from,to) && ReadProcessMemory(GetCurrentProcess(),
        reinterpret_cast<const void*>(at),out,size,&count) && count==size;
}
uintptr_t Relative(uintptr_t next,int32_t displacement) {
    if (displacement<0) {
        const auto delta=static_cast<uintptr_t>(-static_cast<int64_t>(displacement));
        return next>=delta?next-delta:0;
    }
    return next<=UINTPTR_MAX-static_cast<uint32_t>(displacement)?next+displacement:0;
}
}
NpcActorLifetimeSites::Sites NpcActorLifetimeSites::Resolve(uintptr_t from,uintptr_t to,uintptr_t rfrom,uintptr_t rto,
    uintptr_t preferredBase,uintptr_t mappedBase) {
    // The primary-vtable load remains beyond the SDK's constructor entry patch.
    // Verify the intervening instructions preserve RAX until mov [rcx],rax.
    constexpr uint8_t ctor[]{
        0x33,0xdb,0x48,0x8d,0x05,0,0,0,0,
        0x48,0x89,0x99,0x18,0x03,0,0,
        0x49,0xbb,0,0,0,0,0,0,0,0x40,
        0x48,0x89,0x99,0x20,0x03,0,0,
        0x4c,0x89,0x99,0x28,0x03,0,0,
        0xc7,0x81,0x30,0x03,0,0,0xfe,0xff,0,0,
        0x48,0x89,0x99,0x38,0x03,0,0,
        0x48,0x89,0x99,0x40,0x03,0,0,
        0x4c,0x89,0x99,0x48,0x03,0,0,0x48,0x89,0x01
    };
    constexpr uint8_t deleting[]{
        0x48,0x89,0x5c,0x24,0x08,0x57,0x48,0x83,0xec,0x20,
        0x8b,0xda,0x48,0x8b,0xf9,0xe8,0,0,0,0,
        0xf6,0xc3,0x01,0x74,0x0d,0xba,0x40,0x14,0,0,
        0x48,0x8b,0xcf,0xe8,0,0,0,0,
        0x48,0x8b,0x5c,0x24,0x30,0x48,0x8b,0xc7,
        0x48,0x83,0xc4,0x20,0x5f,0xc3
    };
    constexpr uint8_t body[]{
        0x48,0x89,0x5c,0x24,0x10,0x48,0x89,0x74,0x24,0x18,
        0x48,0x89,0x7c,0x24,0x20,0x41,0x56,0x48,0x83,0xec,0x20,
        0x48,0x8d,0x05,0,0,0,0,0x48,0x8b,0xf9,0x48,0x89,0x01
    };
    if (!Fits(from,sizeof(ctor),from,to) || !Fits(rfrom,8,rfrom,rto) ||
        to-from>128u*1024u*1024u || !(to<=rfrom || rto<=from) ||
        (!preferredBase != !mappedBase)) return {};
    std::vector<uint8_t> code(to-from);
    if (!Read(from,code.data(),code.size(),from,to)) return {};
    size_t selected=0,hits=0;
    for(size_t i=0;i+sizeof(ctor)<=code.size();++i) {
        if (std::memcmp(code.data()+i,ctor,5) ||
            std::memcmp(code.data()+i+9,ctor+9,sizeof(ctor)-9)) continue;
        selected=i; ++hits;
    }
    if (hits!=1) return {}; // Ambiguity rejects even if one chain would be valid.
    int32_t disp=0; std::memcpy(&disp,code.data()+selected+5,4);
    const auto anchor=from+selected,table=Relative(anchor+9,disp);
    uintptr_t wrapper=0;
    if (table%8 || !Read(table,&wrapper,8,rfrom,rto)) return {};
    if (preferredBase) {
        if (wrapper<preferredBase || wrapper-preferredBase>UINTPTR_MAX-mappedBase) return {};
        wrapper=mappedBase+(wrapper-preferredBase);
    }
    std::array<uint8_t,sizeof(deleting)> entry{};
    if (!Read(wrapper,entry.data(),entry.size(),from,to)) return {};
    for(size_t i=0;i<entry.size();++i)
        if (!((i>=16 && i<20) || (i>=34 && i<38)) && entry[i]!=deleting[i]) return {};
    std::memcpy(&disp,entry.data()+16,4);
    const auto destructor=Relative(wrapper+20,disp);
    std::memcpy(&disp,entry.data()+34,4);
    const auto free=Relative(wrapper+38,disp);
    if (!Fits(free,1,from,to) || free==destructor || wrapper==destructor) return {};
    std::array<uint8_t,sizeof(body)> actual{};
    if (!Read(destructor,actual.data(),actual.size(),from,to)) return {};
    for(size_t i=0;i<actual.size();++i)
        if (!(i>=24 && i<28) && actual[i]!=body[i]) return {};
    std::memcpy(&disp,actual.data()+24,4);
    if (Relative(destructor+28,disp)!=table) return {};
    return {reinterpret_cast<void*>(anchor),reinterpret_cast<void*>(table),
        reinterpret_cast<void*>(wrapper),reinterpret_cast<void*>(destructor)};
}
NpcActorLifetimeSites::Sites NpcActorLifetimeSites::ResolveMainModule() {
    const auto base=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    if (!base) return {};
    const auto* dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic!=IMAGE_DOS_SIGNATURE || dos->e_lfanew<0 || dos->e_lfanew>1024*1024) return {};
    const auto* nt=reinterpret_cast<const IMAGE_NT_HEADERS64*>(base+dos->e_lfanew);
    if (nt->Signature!=IMAGE_NT_SIGNATURE || nt->FileHeader.Machine!=IMAGE_FILE_MACHINE_AMD64 ||
        nt->OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR64_MAGIC || nt->FileHeader.NumberOfSections>96) return {};
    const auto* sections=IMAGE_FIRST_SECTION(nt);
    uintptr_t from=0,to=0,rfrom=0,rto=0;
    for(unsigned i=0;i<nt->FileHeader.NumberOfSections;++i) {
        const auto& section=sections[i];
        if (section.VirtualAddress>nt->OptionalHeader.SizeOfImage ||
            section.Misc.VirtualSize>nt->OptionalHeader.SizeOfImage-section.VirtualAddress) return {};
        if (!std::memcmp(section.Name,".text\0\0\0",8)) { from=base+section.VirtualAddress; to=from+section.Misc.VirtualSize; }
        if (!std::memcmp(section.Name,".rdata\0\0",8)) { rfrom=base+section.VirtualAddress; rto=rfrom+section.Misc.VirtualSize; }
    }
    return Resolve(from,to,rfrom,rto);
}
