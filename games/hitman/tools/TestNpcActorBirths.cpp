#include "NpcActorBirths.h"
#include "TestFramework.h"
#include <array>
#include <atomic>
#include <set>
#include <thread>

using NpcActorBirths::Book;

TEST(NpcActorBirths_MissingConstructionNeverInventsAnIdentity) {
    Book book; int actor=0;
    CHECK(book.Find(&actor)==0); CHECK(book.Find(nullptr)==0);
    CHECK(book.ObserveConstruction(nullptr)==0); CHECK(book.Mark()==0);
    const auto birth=book.ObserveConstruction(&actor);
    CHECK(birth!=0); CHECK(book.Find(&actor)==birth); CHECK(book.Healthy());
}
TEST(NpcActorBirths_ReusedAddressInvalidatesTheOldGeneration) {
    Book book; int actor=0;
    const auto first=book.ObserveConstruction(&actor);
    const auto second=book.ObserveConstruction(&actor);
    CHECK(second>first); CHECK(book.Find(&actor)!=first); CHECK(book.Find(&actor)==second);
}
TEST(NpcActorBirths_RetirementKeepsSurvivorsAndLaterConstructions) {
    Book book; std::array<int,4> actors{};
    book.ObserveConstruction(&actors[0]);
    const auto survivor=book.ObserveConstruction(&actors[1]);
    book.ObserveConstruction(&actors[2]);
    const auto mark=book.Mark();
    const auto replacement=book.ObserveConstruction(&actors[2]);
    const auto later=book.ObserveConstruction(&actors[3]);
    std::array<void*,1> keep{&actors[1]};
    REQUIRE(book.PruneRetired(mark,keep));
    CHECK(book.Find(&actors[0])==0); CHECK(book.Find(&actors[1])==survivor);
    CHECK(book.Find(&actors[2])==replacement); CHECK(book.Find(&actors[3])==later);
    CHECK(!book.PruneRetired(mark-1,{})); CHECK(!book.PruneRetired(later+1,{}));
}
TEST(NpcActorBirths_ReclaimedCapacityNeverRecyclesBirthNumbers) {
    Book book(2); std::array<int,3> actors{};
    book.ObserveConstruction(&actors[0]); const auto last=book.ObserveConstruction(&actors[1]);
    REQUIRE(book.PruneRetired(last,{}));
    const auto newBirth=book.ObserveConstruction(&actors[2]);
    CHECK(newBirth>last); CHECK(book.Find(&actors[0])==0); CHECK(book.Healthy());
}
TEST(NpcActorBirths_CapacityFailureInvalidatesAllIdentityProof) {
    Book book(1); std::array<int,2> actors{};
    REQUIRE(book.ObserveConstruction(&actors[0])!=0);
    CHECK(book.ObserveConstruction(&actors[1])==0); CHECK(!book.Healthy());
    CHECK(book.Find(&actors[0])==0); CHECK(book.ObserveConstruction(&actors[0])==0);
    CHECK(!book.PruneRetired(1,{}));
    Book invalid(0); CHECK(!invalid.Healthy()); CHECK(!invalid.ObserveConstruction(&actors[0]));
}
TEST(NpcActorBirths_WorkerConstructionStampsAreUniqueAndThreadSafe) {
    Book book; std::array<std::array<int,64>,8> actors{};
    std::array<std::array<uint64_t,64>,8> births{};
    std::array<std::thread,8> workers;
    std::atomic<bool> okay=true;
    for(size_t i=0;i<workers.size();++i) workers[i]=std::thread([&,i] {
        for(size_t j=0;j<actors[i].size();++j) {
            births[i][j]=book.ObserveConstruction(&actors[i][j]);
            if (!births[i][j] || book.Find(&actors[i][j])!=births[i][j]) okay=false;
        }
    });
    for(auto& worker:workers) worker.join();
    std::set<uint64_t> unique;
    for(const auto& row:births) for(const auto birth:row) unique.insert(birth);
    CHECK(okay); CHECK(unique.size()==512); CHECK(book.Mark()==512); CHECK(book.Healthy());
}
