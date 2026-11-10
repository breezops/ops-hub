#include "PoseHook.h"
#include "TestFramework.h"

#include <array>
#include <cstring>
#include <thread>
#include <atomic>
#include <Windows.h>

namespace {
using UpdateFn = void (*)(void*, void*, uint8_t*, const void*, uint64_t, uint64_t, uint64_t);
void NativeUpdate(void*, void*, uint8_t*, const void*, uint64_t, uint64_t, uint64_t) {}

PoseHook::Bone MakeBone(float p_Value) {
    PoseHook::Bone s_Bone {};
    s_Bone.rot[3] = 1.f;
    for (float& s_Pos : s_Bone.pos)
        s_Pos = p_Value;
    return s_Bone;
}

// Uses the actual Windows vtable hook and output-buffer layout, without
// launching/capturing HITMAN. It verifies the adapter, not native NPC spawning.
struct Fixture {
    void* table[2] { nullptr, reinterpret_cast<void*>(&NativeUpdate) };
    struct Animator { void** table; } animators[5];
    Fixture() {
        PoseHook::Uninstall();
        PoseHook::ClearExtraPoses();
        PoseHook::ClearTargetPose();
        PoseHook::SetTargetMap({});
        PoseHook::SetTargetMask({});
        for (auto& s_Animator : animators)
            s_Animator.table = table;
    }
    ~Fixture() { PoseHook::Uninstall(); }
    bool Install() { return PoseHook::Install(&animators[0]); }
    std::vector<PoseHook::Bone> Run(size_t p_Index, float p_Value = 1.f, uint32_t p_Count = 4, const void* world = nullptr) {
        std::vector<PoseHook::Bone> s_Bones(p_Count, MakeBone(p_Value));
        std::array<uint8_t, 24> s_Output {};
        auto* s_Data = s_Bones.data();
        uint32_t s_Count = static_cast<uint32_t>(s_Bones.size());
        std::memcpy(s_Output.data() + 8, &s_Data, sizeof(s_Data));
        std::memcpy(s_Output.data() + 16, &s_Count, sizeof(s_Count));
        reinterpret_cast<UpdateFn>(table[1])(&animators[p_Index], nullptr, s_Output.data(), world, 0, 0, 0);
        return s_Bones;
    }
};
std::array<float,16> World(coop::Quat q) {
    return {1-2*(q.y*q.y+q.z*q.z),2*(q.x*q.y+q.w*q.z),2*(q.x*q.z-q.w*q.y),0,
            2*(q.x*q.y-q.w*q.z),1-2*(q.x*q.x+q.z*q.z),2*(q.y*q.z+q.w*q.x),0,
            2*(q.x*q.z+q.w*q.y),2*(q.y*q.z-q.w*q.x),1-2*(q.x*q.x+q.y*q.y),0,
            10,20,30,1};
}
coop::Quat Rotation(const PoseHook::Bone& bone) {
    return {bone.rot[0],bone.rot[1],bone.rot[2],bone.rot[3]};
}
}

TEST(PlayerPoses_AnimationFrameTravelsWithSourceAndResetsOnRebind) {
    Fixture f;
    REQUIRE(f.Install());
    PoseHook::SetSource(&f.animators[0]);
    const auto q=coop::Normalize({.2f,.3f,.1f,.8f});
    auto matrix=World(q);
    f.Run(0,42,4,matrix.data());
    std::vector<PoseHook::Bone> bones;
    PoseHook::Frame frame;
    REQUIRE(PoseHook::TakeSourcePose(bones,0,&frame));
    CHECK(bones[0].pos[0]==42);
    CHECK(frame.hasWorldRotation);
    CHECK(coop::AngleBetween(frame.worldRotation,q)<.001f);
    f.Run(0,43,4,matrix.data());
    PoseHook::SetSource(&f.animators[1]);
    CHECK(!PoseHook::TakeSourcePose(bones,0,&frame));
    f.Run(1,44);
    REQUIRE(PoseHook::TakeSourcePose(bones,0,&frame));
    CHECK(!frame.hasWorldRotation);
    f.Run(1,45,4,matrix.data());
    PoseHook::Uninstall();
    CHECK(!PoseHook::TakeSourcePose(bones,0,&frame));
}

