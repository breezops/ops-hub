#pragma once
#include <Windows.h>

#ifdef __cplusplus
extern "C" {
#endif
LPVOID WINAPI CoopMhVirtualAlloc(LPVOID,SIZE_T,DWORD,DWORD);
SIZE_T WINAPI CoopMhVirtualQuery(LPCVOID,PMEMORY_BASIC_INFORMATION,SIZE_T);
LPVOID WINAPI CoopMhHeapAlloc(HANDLE,DWORD,SIZE_T);
LPVOID WINAPI CoopMhHeapReAlloc(HANDLE,DWORD,LPVOID,SIZE_T);
#ifdef __cplusplus
}
#endif

// Forced into vendor C translation units after Windows declarations. The
// implementation is compiled separately without these aliases. Vendor source
// files and their pinned fingerprints remain unchanged.
#ifdef COOP_MINHOOK_WRAP_ALLOCATIONS
#define VirtualAlloc CoopMhVirtualAlloc
#define VirtualQuery CoopMhVirtualQuery
#define HeapAlloc CoopMhHeapAlloc
#define HeapReAlloc CoopMhHeapReAlloc
#endif
