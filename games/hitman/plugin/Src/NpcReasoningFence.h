#pragma once
#include <cstdint>
#include <mutex>
#include <shared_mutex>
#include <utility>

// A fence for the verified synchronous reasoning loop, NOT a proof that all
// actor state/animation/physics jobs have stopped. NPC scene preparation must
// additionally fence those native paths before enabling IPartyNpcWorld.
namespace NpcReasoningFence {
class Hold {
public:
    Hold() = default;
    Hold(Hold&& other):m_Lock(std::move(other.m_Lock)),m_Held(std::exchange(other.m_Held,nullptr)) {}
    Hold& operator=(Hold&& other) {
        if (this!=&other) {
            if (m_Held) *m_Held=false;
            m_Lock=std::move(other.m_Lock);
            m_Held=std::exchange(other.m_Held,nullptr);
        }
        return *this;
    }
    ~Hold() { if (m_Held) *m_Held=false; }
    explicit operator bool() const { return m_Lock.owns_lock(); }
private:
    explicit Hold(std::unique_lock<std::shared_mutex>&& lock,bool* held):m_Lock(std::move(lock)),m_Held(held) {}
    std::unique_lock<std::shared_mutex> m_Lock;
    bool* m_Held=nullptr;
    friend Hold TryHold();
};
struct Stats { uint64_t entered=0, completed=0; uint32_t callers=0; };

// Install only at a native-quiescent point, on the signature/call-target-verified
// reasoning loop. Refuses a different entry, or unexpected entry instructions.
// Not installed by the movement preview or by merely constructing SdkWorld.
bool Install(void* verifiedEntry);
bool Installed();
Hold TryHold(); // Never waits; returns empty during an active reasoning call.
bool HeldByCurrentThread();
Stats Diagnostics();
// Restore only at a native-quiescent point. Refuses observed in-flight callers;
// does not free trampoline memory, which an already-fetched jump may still use.
bool Remove();
}
