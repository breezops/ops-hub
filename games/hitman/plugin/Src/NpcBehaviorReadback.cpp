#include "NpcBehaviorReadback.h"
#include <Windows.h>
#include <array>
#include <cstring>
#include <limits>

namespace {
bool Copy(uintptr_t at,void* out,size_t size) {
    if (!at || at>std::numeric_limits<uintptr_t>::max()-size) return false;
    SIZE_T read=0;
    return ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<const void*>(at),out,size,&read) && read==size;
}
template<class T> T At(const std::array<uint8_t,0xf0>& bytes,size_t offset) {
    T value{}; std::memcpy(&value,bytes.data()+offset,sizeof(value)); return value;
}
}
bool NpcBehaviorReadback::Read(const void* table,const void* actor,int32_t runtimeId,Snapshot& out) {
    out={};
    const auto base=reinterpret_cast<uintptr_t>(table), object=reinterpret_cast<uintptr_t>(actor);
    if (!base || !object || runtimeId<0 || runtimeId>=500 ||
        base>std::numeric_limits<uintptr_t>::max()-(8+500*0xf8) ||
        object>std::numeric_limits<uintptr_t>::max()-0x11a0) return false;
    int32_t current=-1; uint8_t flags=0;
    std::array<uint8_t,0xf0> unit{};
    if (!Copy(object+0x119c,&current,sizeof(current)) || current!=runtimeId ||
        !Copy(object+0x117d,&flags,sizeof(flags)) ||
        !Copy(base+8+static_cast<uintptr_t>(runtimeId)*0xf8,unit.data(),unit.size()) ||
        At<uintptr_t>(unit,8)!=object) return false;
    out.actorHeld=(flags&0x40)!=0; out.unitHeld=(unit[0x2c]&1)!=0;
    out.action=At<uintptr_t>(unit,0xa0); out.task=At<uintptr_t>(unit,0xe0); out.job=At<uintptr_t>(unit,0xe8);
    out.unit=base+8+static_cast<uintptr_t>(runtimeId)*0xf8;
    return true;
}
