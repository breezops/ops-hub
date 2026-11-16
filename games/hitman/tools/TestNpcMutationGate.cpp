#include "NativeAlternateStack.h"
#include "NpcMutationGate.h"
#include "TestFramework.h"
#include <Windows.h>
#include <cstring>
#include <array>
#include <atomic>
#include <chrono>
#include <thread>

namespace {
using namespace NpcMutationGate;
struct Scene {
    Scope scope=Begin();
    std::array<int,4> actors {};
    ~Scene(){Clear(scope);}
    bool Add(size_t i,uint64_t birth=1,uint64_t lease=1) { return Bind(scope,50+i,&actors[i],birth,lease); }
    Ticket Key(size_t i,uint64_t birth=1,uint64_t lease=1) { return Find(scope,50+i,birth,lease); }
};
template<class F> bool Wait(F predicate) {
    const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(2);
    while(!predicate() && std::chrono::steady_clock::now()<end) std::this_thread::yield();
    return predicate();
}
}

TEST(NpcMutationGate_BoundNpcStartsBlockedAndLocalOwnerKeepsNativeDamage) {
    Scene s; REQUIRE(s.scope); REQUIRE(s.Add(0));
    CHECK(BlocksDamage(&s.actors[0]));
    CHECK(BlocksInteraction(&s.actors[0]));
    CHECK(!BlocksDamage(&s.actors[1])); // Unknown actors keep the native path.
    CHECK(!BlocksDamage(nullptr));
    { Hit hit(&s.actors[0]); CHECK(hit.Blocked()); }
    REQUIRE(SetLocalOwner(s.scope,50,1,1,true));
    CHECK(!BlocksDamage(&s.actors[0]));
    CHECK(!BlocksInteraction(&s.actors[0]));
    { Hit hit(&s.actors[0]); CHECK(!hit.Blocked()); }
    REQUIRE(s.Add(0)); CHECK(!BlocksDamage(&s.actors[0])); // Catalog refresh preserves ownership.
    REQUIRE(SetLocalOwner(s.scope,50,1,1,false));
    CHECK(BlocksDamage(&s.actors[0]));
}

TEST(NpcMutationGate_LeasesScopesAndBirthsFenceStalePermissions) {
    Scene s; REQUIRE(s.scope); REQUIRE(s.Add(0));
    const auto old=s.Key(0);
    REQUIRE(Bind(s.scope,50,&s.actors[0],1,2));
    CHECK(!SetLocalOwner(s.scope,50,1,1,true));
    { auto denied=Canonical(old); CHECK(!denied); }
    CHECK(BlocksDamage(&s.actors[0]));
    REQUIRE(Retire(s.scope,50,1));
    CHECK(!Bind(s.scope,50,&s.actors[0],1,3)); // Retired birth cannot be rebound.
    REQUIRE(Bind(s.scope,50,&s.actors[0],2,1));
    CHECK(!Retire(s.scope,50,1));
    CHECK(!SetLocalOwner(s.scope,50,1,1,true));
    const auto current=Find(s.scope,50,2,1);
    auto permit=Canonical(current); REQUIRE(bool(permit));
    CHECK(!BlocksDamage(&s.actors[0]));
    const auto next=Begin(); REQUIRE(next);
    REQUIRE(Bind(next,50,&s.actors[0],3,1));
    CHECK(!Clear(s.scope));
    CHECK(BlocksDamage(&s.actors[0])); // Old live permit cannot cross a scene scope.
    CHECK(Clear(next));
}

TEST(NpcMutationGate_CanonicalPermitIsExactActorAndDoesNotEnablePicking) {
    Scene s; REQUIRE(s.Add(0)); REQUIRE(s.Add(1));
    auto outer=Canonical(s.Key(0)); REQUIRE(bool(outer));
    CHECK(!BlocksDamage(&s.actors[0]));
    CHECK(BlocksDamage(&s.actors[1]));
    CHECK(BlocksInteraction(&s.actors[0]));
    {
        auto inner=Canonical(s.Key(1)); REQUIRE(bool(inner));
        CHECK(!BlocksDamage(&s.actors[0]));
        CHECK(!BlocksDamage(&s.actors[1]));
        CHECK(BlocksInteraction(&s.actors[1]));
    }
    CHECK(BlocksDamage(&s.actors[1]));
    std::atomic<bool> blocked=false;
    std::thread other([&]{blocked=BlocksDamage(&s.actors[0]);});
    other.join(); CHECK(blocked); // No global damage bypass on another thread.
}

TEST(NpcMutationGate_ThreeConcurrentCanonicalWritersStayIsolated) {
    Scene s;
    for(size_t i=0;i<3;++i) REQUIRE(s.Add(i));
    std::array<std::atomic<bool>,3> valid {};
    std::array<std::thread,3> threads;
    for(size_t i=0;i<3;++i) threads[i]=std::thread([&,i] {
        auto permit=Canonical(s.Key(i));
        bool okay=bool(permit) && !BlocksDamage(&s.actors[i]);
        for(size_t j=0;j<3;++j) if(i!=j) okay=okay && BlocksDamage(&s.actors[j]);
        valid[i]=okay;
    });
    for(auto& thread:threads) thread.join();
    for(const auto& value:valid) CHECK(value.load());
    for(size_t i=0;i<3;++i) CHECK(BlocksDamage(&s.actors[i]));
}

