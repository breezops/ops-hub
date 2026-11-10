#include "NpcActorBirths.h"
#include <algorithm>
#include <limits>

uint64_t NpcActorBirths::Book::ObserveConstruction(void* actor) noexcept {
    if (!actor) return 0;
    try {
        std::lock_guard lock(m_Mutex);
        if (!m_Healthy) return 0;
        const auto found=m_Births.find(actor);
        if (m_Issued==std::numeric_limits<uint64_t>::max() ||
            (found==m_Births.end() && m_Births.size()>=m_Limit)) { m_Healthy=false; return 0; }
        const auto birth=++m_Issued;
        if (found!=m_Births.end()) found->second=birth;
        else m_Births.emplace(actor,birth);
        return birth;
    } catch (...) { m_Healthy=false; return 0; }
}
uint64_t NpcActorBirths::Book::Find(const void* actor) const noexcept {
    try {
        std::lock_guard lock(m_Mutex);
        if (!m_Healthy || !actor) return 0;
        const auto found=m_Births.find(actor);
        return found==m_Births.end()?0:found->second;
    } catch (...) { m_Healthy=false; return 0; }
}
uint64_t NpcActorBirths::Book::Mark() const noexcept {
    try { std::lock_guard lock(m_Mutex); return m_Healthy?m_Issued:0; }
    catch (...) { m_Healthy=false; return 0; }
}
bool NpcActorBirths::Book::PruneRetired(uint64_t through,std::span<void* const> survivors) noexcept {
    try {
        std::lock_guard lock(m_Mutex);
        if (!m_Healthy || through<m_Retired || through>m_Issued || survivors.size()>m_Limit) return false;
        for (auto it=m_Births.begin();it!=m_Births.end();) {
            if (it->second<=through && std::find(survivors.begin(),survivors.end(),it->first)==survivors.end()) it=m_Births.erase(it);
            else ++it;
        }
        m_Retired=through;
        return true;
    } catch (...) { m_Healthy=false; return false; }
}
