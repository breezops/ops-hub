#include "NpcSceneEvidence.h"
#include <atomic>
#include <limits>

namespace {
std::atomic<uint64_t> issued=0;
uint64_t Issue() {
    auto value=issued.load();
    while (value!=std::numeric_limits<uint64_t>::max())
        if (issued.compare_exchange_weak(value,value+1)) return value+1;
    return 0;
}
}
uint64_t NpcSceneEvidence::Book::Begin(bool fullUnload,const NpcActorBirths::Book* births) {
    // Capture only observed entries; no native pointer dereference or lock
    // spans ClearScene or a yielding job. Keep this book across scene resets.
    const auto jobs=NpcActionProbe::CaptureFence();
    const auto actors=births?births->CaptureUnfinished():NpcActorBirths::Book::Cut{};
    std::lock_guard lock(m_Mutex);
    if (!m_Healthy) return 0;
    for (auto& entry:m_Entries) if (!entry.token) {
        const auto token=Issue();
        if (!token) { m_Healthy=false; return 0; }
        entry={token,fullUnload,false,jobs,actors};
        return token;
    }
    m_Healthy=false; // Bounded diagnostic: do not overwrite unresolved evidence.
    return 0;
}
void NpcSceneEvidence::Book::ObserveDispatchReturn(uint64_t token) {
    if (!token) return;
    std::lock_guard lock(m_Mutex);
    for (auto& entry:m_Entries) if (entry.token==token) {
        entry.dispatchReturned=true;
        return;
    }
}
std::array<NpcSceneEvidence::Sample,NpcSceneEvidence::Book::Capacity> NpcSceneEvidence::Book::Snapshot(const NpcActorBirths::Book* births) const {
    std::array<Entry,Capacity> entries;
    bool healthy;
    { std::lock_guard lock(m_Mutex); entries=m_Entries; healthy=m_Healthy; }
    std::array<Sample,Capacity> samples{};
    for (size_t i=0;i<entries.size();++i) {
        const auto& entry=entries[i];
        if (entry.token) samples[i]={entry.token,entry.fullUnload,healthy,entry.dispatchReturned,
            NpcActionProbe::InspectFence(entry.jobs),births?births->Inspect(entry.actors):NpcActorBirths::Book::CutState{}};
    }
    return samples;
}
