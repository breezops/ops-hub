#include "NpcActorBirths.h"
#include <algorithm>
#include <limits>

namespace {
uint64_t NextInstance() noexcept {
    static std::atomic<uint64_t> issued=0;
    auto value=issued.load();
    while(value!=std::numeric_limits<uint64_t>::max())
        if (issued.compare_exchange_weak(value,value+1)) return value+1;
    return 0;
}
}
NpcActorBirths::Book::Book(size_t limit) noexcept:m_Limit(limit),m_Instance(NextInstance()) {
    if (!limit || limit>Capacity || !m_Instance) m_Healthy=false;
}

uint64_t NpcActorBirths::Book::ObserveConstruction(void* actor) noexcept {
    if (!actor) return 0;
    try {
        std::lock_guard lock(m_Mutex);
        if (!m_Healthy) return 0;
        const auto found=m_Births.find(actor);
        if (m_Issued==std::numeric_limits<uint64_t>::max() ||
            (found==m_Births.end() && m_Births.size()>=m_Limit)) { m_Healthy=false; return 0; }
        const auto birth=++m_Issued;
        if (found!=m_Births.end()) found->second={birth,true};
        else m_Births.emplace(actor,Entry{birth,true});
        return birth;
    } catch (...) { m_Healthy=false; return 0; }
}
uint64_t NpcActorBirths::Book::Find(const void* actor) const noexcept {
    try {
        std::lock_guard lock(m_Mutex);
        if (!m_Healthy || !actor) return 0;
        const auto found=m_Births.find(actor);
        return found==m_Births.end() || !found->second.live?0:found->second.birth;
    } catch (...) { m_Healthy=false; return 0; }
}
uint64_t NpcActorBirths::Book::ObserveDestructionEntry(void* actor) noexcept {
    try {
        std::lock_guard lock(m_Mutex);
        if (!m_Healthy || !actor) return 0;
        const auto found=m_Births.find(actor);
        if (found==m_Births.end() || !found->second.live) return 0;
        if (m_Destructions.size()>=m_Limit || m_Destructions.contains(found->second.birth)) {
            m_Healthy=false; return 0;
        }
        m_Destructions.emplace(found->second.birth,Destruction{actor,false});
        found->second.live=false;
        return found->second.birth;
    } catch (...) { m_Healthy=false; return 0; }
}
bool NpcActorBirths::Book::ObserveDestructionReturn(const void* actor,uint64_t birth) noexcept {
    try {
        std::lock_guard lock(m_Mutex);
        if (!m_Healthy || !actor || !birth) return false;
        const auto found=m_Destructions.find(birth);
        if (found==m_Destructions.end() || found->second.actor!=actor || found->second.returned) return false;
        found->second.returned=true; return true;
    } catch (...) { m_Healthy=false; return false; }
}
NpcActorBirths::Book::Cut NpcActorBirths::Book::CaptureUnfinished() const noexcept {
    try {
        std::lock_guard lock(m_Mutex);
        if (!m_Healthy) return {};
        Cut cut;
        for(const auto& [actor,entry]:m_Births)
            if (entry.live) cut.actors.push_back({actor,entry.birth});
        for(const auto& [birth,entry]:m_Destructions)
            if (!entry.returned) cut.actors.push_back({entry.actor,birth});
        cut.source=m_Instance; cut.pruneEpoch=m_PruneEpoch; return cut;
    } catch (...) { m_Healthy=false; return {}; }
}
NpcActorBirths::Book::CutState NpcActorBirths::Book::Inspect(const Cut& cut) const noexcept {
    try {
        std::lock_guard lock(m_Mutex);
        if (!cut.source || cut.source!=m_Instance || cut.pruneEpoch!=m_PruneEpoch) return {};
        CutState state{cut.actors.size(),0,0,true,m_Healthy.load()};
        if (!state.healthy) return state;
        for(const auto& identity:cut.actors) {
            const auto receipt=m_Destructions.find(identity.birth);
            if (receipt==m_Destructions.end()) continue;
            if (receipt->second.actor!=identity.actor) { state.valid=false; return state; }
            ++state.entered;
            if (receipt->second.returned) ++state.returned;
        }
        return state;
    } catch (...) { m_Healthy=false; return {}; }
}
uint64_t NpcActorBirths::Book::Mark() const noexcept {
    try { std::lock_guard lock(m_Mutex); return m_Healthy?m_Issued:0; }
    catch (...) { m_Healthy=false; return 0; }
}
bool NpcActorBirths::Book::PruneRetired(uint64_t through,std::span<void* const> survivors) noexcept {
    try {
        std::lock_guard lock(m_Mutex);
        if (!m_Healthy || through<m_Retired || through>m_Issued || survivors.size()>m_Limit ||
            m_PruneEpoch==std::numeric_limits<uint64_t>::max()) return false;
        for (auto it=m_Births.begin();it!=m_Births.end();) {
            if (it->second.birth<=through && std::find(survivors.begin(),survivors.end(),it->first)==survivors.end()) it=m_Births.erase(it);
            else ++it;
        }
        m_Retired=through;
        ++m_PruneEpoch; // Old cuts lose their evidence, including surviving births.
        for (auto it=m_Destructions.begin();it!=m_Destructions.end();)
            if (it->first<=through) it=m_Destructions.erase(it);
            else ++it;
        return true;
    } catch (...) { m_Healthy=false; return false; }
}
