#pragma once
#include <cstdint>

// Diagnostic for the verified 3.290.1 scene-context virtual dispatch.
namespace NpcSceneClearObserver {
using ReturnCallback = void (*)(void*, uint64_t);
bool Install(void* context, ReturnCallback callback);
void Stop();
bool AssociatePreClear(const void* scene, bool fullUnload, uint64_t token);
#ifdef COOP_SCENE_CLEAR_TESTS
bool InstallFixture(void** slot, void* entry, void* context, ReturnCallback callback);
#endif
}
