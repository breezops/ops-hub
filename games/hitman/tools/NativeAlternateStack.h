#pragma once
#include <Windows.h>
#include <cstdint>
#include <cstring>

// Own synthetic context switch: update the TIB stack bounds and rsp without
// changing Windows thread/FLS identity, matching the relevant property of the
// game's custom context switch. No game code or process is patched.
struct AlternateStack {
    struct Call { void* top; void* low; void (*fn)(void*); void* arg; };
    void* stack=VirtualAlloc(nullptr,65536,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
    uint8_t* code=static_cast<uint8_t*>(VirtualAlloc(nullptr,4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
    AlternateStack() {
        if(!code) return;
        const uint8_t bytes[] {
            0x53,0x56,0x57,0x41,0x54, // save rbx,rsi,rdi,r12
            0x48,0x89,0xE3,0x49,0x89,0xCC, // old rsp -> rbx; context -> r12
            0x65,0x48,0x8B,0x04,0x25,0x30,0,0,0, // rax = TIB
            0x48,0x8B,0x70,0x08,0x48,0x8B,0x78,0x10, // save stack base/limit
            0x49,0x8B,0x0C,0x24,0x48,0x89,0x48,0x08, // new top -> TIB
            0x49,0x8B,0x54,0x24,0x08,0x48,0x89,0x50,0x10, // new low -> TIB
            0x48,0x89,0xCC,0x48,0x83,0xE4,0xF0,0x48,0x83,0xEC,0x20, // new rsp/shadow
            0x49,0x8B,0x4C,0x24,0x18,0x41,0xFF,0x54,0x24,0x10, // invoke callback(arg)
            0x65,0x48,0x8B,0x04,0x25,0x30,0,0,0,
            0x48,0x89,0x70,0x08,0x48,0x89,0x78,0x10, // restore TIB
            0x48,0x89,0xDC,0x41,0x5C,0x5F,0x5E,0x5B,0xC3 // restore rsp/registers/return
        };
        std::memcpy(code,bytes,sizeof(bytes));
        DWORD previous=0;
        if(!VirtualProtect(code,4096,PAGE_EXECUTE_READ,&previous)) { VirtualFree(code,0,MEM_RELEASE); code=nullptr; return; }
        FlushInstructionCache(GetCurrentProcess(),code,sizeof(bytes));
    }
    ~AlternateStack() { if(code) VirtualFree(code,0,MEM_RELEASE); if(stack) VirtualFree(stack,0,MEM_RELEASE); }
    void Run(void(*fn)(void*),void* arg) {
        Call call{static_cast<uint8_t*>(stack)+65536,stack,fn,arg};
        reinterpret_cast<void(*)(Call*)>(code)(&call);
    }
};
