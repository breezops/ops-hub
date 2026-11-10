#pragma once
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <span>
#include <unordered_map>

// Pointer-only construction stamps. No game memory is read here. A stamp is
// identity evidence, not constructor-completion or native-lifetime proof.
namespace NpcActorBirths {
class Book {
public:
    static constexpr size_t Capacity=8192;
    explicit Book(size_t limit=Capacity):m_Limit(limit) { if (!limit || limit>Capacity) m_Healthy=false; }
    uint64_t ObserveConstruction(void* actor) noexcept;
    uint64_t Find(const void* actor) const noexcept;
    uint64_t Mark() const noexcept;
    bool Healthy() const { return m_Healthy.load(); }
    // Only after a verified native retirement barrier. Survivors may be actors
    // reused by a mission restart; later constructions are always retained.
    // Never call this from the early scene-clear callback. Counters never reset.
    bool PruneRetired(uint64_t through,std::span<void* const> survivors) noexcept;
private:
    const size_t m_Limit;
    mutable std::mutex m_Mutex;
    mutable std::atomic<bool> m_Healthy=true;
    uint64_t m_Issued=0,m_Retired=0;
    std::unordered_map<const void*,uint64_t> m_Births;
};
}
