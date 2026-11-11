#pragma once
#include "NpcActionProbe.h"
#include "NpcActorBirths.h"
#include <array>
#include <mutex>

// Read-only evidence. Dispatch return and observed jobs draining do not
// establish physical actor retirement or prove the SDK ran the native body.
namespace NpcSceneEvidence {
struct Sample {
    uint64_t token=0;
    bool fullUnload=false,bookHealthy=false;
    bool dispatchReturned=false;
    NpcActionProbe::FenceState jobs;
    NpcActorBirths::Book::CutState actors;
};
class Book {
public:
    static constexpr size_t Capacity=32;
    uint64_t Begin(bool fullUnload,const NpcActorBirths::Book* births=nullptr);
    void ObserveDispatchReturn(uint64_t token);
    std::array<Sample,Capacity> Snapshot(const NpcActorBirths::Book* births=nullptr) const;
private:
    struct Entry {
        uint64_t token=0;
        bool fullUnload=false;
        bool dispatchReturned=false;
        NpcActionProbe::Fence jobs;
        NpcActorBirths::Book::Cut actors;
    };
    mutable std::mutex m_Mutex;
    std::array<Entry,Capacity> m_Entries{};
    bool m_Healthy=true;
};
}
