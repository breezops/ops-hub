#include "NpcActorBirths.h"
#include "TestFramework.h"
#include <array>
#include <atomic>
#include <set>
#include <thread>
#include <memory>

using NpcActorBirths::Book;

TEST(NpcActorBirths_ExactBodyReturnCannotCompleteReplacementGeneration) {
    Book book;int actor=0,other=0;
    const auto old=book.ObserveConstruction(&actor);const auto cut=book.CaptureUnfinished();
    REQUIRE(book.ObserveDestructionEntry(&actor)==old);
    const auto pending=book.Inspect(cut);
    CHECK(pending.valid && pending.healthy && pending.captured==1 && pending.entered==1 && !pending.returned);
    const auto fresh=book.ObserveConstruction(&actor);const auto replacement=book.CaptureUnfinished();
    CHECK(!book.ObserveDestructionReturn(&other,old));
    CHECK(!book.ObserveDestructionReturn(&actor,fresh)); // No entry for fresh generation.
    CHECK(book.ObserveDestructionReturn(&actor,old));
    CHECK(!book.ObserveDestructionReturn(&actor,old));
    CHECK(book.Inspect(cut).returned==1);
    CHECK(book.Inspect(replacement).captured==2 && book.Inspect(replacement).entered==1 && book.Inspect(replacement).returned==1);
    CHECK(book.Find(&actor)==fresh);
    CHECK(book.ObserveDestructionEntry(&actor)==fresh);
    CHECK(book.ObserveDestructionReturn(&actor,fresh));
    CHECK(book.Inspect(replacement).returned==2);CHECK(book.Inspect(cut).returned==1);
}
TEST(NpcActorBirths_CutRejectsOtherOrReconstructedBooksAndPrunedEvidence) {
    int actor=0;alignas(Book) std::byte storage[sizeof(Book)];
    auto* first=std::construct_at(reinterpret_cast<Book*>(storage));
    REQUIRE(first->ObserveConstruction(&actor));const auto cut=first->CaptureUnfinished();
    std::destroy_at(first);
    auto* replacement=std::construct_at(reinterpret_cast<Book*>(storage));
    const auto birth=replacement->ObserveConstruction(&actor);
    CHECK(!replacement->Inspect(cut).valid);CHECK(!replacement->Inspect({}).valid);
    const auto own=replacement->CaptureUnfinished();
    CHECK(replacement->ObserveDestructionEntry(&actor)==birth);
    CHECK(replacement->ObserveDestructionReturn(&actor,birth));
    CHECK(replacement->Inspect(own).returned==1);
    CHECK(replacement->PruneRetired(birth,{}));
    CHECK(!replacement->Inspect(own).valid); // Losing history never invents completion.
    std::destroy_at(replacement);
}
TEST(NpcActorBirths_BodyReturnDoesNotReclaimHistoryWithoutBarrier) {
    Book book(1);int actor=0;
    const auto birth=book.ObserveConstruction(&actor);REQUIRE(birth);
    CHECK(book.ObserveDestructionEntry(&actor)==birth);
    CHECK(book.ObserveDestructionReturn(&actor,birth));
    REQUIRE(book.ObserveConstruction(&actor));const auto cut=book.CaptureUnfinished();
    CHECK(!book.ObserveDestructionEntry(&actor));CHECK(!book.Healthy());
    CHECK(!book.Inspect(cut).healthy);CHECK(!book.Find(&actor));
}
TEST(NpcActorBirths_CutIncludesAlreadyEnteredBodies) {
    Book book;int actor=0;
    const auto birth=book.ObserveConstruction(&actor);REQUIRE(birth);
    CHECK(book.ObserveDestructionEntry(&actor)==birth);
    const auto cut=book.CaptureUnfinished();const auto pending=book.Inspect(cut);
    CHECK(pending.valid && pending.captured==1 && pending.entered==1 && !pending.returned);
    CHECK(book.ObserveDestructionReturn(&actor,birth));CHECK(book.Inspect(cut).returned==1);
    const auto after=book.Inspect(book.CaptureUnfinished());
    CHECK(after.valid && !after.captured); // Completed bodies need not enter later snapshots.
}
TEST(NpcActorBirths_PruneInvalidatesOldCutsAndKeepsNewSurvivorCutsUsable) {
    Book book;int actor=0;
    const auto birth=book.ObserveConstruction(&actor);const auto before=book.CaptureUnfinished();
    std::array<void*,1> keep{&actor};REQUIRE(book.PruneRetired(birth,keep));
    CHECK(!book.Inspect(before).valid);
    const auto after=book.CaptureUnfinished();CHECK(book.Inspect(after).valid);
    CHECK(book.ObserveDestructionEntry(&actor)==birth);
    CHECK(book.ObserveDestructionReturn(&actor,birth));CHECK(book.Inspect(after).returned==1);
    REQUIRE(book.PruneRetired(birth,{})); // Same watermark still invalidates lost evidence.
    CHECK(!book.Inspect(after).valid);
}

TEST(NpcActorBirths_DestructionEntryInvalidatesWithoutInventingRetirement) {
    Book book; int actor=0,unknown=0;
    CHECK(!book.ObserveDestructionEntry(nullptr));
    CHECK(!book.ObserveDestructionEntry(&unknown)); CHECK(!book.Mark());
    const auto first=book.ObserveConstruction(&actor);
    REQUIRE(first);
    CHECK(book.ObserveDestructionEntry(&actor)==first);
    CHECK(!book.Find(&actor)); CHECK(!book.ObserveDestructionEntry(&actor));
    CHECK(book.Mark()==first); CHECK(book.Healthy());
    const auto replacement=book.ObserveConstruction(&actor);
    CHECK(replacement>first); CHECK(book.Find(&actor)==replacement);
    // A verified prune for an older generation cannot erase the replacement.
    CHECK(book.PruneRetired(first,{})); CHECK(book.Find(&actor)==replacement);
}
TEST(NpcActorBirths_DestructionTombstonesDoNotReclaimUnverifiedCapacity) {
    Book book(1); int actor=0,other=0;
    REQUIRE(book.ObserveConstruction(&actor));
    REQUIRE(book.ObserveDestructionEntry(&actor));
    CHECK(!book.ObserveConstruction(&other)); CHECK(!book.Healthy());
}

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
