#pragma once
#include "NpcMutationGate.h"
#include <map>
#include <string>

namespace NpcLifeReceipts {
enum class Life : uint8_t { Alive, Unconscious, Dead };
// 3.290.1 telemetry's ActorId is the hash of the *native entity reference*,
// not the stable NPC template ID, actor name, or runtime-array index. Verified
// at native ActorId event producers after the GetID interface call.
constexpr uint32_t ActorEventId(uint64_t nativeEntityRef) {
    return static_cast<uint32_t>(nativeEntityRef)*0x13285CD7u -
        static_cast<uint32_t>(nativeEntityRef>>32)*0x61C88647u;
}
class Book {
public:
    // Used under the native adapter's existing session/game-thread lock.
    bool Begin(NpcMutationGate::Scope,const std::string& nativeContract);
    void Clear();
    NpcMutationGate::Scope ScopeKey() const { return m_Scope; }
    bool Bind(const NpcMutationGate::Ticket&,uint32_t eventActor);
    bool Observe(const std::string& nativeContract,uint32_t eventActor,Life);
    bool Read(const NpcMutationGate::Ticket&,Life&) const;
private:
    struct Entry { NpcMutationGate::Ticket ticket; Life life=Life::Alive; bool observed=false; };
    NpcMutationGate::Scope m_Scope=0;
    NpcMutationGate::Scope m_HighestScope=0;
    std::string m_Contract;
    std::map<uint32_t,Entry> m_Entries;
};
}
