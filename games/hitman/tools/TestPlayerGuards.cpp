#include "CrashGuards.h"
#include "TestFramework.h"
#include <Windows.h>
#include <array>
#include <algorithm>
#include <cstring>

namespace {
struct Actor {
    alignas(8) std::array<uint8_t, 64> bytes {};
    explicit Actor(int32_t id) {
        std::memcpy(bytes.data() + 8, &id, 4);
        uintptr_t controller = 1;
        std::memcpy(bytes.data() + 24, &controller, 8);
    }
    void* Ptr() { return bytes.data(); }
    void* Speaker() { return bytes.data() + 16; }
    void NoVoice() { std::memset(bytes.data() + 24, 0, 8); }
};
struct Fixture {
    std::array<uint8_t, 1024> units {};
    Fixture() { CrashGuardsTest::Configure(units.data()); }
    ~Fixture() { CrashGuardsTest::Configure(nullptr); }
    void* Unit(int id) { return units.data() + id * 32; }
};

// Execute the production speech stub with synthetic actor memory and explicit
// resume/silent return paths. No HITMAN executable code or process is patched.
class SpeechHarness {
public:
    uint8_t* code = nullptr;
    SpeechHarness() {
        code = static_cast<uint8_t*>(VirtualAlloc(nullptr, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
        if (!code)
            return;
        const uint8_t save[] {0x55, 0x56, 0x41, 0x56, 0x41, 0x57}; // rbp,rsi,r14,r15
        std::memcpy(code, save, sizeof(save));
        const auto stub = CrashGuardsTest::SpeechStub(reinterpret_cast<uintptr_t>(code + 1024), reinterpret_cast<uintptr_t>(code + 1100));
        if (stub.size() + sizeof(save) >= 1024) {
            VirtualFree(code, 0, MEM_RELEASE);
            code = nullptr;
            return;
        }
        std::memcpy(code + sizeof(save), stub.data(), stub.size());
        const uint8_t resume[] {0xB8, 17, 0, 0, 0, 0x41, 0x5F, 0x41, 0x5E, 0x5E, 0x5D, 0xC3};
        const uint8_t silent[] {0xB8, 42, 0, 0, 0, 0x41, 0x5F, 0x41, 0x5E, 0x5E, 0x5D, 0xC3};
        std::memcpy(code + 1024, resume, sizeof(resume));
        std::memcpy(code + 1100, silent, sizeof(silent));
        DWORD old;
        if (!VirtualProtect(code, 4096, PAGE_EXECUTE_READ, &old)) {
            VirtualFree(code, 0, MEM_RELEASE);
            code = nullptr;
            return;
        }
        FlushInstructionCache(GetCurrentProcess(), code, 4096);
    }
    ~SpeechHarness() { if (code) VirtualFree(code, 0, MEM_RELEASE); }
    int Run(Actor& actor) {
        return reinterpret_cast<int (*)(void*, void*, void*, void*)>(code)(actor.Speaker(), nullptr, nullptr, nullptr);
    }
};
}

TEST(PlayerGuards_ThreeKnowledgeRecordsAndIndependentWarmup) {
    Fixture f;
    Actor a(1), b(2), c(3), npc(4);
    int ka = 0, kb = 0, kc = 0;
    REQUIRE(CrashGuards::SetPlayerActor(2, 100, a.Ptr()));
    REQUIRE(CrashGuards::SetPlayerActor(3, 200, b.Ptr()));
    REQUIRE(CrashGuards::SetPlayerActor(4, 300, c.Ptr()));
    REQUIRE(CrashGuards::SetPlayerKnowledge(2, 100, &ka, false));
    REQUIRE(CrashGuards::SetPlayerKnowledge(3, 200, &kb, true)); // Needs native initialization.
    REQUIRE(CrashGuards::SetPlayerKnowledge(4, 300, &kc, false));
    CHECK(CrashGuards::IsStandInKnowledge(&ka));
    CHECK(CrashGuards::IsStandInKnowledge(&kb));
    CHECK(CrashGuards::IsStandInKnowledge(&kc));
    CHECK(!CrashGuards::IsStandInKnowledge(nullptr));
    CHECK(!CrashGuards::IsStandInKnowledge(&npc));
    auto units = CrashGuardsTest::SkippedUnits();
    CHECK(units == std::vector<void*>({f.Unit(1), f.Unit(3)}));
    REQUIRE(CrashGuards::SetPlayerKnowledge(3, 200, &kb, false));
    CHECK(CrashGuardsTest::SkippedUnits().size() == 3);
    CrashGuards::SetPuppet(npc.Ptr(), true);
    CHECK(CrashGuardsTest::SkippedUnits().size() == 4);
    REQUIRE(CrashGuards::ClearPlayer(3, 200));
    CHECK(!CrashGuards::IsStandInKnowledge(&kb));
    CHECK(CrashGuards::IsStandInKnowledge(&ka));
    CHECK(CrashGuards::IsStandInKnowledge(&kc));
    units = CrashGuardsTest::SkippedUnits();
    CHECK(units.size() == 3);
    CHECK(std::find(units.begin(), units.end(), f.Unit(2)) == units.end());
    CHECK(std::find(units.begin(), units.end(), f.Unit(4)) != units.end());
}

TEST(PlayerGuards_ReconnectRejectsOldUpdatesAndRetainsRestartHistory) {
    Fixture f;
    Actor oldActor(1), replacement(2);
    int oldKnowledge = 0, newKnowledge = 0;
    REQUIRE(CrashGuards::SetPlayerActor(4, 5, oldActor.Ptr()));
    REQUIRE(CrashGuards::SetPlayerKnowledge(4, 5, &oldKnowledge, false));
    REQUIRE(CrashGuards::SetPlayerActor(4, 6, replacement.Ptr()));
    CHECK(!CrashGuards::IsStandInKnowledge(&oldKnowledge));
    CHECK(CrashGuardsTest::SkippedUnits().empty());
    REQUIRE(CrashGuards::SetPlayerKnowledge(4, 6, &newKnowledge, false));
    CHECK(!CrashGuards::SetPlayerKnowledge(4, 5, &oldKnowledge, false));
    CHECK(!CrashGuards::ClearPlayer(4, 5));
    CHECK(CrashGuards::IsStandInKnowledge(&newKnowledge));
    CHECK(CrashGuardsTest::SkippedUnits() == std::vector<void*>({f.Unit(2)}));
    REQUIRE(CrashGuards::ClearPlayer(4, 6));
    CHECK(CrashGuardsTest::SkippedUnits().empty());
    CHECK(!CrashGuards::IsStandInKnowledge(&newKnowledge));
    CHECK(CrashGuardsTest::Remembered(oldActor.Ptr()));
    CHECK(CrashGuardsTest::Remembered(replacement.Ptr()));
}

TEST(PlayerGuards_ActualSpeechStubMutesThreeSparsePlayersAndLegacyOnly) {
    Fixture f;
    Actor legacy(0), a(1), b(2), c(3), normal(4);
    CrashGuards::SetActor(legacy.Ptr());
    REQUIRE(CrashGuards::SetPlayerActor(2, 10, a.Ptr()));
    REQUIRE(CrashGuards::SetPlayerActor(3, 20, b.Ptr()));
    REQUIRE(CrashGuards::SetPlayerActor(4, 30, c.Ptr()));
    SpeechHarness harness;
    REQUIRE(harness.code != nullptr);
    const auto all = CrashGuards::AllLines();
    const auto muted = CrashGuards::MutedLines();
    CHECK(harness.Run(legacy) == 42);
    CHECK(harness.Run(a) == 42);
    CHECK(harness.Run(b) == 42);
    CHECK(harness.Run(c) == 42);
    CHECK(harness.Run(normal) == 17);
    CHECK(CrashGuards::MutedLines() == muted + 4);
    REQUIRE(CrashGuards::ClearPlayer(3, 20));
    CHECK(harness.Run(b) == 17);
    CHECK(harness.Run(c) == 42); // Empty earlier slot must not hide player 4.
    normal.NoVoice();
    CHECK(harness.Run(normal) == 42);
    CHECK(CrashGuards::MutedLines() == muted + 5); // Ordinary no-voice NPC is not counted.
    CHECK(CrashGuards::AllLines() == all + 8);
    CrashGuards::ClearPlayers();
    CHECK(harness.Run(a) == 17);
    CHECK(harness.Run(c) == 17);
    CHECK(harness.Run(legacy) == 42);
}

TEST(PlayerGuards_IdentityCapacityAndLegacyKnowledgeStayIndependent) {
    Fixture f;
    Actor legacy(0), a(1), b(2), c(3), fourth(4);
    int legacyKnowledge = 0, knowledge = 0;
    CrashGuards::SetActor(legacy.Ptr());
    CrashGuards::SetKnowledge(&legacyKnowledge, false);
    CHECK(!CrashGuards::SetPlayerActor(0, 1, a.Ptr()));
    CHECK(!CrashGuards::SetPlayerActor(5, 1, a.Ptr()));
    CHECK(!CrashGuards::SetPlayerActor(1, 0, a.Ptr()));
    CHECK(!CrashGuards::SetPlayerActor(1, 1, nullptr));
    CHECK(!CrashGuards::SetPlayerActor(1, 1, legacy.Ptr()));
    REQUIRE(CrashGuards::SetPlayerActor(1, 1, a.Ptr()));
    REQUIRE(CrashGuards::SetPlayerActor(2, 2, b.Ptr()));
    REQUIRE(CrashGuards::SetPlayerActor(3, 3, c.Ptr()));
    CrashGuards::SetActor(a.Ptr()); // Refused: actor already belongs to room player 1.
    SpeechHarness harness;
    REQUIRE(harness.code != nullptr);
    CHECK(harness.Run(legacy) == 42); // Legacy slot must still belong to legacy actor.
    CHECK(!CrashGuards::SetPlayerActor(4, 4, fourth.Ptr()));
    CHECK(!CrashGuards::SetPlayerActor(2, 2, a.Ptr()));
    REQUIRE(CrashGuards::SetPlayerKnowledge(1, 1, &knowledge, false));
    REQUIRE(CrashGuards::SetPlayerActor(1, 1, a.Ptr()));
    CHECK(CrashGuards::IsStandInKnowledge(&knowledge)); // Idempotent roster update.
    CrashGuards::ClearPlayers();
    CHECK(!CrashGuards::IsStandInKnowledge(&knowledge));
    CHECK(CrashGuards::IsStandInKnowledge(&legacyKnowledge));
    CHECK(CrashGuardsTest::SkippedUnits() == std::vector<void*>({f.Unit(0)}));
}


namespace {
class InteractionHarness {
public:
    uint8_t* code = nullptr;
    InteractionHarness() {
        code = static_cast<uint8_t*>(VirtualAlloc(nullptr, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
        if (!code) return;
        const uint8_t save[] {0x41,0x56,0x4C,0x8B,0xF1}; // save r14; candidate(rcx) -> r14
        std::memcpy(code,save,sizeof(save));
        // Use the actual overwritten native instructions. The normal path's
        // original cmp must leave ZF intact for the following game branch.
        const std::vector<uint8_t> original {0x49,0x8B,0x86,0xB8,0x10,0,0,
            0x48,0x8B,0x48,0x50,0x83,0xB9,0,1,0,0,0};
        const auto stub=CrashGuardsTest::InteractionStub(original,
            reinterpret_cast<uintptr_t>(code+1024),reinterpret_cast<uintptr_t>(code+1100));
        std::memcpy(code+sizeof(save),stub.data(),stub.size());
        const uint8_t resume[] {0x75,8,0xB8,17,0,0,0,0x41,0x5E,0xC3,
            0xB8,23,0,0,0,0x41,0x5E,0xC3};
        const uint8_t reject[] {0xB8,42,0,0,0,0x41,0x5E,0xC3};
        std::memcpy(code+1024,resume,sizeof(resume));
        std::memcpy(code+1100,reject,sizeof(reject));
        DWORD old;
        if (!VirtualProtect(code,4096,PAGE_EXECUTE_READ,&old)) {
            VirtualFree(code,0,MEM_RELEASE);code=nullptr;return;
        }
        FlushInstructionCache(GetCurrentProcess(),code,4096);
    }
    ~InteractionHarness() { if(code) VirtualFree(code,0,MEM_RELEASE); }
    int Run(void* actor) { return reinterpret_cast<int(*)(void*)>(code)(actor); }
};
struct PickerActor {
    alignas(8) std::array<uint8_t,0x1100> actor {};
    alignas(8) std::array<uint8_t,0x60> animated {};
    alignas(8) std::array<uint8_t,0x110> state {};
    PickerActor() {
        void* a=animated.data();void* s=state.data();
        std::memcpy(actor.data()+0x10b8,&a,8);
        std::memcpy(animated.data()+0x50,&s,8);
    }
    void Busy() { state[0x100]=1; }
    void* Ptr() { return actor.data(); }
};
}
TEST(PlayerGuards_ActualInteractionStubRejectsOnlyOwnedActorsAndPreservesNormalFlags) {
    Fixture f;
    PickerActor legacy,a,b,c,normal;
    CrashGuards::SetActor(legacy.Ptr());
    REQUIRE(CrashGuards::SetPlayerActor(1,10,a.Ptr()));
    REQUIRE(CrashGuards::SetPlayerActor(3,20,b.Ptr()));
    REQUIRE(CrashGuards::SetPlayerActor(4,30,c.Ptr()));
    InteractionHarness harness;
    REQUIRE(harness.code!=nullptr);
    CHECK(harness.Run(legacy.Ptr())==42);
    CHECK(harness.Run(a.Ptr())==42);
    CHECK(harness.Run(b.Ptr())==42);
    CHECK(harness.Run(c.Ptr())==42);
    CHECK(harness.Run(normal.Ptr())==17);
    normal.Busy();
    CHECK(harness.Run(normal.Ptr())==23);
    CHECK(!CrashGuards::ClearPlayer(3,19));
    CHECK(harness.Run(b.Ptr())==42);
    REQUIRE(CrashGuards::ClearPlayer(3,20));
    CHECK(harness.Run(b.Ptr())==17);
    CHECK(harness.Run(c.Ptr())==42); // sparse list cannot terminate at an empty slot
    CrashGuards::ClearPlayers();
    CHECK(harness.Run(a.Ptr())==17);
    CHECK(harness.Run(c.Ptr())==17);
    CHECK(harness.Run(legacy.Ptr())==42);
    CrashGuards::SetActor(nullptr);
    CHECK(harness.Run(legacy.Ptr())==17);
}