TEST(PlayerPoses_ThreeFramesPreserveFacingStrafingAndLocalChildBones) {
    Fixture f;
    REQUIRE(f.Install());
    for (uint8_t id=2;id<=4;++id) {
        REQUIRE(PoseHook::BindPlayerTarget(id,id,&f.animators[id-1]));
        const auto source=coop::Normalize({.1f*id,.3f,-.2f,.8f});
        const auto target=coop::Normalize({.3f,.1f*id,.4f,.5f});
        const auto root=coop::Normalize({.2f,.6f,-.3f,.1f*id});
        auto matrix=World(target);
        auto bone=MakeBone(.25f);
        bone.rot[0]=root.x;bone.rot[1]=root.y;bone.rot[2]=root.z;bone.rot[3]=root.w;
        REQUIRE(PoseHook::SetPlayerTargetPose(id,id,{bone,bone,bone},false,{}, {},4,{true,source}));
        const auto actual=f.Run(id-1,1,4,matrix.data());
        CHECK(coop::AngleBetween(coop::Multiply(target,Rotation(actual[0])),coop::Multiply(source,root))<.001f);
        CHECK(coop::AngleBetween(Rotation(actual[1]),root)<.001f);
        CHECK(actual[1].pos[0]==.25f);
        CHECK(actual[2].pos[0]==1); // Preserve target limb lengths.
        CHECK(actual[0].pos[3]==.25f);
        CHECK(coop::Distance(coop::Rotate(target,{actual[0].pos[0],actual[0].pos[1],actual[0].pos[2]}),
            coop::Rotate(source,{.25f,.25f,.25f}))<.0001f);
        // No movement vector is involved. The same facing survives strafing,
        // backward travel and an idle turn at the same world position.
        CHECK(coop::AngleBetween(Rotation(f.Run(id-1,99,4,matrix.data())[0]),Rotation(actual[0]))<.001f);
        CHECK(f.Run(id-1,1,5,matrix.data())[0].pos[0]==1); // Rig guard remains active.
        const auto fallback=f.Run(id-1,1,4,nullptr);
        CHECK(coop::AngleBetween(Rotation(fallback[0]),root)<.001f);
        PoseHook::FrameStats stats;
        REQUIRE(PoseHook::PlayerFrameStats(id,id,stats));
        CHECK(stats.rebased==2);
        CHECK(stats.unavailable==1);
        CHECK(!PoseHook::PlayerFrameStats(id,id+1,stats));
    }
}

