#pragma once
#include <cstddef>
#include <cstdint>

// Resolves relationships only. Does not install hooks or establish a worker
// barrier. The constructor's primary vtable points to a deleting wrapper,
// which calls the body destructor before the optional size-0x1440 free.
namespace NpcActorLifetimeSites {
struct Sites {
    const void* constructorAnchor=nullptr;
    const void* vtable=nullptr;
    const void* deletingDestructor=nullptr;
    const void* destructor=nullptr;
    explicit operator bool() const { return constructorAnchor && vtable && deletingDestructor && destructor; }
};
// Explicit mapped-image section bounds; no pointers from the input are trusted
// until their complete reads fit the required section. x64 3.290.1 only.
// For a non-executable file mapping only, translate the vtable's stored PE VA
// from preferredBase into mappedBase. Runtime callers leave both zero.
Sites Resolve(uintptr_t textBegin,uintptr_t textEnd,uintptr_t rdataBegin,uintptr_t rdataEnd,
    uintptr_t preferredBase=0,uintptr_t mappedBase=0);
Sites ResolveMainModule();
}
