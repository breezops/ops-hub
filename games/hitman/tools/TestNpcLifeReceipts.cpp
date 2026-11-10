#include "NpcLifeReceipts.h"
#include "TestFramework.h"
#include <array>
using namespace NpcMutationGate;
using namespace NpcLifeReceipts;
namespace {
struct Scene {
    Scope scope=NpcMutationGate::Begin();
    std::array<int,3> actors {};
    Book book;
    ~Scene(){NpcMutationGate::Clear(scope);}
    Ticket Add(size_t i,uint32_t eventId,uint64_t birth=1,uint64_t lease=1) {
        if(!NpcMutationGate::Bind(scope,50+i,&actors[i],birth,lease)) return {};
        const auto ticket=Find(scope,50+i,birth,lease);
        return book.Bind(ticket,eventId)?ticket:Ticket{};
    }
};
}
TEST(NpcLifeReceipts_NativeActorIdUsesEntityReferenceBothHalvesAndWrapsUint32) {
    CHECK(ActorEventId(1)==0x13285CD7u);
    CHECK(ActorEventId(0x100000000ull)==uint32_t(0u-0x61C88647u));
    CHECK(ActorEventId(0x100000008ull)==0x377A6071u);
    CHECK(ActorEventId(0xFFFFFFFFFFFFFFFFull)==uint32_t(0x61C88647u-0x13285CD7u));
}
TEST(NpcLifeReceipts_OnlyNativeContractAndRegisteredActorCanConfirmLife) {
    Scene s;
    CHECK(!s.book.Begin(s.scope,""));
    REQUIRE(s.book.Begin(s.scope,"native-contract-A"));
    const auto ticket=s.Add(0,123); REQUIRE(ticket.scope);
    Life life=Life::Alive;
    CHECK(!s.book.Read(ticket,life)); // Command/catalog entry is not a completion.
    CHECK(!s.book.Observe("other-contract",123,Life::Dead));
    CHECK(!s.book.Observe("native-contract-A",456,Life::Dead));
    CHECK(!s.book.Observe("native-contract-A",123,Life::Alive));
    REQUIRE(s.book.Observe("native-contract-A",123,Life::Unconscious));
    REQUIRE(s.book.Read(ticket,life)); CHECK(life==Life::Unconscious);
    REQUIRE(s.book.Observe("native-contract-A",123,Life::Dead));
    CHECK(!s.book.Observe("native-contract-A",123,Life::Unconscious));
    REQUIRE(s.book.Read(ticket,life)); CHECK(life==Life::Dead);
    REQUIRE(s.book.Begin(s.scope,"native-contract-A"));
    REQUIRE(s.book.Read(ticket,life)); CHECK(life==Life::Dead); // Idempotent begin cannot forget death.
    CHECK(!s.book.Begin(s.scope,"changed-contract"));
}
TEST(NpcLifeReceipts_HashCollisionsAndRecycledActorAddressesCannotRedirectEvents) {
    Scene s; REQUIRE(s.book.Begin(s.scope,"contract"));
    const auto old=s.Add(0,777); REQUIRE(old.scope);
    REQUIRE(NpcMutationGate::Bind(s.scope,51,&s.actors[1],1,1));
    CHECK(!s.book.Bind(Find(s.scope,51,1,1),777)); // Same event hash, different native actor.
    CHECK(!s.book.Bind(old,778)); // Same native actor cannot change its event identity.
    REQUIRE(Retire(s.scope,50,1));
    REQUIRE(NpcMutationGate::Bind(s.scope,50,&s.actors[0],2,1));
    CHECK(!s.book.Bind(Find(s.scope,50,2,1),777)); // Old address/hash is a tombstone.
    CHECK(!s.book.Observe("contract",777,Life::Dead));
    Life life=Life::Alive;
    CHECK(!s.book.Read(old,life));
}
TEST(NpcLifeReceipts_LeaseRefreshPreservesFactsAndOldSceneEventsAreRejected) {
    Scene s; REQUIRE(s.book.Begin(s.scope,"old-contract"));
    const auto old=s.Add(0,100); REQUIRE(old.scope);
    REQUIRE(s.book.Observe("old-contract",100,Life::Dead));
    const auto next=s.Add(0,100,1,2); REQUIRE(next.scope);
    Life life=Life::Alive;
    CHECK(!s.book.Read(old,life));
    REQUIRE(s.book.Read(next,life)); CHECK(life==Life::Dead);
    s.book.Clear(); CHECK(!s.book.Begin(s.scope,"old-contract"));
    const auto fresh=NpcMutationGate::Begin(); REQUIRE(fresh);
    REQUIRE(s.book.Begin(fresh,"new-contract"));
    REQUIRE(NpcMutationGate::Bind(fresh,50,&s.actors[0],1,1));
    const auto current=Find(fresh,50,1,1);
    REQUIRE(s.book.Bind(current,100));
    CHECK(!s.book.Observe("old-contract",100,Life::Dead));
    CHECK(!s.book.Read(current,life));
    CHECK(NpcMutationGate::Clear(fresh));
}

TEST(NpcLifeReceipts_StopReadyStartReticketsSameNativeActorWithoutForgettingDeath) {
    Scene s; REQUIRE(s.book.Begin(s.scope,"same-native-contract"));
    const auto old=s.Add(0,123); REQUIRE(old.scope);
    REQUIRE(s.book.Observe("same-native-contract",123,Life::Dead));
    const auto next=NpcMutationGate::Begin(); REQUIRE(next);
    REQUIRE(s.book.Begin(next,"same-native-contract"));
    REQUIRE(NpcMutationGate::Bind(next,50,&s.actors[0],1,1));
    const auto fresh=Find(next,50,1,1);
    Life life=Life::Alive;
    CHECK(!s.book.Read(old,life));
    CHECK(!s.book.Read(fresh,life)); // Fresh scope has not proven its actor binding yet.
    REQUIRE(s.book.Bind(fresh,123));
    REQUIRE(s.book.Read(fresh,life)); CHECK(life==Life::Dead);
    CHECK(!s.book.Observe("same-native-contract",123,Life::Unconscious));
    CHECK(NpcMutationGate::Clear(next));
}
