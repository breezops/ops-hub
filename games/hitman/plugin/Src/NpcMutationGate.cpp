#include "NpcMutationGate.h"
#include <Windows.h>
#include <array>
#include <limits>
#include <map>
#include <mutex>

namespace {
using namespace NpcMutationGate;
static_assert(sizeof(std::atomic<void*>)==8 && sizeof(std::atomic<uint32_t>)==4);
static_assert(std::atomic<void*>::is_always_lock_free && std::atomic<uint32_t>::is_always_lock_free);
struct Record { Ticket ticket; size_t slot=0; };
std::mutex g_Mutex;
// No thread-owned lock spans a native call: an engine fiber may suspend or
// migrate. The writer bit admits a mutation only with zero in-flight hits.
constexpr uint64_t WriterBit=uint64_t(1)<<63;
std::atomic<uint64_t> g_Ingress=0;
struct Writer {
    bool held=false;
    Writer() { uint64_t empty=0; held=g_Ingress.compare_exchange_strong(empty,WriterBit,std::memory_order_acq_rel); }
    ~Writer() { if(held) g_Ingress.store(0,std::memory_order_release); }
    explicit operator bool() const { return held; }
};
Scope g_Scope=0,g_Issued=0;
std::atomic<Scope> g_PublishedScope=0;
std::map<uint64_t,Record> g_Records;
std::map<uint64_t,uint64_t> g_Retired;
std::array<bool,Capacity> g_Used {};
std::atomic<void*> g_Blocked[Capacity] {};
std::atomic<uint32_t> g_Limit=0;
std::mutex g_PermitMutex;
std::map<uintptr_t,Permit*> g_PermitStacks;
uintptr_t CurrentStack() {
    char marker;
    const auto* tib=reinterpret_cast<const NT_TIB*>(NtCurrentTeb());
    const auto low=reinterpret_cast<uintptr_t>(tib->StackLimit);
    const auto high=reinterpret_cast<uintptr_t>(tib->StackBase);
    const auto at=reinterpret_cast<uintptr_t>(&marker);
    return low<=at && at<high ? high : 0;
}

bool Same(const Ticket& a,const Ticket& b) {
    return a.scope==b.scope && a.npc==b.npc && a.birth==b.birth && a.lease==b.lease && a.actor==b.actor;
}
bool Current(const Ticket& ticket) { // g_Mutex held.
    const auto it=g_Records.find(ticket.npc);
    return ticket.scope && ticket.scope==g_Scope && it!=g_Records.end() && Same(ticket,it->second.ticket);
}
void Reset() {
    g_Limit.store(0,std::memory_order_release);
    for(auto& ptr:g_Blocked) ptr.store(nullptr,std::memory_order_release);
    g_Used.fill(false);
    g_Records.clear();
    g_Retired.clear();
}
}

