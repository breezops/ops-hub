#include "NpcActorLifetimeSites.h"
#include "TestFramework.h"
#include <array>
#include <cstring>
#include <sstream>
#include <string>

namespace {
struct Fixture {
    alignas(8) std::array<uint8_t,0x1800> data{};
    uintptr_t Base() const { return reinterpret_cast<uintptr_t>(data.data()); }
    void Put(size_t at,const char* hex) {
        std::istringstream in(hex); std::string word;
        while(in>>word) data.at(at++)=static_cast<uint8_t>(std::stoul(word,nullptr,16));
    }
    void Rel(size_t field,size_t target) {
        const auto d=static_cast<int32_t>(target)-static_cast<int32_t>(field+4);
        std::memcpy(data.data()+field,&d,4);
    }
    Fixture() {
        Put(0x40,"33 db 48 8d 05 00 00 00 00 48 89 99 18 03 00 00 49 bb 00 00 00 00 00 00 00 40 48 89 99 20 03 00 00 4c 89 99 28 03 00 00 c7 81 30 03 00 00 fe ff 00 00 48 89 99 38 03 00 00 48 89 99 40 03 00 00 4c 89 99 48 03 00 00 48 89 01");
        Rel(0x45,0x1000);
        Put(0x200,"48 89 5c 24 08 57 48 83 ec 20 8b da 48 8b f9 e8 00 00 00 00 f6 c3 01 74 0d ba 40 14 00 00 48 8b cf e8 00 00 00 00 48 8b 5c 24 30 48 8b c7 48 83 c4 20 5f c3");
        Rel(0x210,0x300); Rel(0x222,0x400);
        Put(0x300,"48 89 5c 24 10 48 89 74 24 18 48 89 7c 24 20 41 56 48 83 ec 20 48 8d 05 00 00 00 00 48 8b f9 48 89 01");
        Rel(0x318,0x1000);
        auto wrapper=Base()+0x200;
        std::memcpy(data.data()+0x1000,&wrapper,8);
    }
    auto Resolve() const { return NpcActorLifetimeSites::Resolve(Base(),Base()+0x1000,Base()+0x1000,Base()+data.size()); }
};
}
TEST(NpcActorLifetimeSites_ResolvesLinkedClassAndBodyWithoutExecutingIt) {
    Fixture f;const auto s=f.Resolve();REQUIRE(s);
    CHECK(s.constructorAnchor==f.data.data()+0x40); CHECK(s.vtable==f.data.data()+0x1000);
    CHECK(s.deletingDestructor==f.data.data()+0x200); CHECK(s.destructor==f.data.data()+0x300);
}
TEST(NpcActorLifetimeSites_RejectsAmbiguousAndBrokenClassRelationships) {
    { Fixture f;std::memcpy(f.data.data()+0x500,f.data.data()+0x40,74); CHECK(!f.Resolve()); }
    { Fixture f;f.Rel(0x318,0x1008); CHECK(!f.Resolve()); } // Wrong class vtable.
    { Fixture f;f.Rel(0x210,0x1700); CHECK(!f.Resolve()); } // Body outside text.
    { Fixture f;f.Rel(0x222,0x1700); CHECK(!f.Resolve()); } // Free outside text.
    { Fixture f;f.data[0x21a]^=1; CHECK(!f.Resolve()); } // Wrong delete size.
    { Fixture f;f.data[0x320]=0;CHECK(!f.Resolve()); } // Missing valid body store.
    { Fixture f;f.Rel(0x45,0x1001); CHECK(!f.Resolve()); } // Unaligned vtable.
    { Fixture f;uintptr_t invalid=1;std::memcpy(f.data.data()+0x1000,&invalid,8);CHECK(!f.Resolve()); }
}
TEST(NpcActorLifetimeSites_RejectsInvalidRangesAndTranslatesFilePointersExplicitly) {
    Fixture f;const auto b=f.Base();
    CHECK(!NpcActorLifetimeSites::Resolve(b,b,b+0x1000,b+0x1800));
    CHECK(!NpcActorLifetimeSites::Resolve(b,b+0x1000,b+0x500,b+0x1800));
    CHECK(!NpcActorLifetimeSites::Resolve(b,b+0x1000,b+0x1000,b+0x1007));
    const uintptr_t preferred=0x140000000,stored=preferred+0x200;
    std::memcpy(f.data.data()+0x1000,&stored,8);
    CHECK(!f.Resolve());
    CHECK(NpcActorLifetimeSites::Resolve(b,b+0x1000,b+0x1000,b+0x1800,preferred,b));
    CHECK(!NpcActorLifetimeSites::Resolve(b,b+0x1000,b+0x1000,b+0x1800,preferred,0));
}
