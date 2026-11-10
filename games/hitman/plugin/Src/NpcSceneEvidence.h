#pragma once
#include "NpcActionProbe.h"
#include "NpcActorBirths.h"
#include <array>
#include <mutex>

// Read-only pre-clear evidence only. Observed jobs draining does not establish
// native ClearScene return or physical actor retirement. Never
// feed this report directly into ConfirmNpcSceneRetired or prune birth stamps.
namespace NpcSceneEvidence {
struct Sample {
    uint64_t token=0;
    bool fullUnload=false,bookHealthy=false;
    NpcActionProbe::FenceState jobs;
    NpcActorBirths::Book::CutState actors;
};
class Book {
public:
    static constexpr size_t Capacity=32;
    uint64_t Begin(bool fullUnload,const NpcActorBirths::Book* births=nullptr);
    std::array<Sample,Capacity> Snapshot(const NpcActorBirths::Book* births=nullptr) const;
private:
    struct Entry {
        uint64_t token=0;
        bool fullUnload=false;
        NpcActionProbe::Fence jobs;
        NpcActorBirths::Book::Cut actors;
    };
    mutable std::mutex m_Mutex;
    std::array<Entry,Capacity> m_Entries{};
    bool m_Healthy=true;
};
}