TEST(PlayerPoses_UnreadableAndGuardedWorldFramesFallBackSafely) {
    Fixture f;
    REQUIRE(f.Install());
    PoseHook::SetSource(&f.animators[0]);
    std::vector<PoseHook::Bone> bones;
    PoseHook::Frame frame;
    f.Run(0,1,4,reinterpret_cast<void*>(1));
    REQUIRE(PoseHook::TakeSourcePose(bones,0,&frame));
    CHECK(!frame.hasWorldRotation);
    auto* memory=VirtualAlloc(nullptr,4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
    REQUIRE(memory!=nullptr);
    auto matrix=World({});
    std::memcpy(memory,matrix.data(),64);
    DWORD previous=0;
    const bool guarded=VirtualProtect(memory,4096,PAGE_READWRITE|PAGE_GUARD,&previous)!=0;
    CHECK(guarded);
    if (guarded) {
        f.Run(0,1,4,memory);
        CHECK(PoseHook::TakeSourcePose(bones,0,&frame));
        CHECK(!frame.hasWorldRotation);
        MEMORY_BASIC_INFORMATION info {};
        VirtualQuery(memory,&info,sizeof(info));
        CHECK((info.Protect&PAGE_GUARD)!=0); // Reading did not consume the guard.
    }
    VirtualFree(memory,0,MEM_RELEASE);
}

TEST(PlayerPoses_ThreeDistinctRigsNativeDiagnosticsAndLocalCapture) {
    Fixture f;
    REQUIRE(f.Install());
    PoseHook::SetSource(&f.animators[0]);
    for (uint8_t id = 2; id <= 4; ++id) {
        REQUIRE(PoseHook::BindPlayerTarget(id, 100 + id, &f.animators[id - 1]));
        std::vector<PoseHook::Bone> pose { MakeBone(10.f * id), MakeBone(10.f * id + 1), MakeBone(10.f * id + 2) };
        // Different maps/masks/translation policies for each remote player.
        REQUIRE(PoseHook::SetPlayerTargetPose(id, 100 + id, pose, id == 4,
            id == 3 ? std::vector<int32_t>{2, -1, 0, 999} : std::vector<int32_t>{},
            id == 2 ? std::vector<uint8_t>{0, 1, 1} : std::vector<uint8_t>{}));
    }
    const auto a = f.Run(1);
    const auto b = f.Run(2);
    const auto c = f.Run(3);
    CHECK(a[0].pos[0] == 1.f); // Masked root.
    CHECK(a[1].pos[0] == 21.f);
    CHECK(a[2].pos[0] == 1.f); // Preserve NPC limb lengths.
    CHECK(b[0].pos[0] == 32.f);
    CHECK(b[1].pos[0] == 1.f); // Unmapped bone.
    CHECK(b[3].pos[0] == 1.f); // Out-of-range map ignored.
    CHECK(c[0].pos[0] == 40.f);
    CHECK(c[2].pos[0] == 42.f); // All-position rig.
    CHECK(c[3].pos[0] == 1.f); // Short source bounded.
    std::vector<PoseHook::Bone> native;
    for (uint8_t id = 2; id <= 4; ++id) {
        REQUIRE(PoseHook::PlayerTargetNativePose(id, 100 + id, native));
        CHECK(native.size() == 4);
        CHECK(native[0].pos[0] == 1.f);
    }
    f.Run(0, 77.f);
    REQUIRE(PoseHook::TakeSourcePose(native));
    CHECK(native[0].pos[0] == 77.f);
    CHECK(!PoseHook::TakeSourcePose(native));
}

TEST(PlayerPoses_ReconnectClearsRigAndRejectsOldIncarnation) {
    Fixture f;
    REQUIRE(f.Install());
    REQUIRE(PoseHook::BindPlayerTarget(4, 10, &f.animators[0]));
    REQUIRE(PoseHook::SetPlayerTargetPose(4, 10, {MakeBone(40.f)}, true));
    CHECK(f.Run(0)[0].pos[0] == 40.f);
    REQUIRE(PoseHook::BindPlayerTarget(4, 10, &f.animators[0]));
    CHECK(f.Run(0)[0].pos[0] == 40.f); // Idempotent roster refresh.
    REQUIRE(PoseHook::BindPlayerTarget(4, 11, &f.animators[1]));
    CHECK(!PoseHook::SetPlayerTargetPose(4, 10, {MakeBone(99.f)}, true));
    CHECK(!PoseHook::ClearPlayerTarget(4, 10));
    std::vector<PoseHook::Bone> native {MakeBone(99.f)};
    CHECK(!PoseHook::PlayerTargetNativePose(4, 11, native));
    CHECK(native.empty());
    CHECK(f.Run(0)[0].pos[0] == 1.f); // Old animator detached.
    CHECK(f.Run(1)[0].pos[0] == 1.f); // No stale pose on replacement.
    REQUIRE(PoseHook::SetPlayerTargetPose(4, 11, {MakeBone(50.f)}, true));
    CHECK(f.Run(1)[0].pos[0] == 50.f);
    REQUIRE(PoseHook::ClearPlayerTarget(4, 11));
    CHECK(f.Run(1)[0].pos[0] == 1.f);
    CHECK(!PoseHook::SetPlayerTargetPose(4, 11, {MakeBone(60.f)}, true));
}

TEST(PlayerPoses_CapacityIdentityBoundsAndDuplicateWriters) {
    Fixture f;
    REQUIRE(f.Install());
    CHECK(!PoseHook::BindPlayerTarget(0, 1, &f.animators[0]));
    CHECK(!PoseHook::BindPlayerTarget(5, 1, &f.animators[0]));
    CHECK(!PoseHook::BindPlayerTarget(1, 0, &f.animators[0]));
    CHECK(!PoseHook::BindPlayerTarget(1, 1, nullptr));
    PoseHook::SetTarget(&f.animators[0]);
    CHECK(!PoseHook::BindPlayerTarget(1, 1, &f.animators[0]));
    PoseHook::SetTarget(nullptr);
    REQUIRE(PoseHook::SetPoseFor(&f.animators[0], {MakeBone(5.f)}));
    CHECK(!PoseHook::BindPlayerTarget(1, 1, &f.animators[0]));
    PoseHook::ClearPoseFor(&f.animators[0]);
    for (uint8_t id = 1; id <= 3; ++id)
        REQUIRE(PoseHook::BindPlayerTarget(id, id, &f.animators[id - 1]));
    CHECK(!PoseHook::BindPlayerTarget(4, 4, &f.animators[3]));
    CHECK(!PoseHook::BindPlayerTarget(2, 2, &f.animators[0]));
    CHECK(!PoseHook::SetPoseFor(&f.animators[0], {MakeBone(5.f)}));
    CHECK(!PoseHook::SetPlayerTargetPose(1, 1, std::vector<PoseHook::Bone>(1024), true));
    REQUIRE(PoseHook::ClearPlayerTarget(2, 2));
    REQUIRE(PoseHook::BindPlayerTarget(4, 4, &f.animators[3]));
    PoseHook::ClearPlayerTargets();
    CHECK(!PoseHook::SetPlayerTargetPose(4, 4, {MakeBone(50.f)}, true));
}

TEST(PlayerPoses_ConcurrentWorkerClearAndRebind) {
    Fixture f;
    REQUIRE(f.Install());
    std::atomic<bool> running {true};
    std::atomic<bool> valid {true};
    std::thread worker([&] {
        while (running.load()) {
            const float value = f.Run(0)[0].pos[0];
            if (value != 1.f && value != 40.f)
                valid = false;
        }
    });
    for (uint64_t incarnation = 1; incarnation <= 2000; ++incarnation) {
        CHECK(PoseHook::BindPlayerTarget(4, incarnation, &f.animators[0]));
        CHECK(PoseHook::SetPlayerTargetPose(4, incarnation, {MakeBone(40.f)}, true));
        CHECK(PoseHook::ClearPlayerTarget(4, incarnation));
    }
    running = false;
    worker.join();
    CHECK(valid.load());
    // Uninstall only after workers join, matching engine hook shutdown rules.
    PoseHook::Uninstall();
    CHECK(f.table[1] == reinterpret_cast<void*>(&NativeUpdate));
    REQUIRE(f.Install());
    CHECK(f.Run(0)[0].pos[0] == 1.f);
}

TEST(PlayerPoses_RotationHemisphereAndIndependentLegacyTarget) {
    Fixture f;
    REQUIRE(f.Install());
    PoseHook::SetTarget(&f.animators[0]);
    PoseHook::SetTargetPose({MakeBone(12.f)}, true);
    REQUIRE(PoseHook::BindPlayerTarget(3, 10, &f.animators[1]));
    auto bone = MakeBone(30.f);
    bone.rot[0] = 0.6f;
    bone.rot[3] = -0.8f;
    REQUIRE(PoseHook::SetPlayerTargetPose(3, 10, {bone}, true));
    auto result = f.Run(1);
    CHECK(result[0].rot[0] == -0.6f);
    CHECK(result[0].rot[3] == 0.8f); // Equivalent quaternion on native hemisphere.
    CHECK(result[0].pos[0] == 30.f);
    CHECK(f.Run(0)[0].pos[0] == 12.f);
    PoseHook::SetTarget(&f.animators[1]); // Refused: would overwrite player 3.
    CHECK(f.Run(0)[0].pos[0] == 12.f);
    CHECK(f.Run(1)[0].pos[0] == 30.f);
    PoseHook::ClearPlayerTargets();
    CHECK(f.Run(0)[0].pos[0] == 12.f);
    CHECK(f.Run(1)[0].pos[0] == 1.f);
}

TEST(PlayerPoses_UninstallClearsEveryRemoteRigBeforeReinstall) {
    Fixture f;
    REQUIRE(f.Install());
    REQUIRE(PoseHook::BindPlayerTarget(2, 7, &f.animators[0]));
    REQUIRE(PoseHook::SetPlayerTargetPose(2, 7, {MakeBone(11.f)}, true, {0, 0, 0}, {1, 1, 1}));
    CHECK(f.Run(0)[2].pos[0] == 11.f);
    PoseHook::Uninstall();
    REQUIRE(f.Install());
    REQUIRE(PoseHook::BindPlayerTarget(2, 7, &f.animators[0]));
    CHECK(f.Run(0)[0].pos[0] == 1.f);
    REQUIRE(PoseHook::SetPlayerTargetPose(2, 7, {MakeBone(15.f)}, false));
    const auto result = f.Run(0);
    CHECK(result[0].pos[0] == 15.f);
    CHECK(result[1].pos[0] == 1.f);
    CHECK(result[2].pos[0] == 1.f);
}

TEST(PlayerPoses_OutfitBodyCountChangeNeverUsesThePreviousRigMap) {
    Fixture f;
    REQUIRE(f.Install());
    REQUIRE(PoseHook::BindPlayerTarget(2,10,&f.animators[1]));
    REQUIRE(PoseHook::BindPlayerTarget(4,30,&f.animators[3]));
    std::vector<PoseHook::Bone> pose(4,MakeBone(42.f));
    REQUIRE(PoseHook::SetPlayerTargetPose(2,10,pose,false,{}, {},4));
    REQUIRE(PoseHook::SetPlayerTargetPose(4,30,pose,false,{}, {},4));
    CHECK(f.Run(1)[0].pos[0]==42.f);
    CHECK(f.Run(1,7.f,5)[0].pos[0]==7.f); // new body retains native pose
    std::vector<PoseHook::Bone> native;
    REQUIRE(PoseHook::PlayerTargetNativePose(2,10,native));
    CHECK(native.size()==5); // game thread can now rematch the new rig
    CHECK(f.Run(3)[0].pos[0]==42.f); // other player is unaffected
    REQUIRE(PoseHook::SetPlayerTargetPose(2,10,pose,false,{0,1,2,3,-1},{},5));
    auto rematched=f.Run(1,7.f,5);
    CHECK(rematched[0].pos[0]==42.f);
    CHECK(rematched[4].pos[0]==7.f);
    CHECK(!PoseHook::SetPlayerTargetPose(2,10,pose,false,{}, {},1024));
    CHECK(f.Run(1,7.f,4)[0].pos[0]==7.f); // stale/invalid map still cannot apply
    REQUIRE(PoseHook::SetPlayerTargetPose(2,10,{},false));
    CHECK(f.Run(1,7.f,5)[0].pos[0]==7.f);
}
