#pragma once
#include <cstdint>

// Observation only: this does not block scheduling, cancel a job or freeze an NPC.
// The native action may yield onto another stack before its function returns.
namespace NpcActionProbe {
struct Stats {
    uint64_t entered=0, returned=0;
    uint32_t outstanding=0;
    bool installed=false, healthy=false;
};
struct UnitStats {
    uint32_t outstanding=0;
    bool observed=false, healthy=false;
};
// Verified 3.290.1 action-job entry; installation/removal additionally require
// external native quiescence. No automatic installation in the player preview.
bool Install(void* verifiedEntry);
bool Installed();
bool InstalledFor(const void* entry);
Stats Diagnostics();
// Only compares unit addresses; never reads native actor/unit memory. A zero
// count is NOT preparation proof: scheduling and other action paths remain open.
UnitStats Inspect(const void* unit);
bool Remove();
}