TEST(NpcMutationGate_NativeHitMustFinishBeforeOwnershipCanChange) {
    Scene s; REQUIRE(s.Add(0)); REQUIRE(SetLocalOwner(s.scope,50,1,1,true));
    std::atomic<bool> entered=false,release=false,allowed=false;
    std::thread worker([&] {
        Hit hit(&s.actors[0]); allowed=!hit.Blocked(); entered=true;
        while(!release) std::this_thread::yield(); // Models the original native operation.
    });
    CHECK(Wait([&]{return entered.load();}));
    CHECK(!SetLocalOwner(s.scope,50,1,1,false));
    CHECK(!Retire(s.scope,50,1));
    CHECK(!Clear(s.scope));
    CHECK(Begin()==0);
    CHECK(CurrentScope()==s.scope);
    release=true; worker.join(); CHECK(allowed);
    REQUIRE(SetLocalOwner(s.scope,50,1,1,false));
    CHECK(BlocksDamage(&s.actors[0]));
}

TEST(NpcMutationGate_ReentrantHitCannotMutateItsOwnAdmission) {
    Scene s; REQUIRE(s.Add(0));
    auto permit=Canonical(s.Key(0)); REQUIRE(bool(permit));
    Hit outer(&s.actors[0]); CHECK(!outer.Blocked());
    { Hit inner(&s.actors[0]); CHECK(!inner.Blocked()); }
    CHECK(!SetLocalOwner(s.scope,50,1,1,true));
    CHECK(!Retire(s.scope,50,1));
    CHECK(!Clear(s.scope));
}

TEST(NpcMutationGate_InvalidAndAmbiguousBindingsCannotAuthorizeNativeDamage) {
    Scene s;
    CHECK(!Bind(0,50,&s.actors[0],1,1));
    CHECK(!Bind(s.scope,0,&s.actors[0],1,1));
    CHECK(!Bind(s.scope,50,nullptr,1,1));
    CHECK(!Bind(s.scope,50,&s.actors[0],0,1));
    REQUIRE(s.Add(0));
    CHECK(!Bind(s.scope,51,&s.actors[0],2,1));
    CHECK(!Bind(s.scope,50,&s.actors[1],1,1));
    auto forged=s.Key(0); forged.actor=&s.actors[1];
    auto denied=Canonical(forged); CHECK(!denied);
    CHECK(BlocksDamage(&s.actors[0]));
}

TEST(NpcMutationGate_RegistryAndRetiredIdMemoryAreBounded) {
    Scene s;
    std::array<int,Capacity+1> actors {};
    for(size_t i=0;i<Capacity;++i) REQUIRE(Bind(s.scope,100+i,&actors[i],1,1));
    CHECK(!Bind(s.scope,100+Capacity,&actors[Capacity],1,1));
    CHECK(SlotLimit()->load()<=Capacity);
    REQUIRE(Retire(s.scope,100,1));
    CHECK(!Bind(s.scope,100+Capacity,&actors[Capacity],1,1)); // Tombstones stay bounded too.
    REQUIRE(Bind(s.scope,100,&actors[0],2,1));
    CHECK(BlocksInteraction(&actors[0]));
}

namespace {
thread_local int ThreadMarker=0;
struct ContextProbe {
    Ticket own,other;
    bool sameThreadLocal=false,outerBlocked=false,ownAllowed=false,roleChangeRefused=false;
};
void ProbeAlternateContext(void* opaque) {
    auto& probe=*static_cast<ContextProbe*>(opaque);
    probe.sameThreadLocal=ThreadMarker==42;
    probe.outerBlocked=BlocksDamage(probe.other.actor);
    auto permit=Canonical(probe.own);
    probe.ownAllowed=bool(permit) && !BlocksDamage(probe.own.actor) && BlocksDamage(probe.other.actor);
    probe.roleChangeRefused=!SetLocalOwner(probe.other.scope,probe.other.npc,probe.other.birth,probe.other.lease,true);
}
}

TEST(NpcMutationGate_CustomFiberStackDoesNotBorrowAnotherContextsPermit) {
    Scene s; REQUIRE(s.Add(0)); REQUIRE(s.Add(1));
    AlternateStack alternate; REQUIRE(alternate.code && alternate.stack);
    ThreadMarker=42;
    auto permit=Canonical(s.Key(0)); REQUIRE(bool(permit));
    Hit hit(&s.actors[0]); REQUIRE(!hit.Blocked());
    ContextProbe probe{s.Key(1),s.Key(0)};
    alternate.Run(&ProbeAlternateContext,&probe);
    CHECK(probe.sameThreadLocal); // This regression would leak with thread-local permissions.
    CHECK(probe.outerBlocked);
    CHECK(probe.ownAllowed);
    CHECK(probe.roleChangeRefused); // Parent's native call is still in flight on another stack.
    CHECK(!BlocksDamage(&s.actors[0]));
    CHECK(BlocksDamage(&s.actors[1]));
}