NpcMutationGate::Scope NpcMutationGate::Begin() {
    Writer ingress;
    if(!ingress) return 0;
    std::lock_guard lock(g_Mutex);
    if(g_Issued==std::numeric_limits<Scope>::max()) return 0;
    Reset();
    g_Scope=0;
    g_PublishedScope.store(0,std::memory_order_release);
    g_Scope=++g_Issued;
    g_PublishedScope.store(g_Scope,std::memory_order_release);
    return g_Scope;
}
NpcMutationGate::Scope NpcMutationGate::CurrentScope() { return g_PublishedScope.load(std::memory_order_acquire); }
bool NpcMutationGate::Clear(Scope scope) {
    Writer ingress;
    if(!ingress) return false;
    std::lock_guard lock(g_Mutex);
    if(!scope || scope!=g_Scope) return false;
    Reset(); g_Scope=0; g_PublishedScope.store(0,std::memory_order_release); return true;
}
bool NpcMutationGate::Bind(Scope scope,uint64_t npc,void* actor,uint64_t birth,uint64_t lease) {
    if(!scope || !npc || !actor || !birth || !lease) return false;
    Writer ingress;
    if(!ingress) return false;
    std::lock_guard lock(g_Mutex);
    if(scope!=g_Scope) return false;
    auto it=g_Records.find(npc);
    if(it!=g_Records.end()) {
        auto& record=it->second;
        if(record.ticket.birth!=birth || record.ticket.actor!=actor || lease<record.ticket.lease) return false;
        if(lease==record.ticket.lease) return true; // Repeated catalog does not revoke an owner.
        record.ticket.lease=lease;
        g_Blocked[record.slot].store(actor,std::memory_order_release);
        return true;
    }
    if(const auto retired=g_Retired.find(npc);retired!=g_Retired.end() && birth<=retired->second) return false;
    if(!g_Retired.contains(npc) && g_Records.size()+g_Retired.size()>=Capacity) return false;
    for(const auto& [id,record]:g_Records) if(record.ticket.actor==actor) return false;
    size_t slot=0;
    while(slot<Capacity && g_Used[slot]) ++slot;
    if(slot==Capacity) return false;
    g_Records.emplace(npc,Record{{scope,npc,birth,lease,actor},slot});
    g_Retired.erase(npc);
    g_Used[slot]=true;
    g_Blocked[slot].store(actor,std::memory_order_release);
    if(g_Limit.load()<slot+1) g_Limit.store(static_cast<uint32_t>(slot+1),std::memory_order_release);
    return true;
}
bool NpcMutationGate::SetLocalOwner(Scope scope,uint64_t npc,uint64_t birth,uint64_t lease,bool owns) {
    Writer ingress;
    if(!ingress) return false;
    std::lock_guard lock(g_Mutex);
    const auto it=g_Records.find(npc);
    if(!scope || scope!=g_Scope || it==g_Records.end() || it->second.ticket.birth!=birth || it->second.ticket.lease!=lease) return false;
    g_Blocked[it->second.slot].store(owns?nullptr:it->second.ticket.actor,std::memory_order_release);
    return true;
}
bool NpcMutationGate::Retire(Scope scope,uint64_t npc,uint64_t birth) {
    Writer ingress;
    if(!ingress) return false;
    std::lock_guard lock(g_Mutex);
    const auto it=g_Records.find(npc);
    if(!scope || scope!=g_Scope || it==g_Records.end() || it->second.ticket.birth!=birth) return false;
    g_Blocked[it->second.slot].store(nullptr,std::memory_order_release);
    g_Used[it->second.slot]=false;
    g_Retired[npc]=birth;
    g_Records.erase(it);
    return true;
}
NpcMutationGate::Ticket NpcMutationGate::Find(Scope scope,uint64_t npc,uint64_t birth,uint64_t lease) {
    std::lock_guard lock(g_Mutex);
    const auto it=g_Records.find(npc);
    if(!scope || scope!=g_Scope || it==g_Records.end() || it->second.ticket.birth!=birth || it->second.ticket.lease!=lease) return {};
    return it->second.ticket;
}
NpcMutationGate::Ticket NpcMutationGate::FindActor(void* actor) {
    std::lock_guard lock(g_Mutex);
    if(!g_Scope || !actor) return {};
    for(const auto& [id,record]:g_Records) if(record.ticket.actor==actor) return record.ticket;
    return {};
}
NpcMutationGate::Permit::Permit(Ticket ticket,bool active):m_Ticket(ticket) {
    if(!active) return;
    m_Stack=CurrentStack();
    const auto* tib=reinterpret_cast<const NT_TIB*>(NtCurrentTeb());
    const auto self=reinterpret_cast<uintptr_t>(this);
    if(!m_Stack || self<reinterpret_cast<uintptr_t>(tib->StackLimit) || self>=m_Stack) return;
    std::lock_guard lock(g_PermitMutex);
    if(g_PermitStacks.size()>=Capacity && !g_PermitStacks.contains(m_Stack)) return;
    auto& top=g_PermitStacks[m_Stack];
    size_t depth=0;
    for(auto* current=top;current;current=current->m_Previous) if(++depth>=64) return;
    m_Previous=top; top=this; m_Active=true;
}
NpcMutationGate::Permit::~Permit() {
    if(!m_Active) return;
    std::lock_guard lock(g_PermitMutex);
    auto it=g_PermitStacks.find(m_Stack);
    if(it==g_PermitStacks.end()) return;
    if(it->second==this) it->second=m_Previous;
    else for(auto* p=it->second;p;p=p->m_Previous)
        if(p->m_Previous==this) { p->m_Previous=m_Previous; break; }
    if(!it->second) g_PermitStacks.erase(it);
}
NpcMutationGate::Permit NpcMutationGate::Canonical(const Ticket& ticket) {
    bool current=false;
    { std::lock_guard lock(g_Mutex); current=Current(ticket); }
    return Permit(ticket,current);
}
bool NpcMutationGate::BlocksInteraction(void* actor) {
    if(!actor) return false;
    const auto limit=g_Limit.load(std::memory_order_acquire);
    for(uint32_t i=0;i<limit;++i)
        if(g_Blocked[i].load(std::memory_order_acquire)==actor) return true;
    return false;
}
bool NpcMutationGate::BlocksDamage(void* actor) {
    if(!BlocksInteraction(actor)) return false;
    const auto stack=CurrentStack();
    if(!stack) return true;
    std::lock_guard permits(g_PermitMutex);
    const auto it=g_PermitStacks.find(stack);
    if(it==g_PermitStacks.end()) return true;
    for(auto* permit=it->second;permit;permit=permit->m_Previous) {
        if(permit->m_Ticket.actor!=actor) continue;
        std::lock_guard lock(g_Mutex);
        if(Current(permit->m_Ticket)) return false;
    }
    return true;
}
NpcMutationGate::Hit::Hit(void* actor) {
    auto state=g_Ingress.load(std::memory_order_acquire);
    while(!(state&WriterBit)) {
        if(state==WriterBit-1) break;
        if(g_Ingress.compare_exchange_weak(state,state+1,std::memory_order_acq_rel)) { m_Admitted=true; break; }
    }
    if(!m_Admitted) { m_Blocked=true; return; }
    m_Blocked=BlocksDamage(actor);
}
NpcMutationGate::Hit::~Hit() { if(m_Admitted) g_Ingress.fetch_sub(1,std::memory_order_release); }
std::atomic<void*>* NpcMutationGate::BlockedSlots() { return g_Blocked; }
std::atomic<uint32_t>* NpcMutationGate::SlotLimit() { return &g_Limit; }
