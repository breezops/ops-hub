#pragma once
#include <atomic>
#include <cstddef>
#include <cstdint>

// Native ingress controls for an already validated party/NPC binding. This is
// not transport authority: only the native adapter may register/grant actors.
// Unknown actors are untouched; pool admission must bind before enabling NPCs.
namespace NpcMutationGate {
constexpr size_t Capacity=1024;
using Scope=uint64_t;
struct Ticket {
    Scope scope=0;
    uint64_t npc=0,birth=0,lease=0;
    void* actor=nullptr;
};
Scope Begin(); // New local scope; invalidates all prior tickets.
Scope CurrentScope();
bool Clear(Scope);
bool Bind(Scope,uint64_t npc,void* actor,uint64_t birth,uint64_t lease);
bool SetLocalOwner(Scope,uint64_t npc,uint64_t birth,uint64_t lease,bool owns);
bool Retire(Scope,uint64_t npc,uint64_t birth);
Ticket Find(Scope,uint64_t npc,uint64_t birth,uint64_t lease);
Ticket FindActor(void* actor);

class Hit {
public:
    explicit Hit(void* actor);
    Hit(const Hit&)=delete;
    Hit& operator=(const Hit&)=delete;
    ~Hit();
    bool Blocked() const { return m_Blocked; }
private:
    bool m_Admitted=false;
    bool m_Blocked=false;
};

class Permit {
public:
    Permit(const Permit&)=delete;
    Permit& operator=(const Permit&)=delete;
    Permit(Permit&&)=delete;
    Permit& operator=(Permit&&)=delete;
    ~Permit();
    explicit operator bool() const { return m_Active; }
private:
    Permit(Ticket,bool active);
    Ticket m_Ticket;
    uintptr_t m_Stack=0;
    Permit* m_Previous=nullptr;
    bool m_Active=false;
    friend Permit Canonical(const Ticket&);
    friend bool BlocksDamage(void*);
};
Permit Canonical(const Ticket&); // Stack-scoped, exact-actor canonical operation.
bool BlocksDamage(void* actor); // Used by the existing native hit detour.
bool BlocksInteraction(void* actor); // Canonical permits do not bypass picking.
// Fixed atomic slots are read directly by the existing picker stub. Holes are
// allowed; limit bounds the scan, it is not a null-terminated list.
std::atomic<void*>* BlockedSlots();
std::atomic<uint32_t>* SlotLimit();
}
