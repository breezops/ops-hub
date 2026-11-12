#include "NpcLifeReceipts.h"

namespace {
bool Same(const NpcMutationGate::Ticket& a,const NpcMutationGate::Ticket& b) {
    return a.scope==b.scope && a.npc==b.npc && a.birth==b.birth && a.lease==b.lease && a.actor==b.actor;
}
bool Current(const NpcMutationGate::Ticket& ticket) {
    return ticket.scope && Same(ticket,NpcMutationGate::Find(ticket.scope,ticket.npc,ticket.birth,ticket.lease));
}
}
bool NpcLifeReceipts::Book::Begin(NpcMutationGate::Scope scope,const std::string& contract) {
    if(!scope || scope!=NpcMutationGate::CurrentScope() || contract.empty() || contract.size()>256) return false;
    if(scope==m_Scope) return contract==m_Contract;
    if(scope<=m_HighestScope) return false;
    // A new room epoch need not rebuild the native mission. Keep physical life
    // facts for the same contract; each actor must bind a fresh gate ticket.
    if(contract!=m_Contract) Clear();
    m_Scope=m_HighestScope=scope; m_Contract=contract; return true;
}
void NpcLifeReceipts::Book::Clear() { m_Scope=0; m_Contract.clear(); m_Entries.clear(); }
bool NpcLifeReceipts::Book::Bind(const NpcMutationGate::Ticket& ticket,uint32_t eventActor) {
    if(ticket.scope!=m_Scope || !Current(ticket)) return false;
    for(const auto& [id,entry]:m_Entries)
        if(id!=eventActor && entry.ticket.npc==ticket.npc && entry.ticket.birth==ticket.birth) return false;
    auto it=m_Entries.find(eventActor);
    if(it!=m_Entries.end()) {
        auto& previous=it->second.ticket;
        // Retain retired bindings as tombstones. Recycled addresses/hashes in
        // one contract cannot assign a late old event to a replacement actor.
        if(previous.npc!=ticket.npc || previous.birth!=ticket.birth || previous.actor!=ticket.actor || ticket.lease<previous.lease) return false;
        previous=ticket; return true;
    }
    if(m_Entries.size()>=NpcMutationGate::Capacity) return false;
    m_Entries.emplace(eventActor,Entry{ticket}); return true;
}
bool NpcLifeReceipts::Book::Observe(const std::string& contract,uint32_t eventActor,Life life) {
    if(contract!=m_Contract || m_Contract.empty() || (life!=Life::Unconscious && life!=Life::Dead)) return false;
    auto it=m_Entries.find(eventActor);
    if(it==m_Entries.end() || !Current(it->second.ticket)) return false;
    auto& entry=it->second;
    if(entry.observed && entry.life==Life::Dead && life==Life::Unconscious) return false;
    entry.life=life; entry.observed=true; return true;
}
bool NpcLifeReceipts::Book::Read(const NpcMutationGate::Ticket& ticket,Life& life) const {
    if(ticket.scope!=m_Scope || !Current(ticket)) return false;
    for(const auto& [id,entry]:m_Entries) {
        if(Same(entry.ticket,ticket) && entry.observed) { life=entry.life; return true; }
    }
    return false;
}
