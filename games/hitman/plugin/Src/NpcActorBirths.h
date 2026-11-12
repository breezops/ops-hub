#pragma once
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <span>
#include <unordered_map>
#include <vector>

// Pointer-only construction stamps. No game memory is read here. A stamp is
// identity evidence, not constructor-completion or native-lifetime proof.
namespace NpcActorBirths {
class Book {
public:
    class Cut {
    public:
        Cut()=default;
    private:
        struct Identity { const void* actor; uint64_t birth; };
        uint64_t source=0;
        uint64_t pruneEpoch=0;
        std::vector<Identity> actors;
        friend class Book;
    };
    struct CutState {
        size_t captured=0,entered=0,returned=0;
        bool valid=false,healthy=false;
    };
    static constexpr size_t Capacity=8192;
    explicit Book(size_t limit=Capacity) noexcept;
    uint64_t ObserveConstruction(void* actor) noexcept;
    // Pointer-only invalidation at an observed native destructor ENTRY. Returns
    // the invalidated birth, or zero for unknown/already-invalid actors. Does
    // not assert destructor return, free completion or a native worker barrier.
    uint64_t ObserveDestructionEntry(void* actor) noexcept;
    // Match the exact birth returned by ObserveDestructionEntry. Native code
    // has returned from its BODY only; the optional allocation free is later.
    bool ObserveDestructionReturn(const void* actor,uint64_t birth) noexcept;
    // Includes currently live identities AND bodies already entered but not
    // returned. Reused addresses may have multiple unfinished generations.
    Cut CaptureUnfinished() const noexcept;
    CutState Inspect(const Cut&) const noexcept;
    uint64_t Find(const void* actor) const noexcept;
    uint64_t Mark() const noexcept;
    bool Healthy() const { return m_Healthy.load(); }
    // Only after a verified native retirement barrier. Survivors may be actors
    // reused by a mission restart; later constructions are always retained.
    // Never call this from the early scene-clear callback. Counters never reset.
    bool PruneRetired(uint64_t through,std::span<void* const> survivors) noexcept;
private:
    const size_t m_Limit;
    const uint64_t m_Instance;
    mutable std::mutex m_Mutex;
    mutable std::atomic<bool> m_Healthy=true;
    uint64_t m_Issued=0,m_Retired=0;
    uint64_t m_PruneEpoch=0;
    struct Entry { uint64_t birth=0; bool live=false; };
    std::unordered_map<const void*,Entry> m_Births;
    struct Destruction { const void* actor=nullptr; bool returned=false; };
    // Key by birth rather than latest pointer: old/new bodies can overlap at
    // a reused address. No capacity is reclaimed from native entry alone.
    std::unordered_map<uint64_t,Destruction> m_Destructions;
};
}
