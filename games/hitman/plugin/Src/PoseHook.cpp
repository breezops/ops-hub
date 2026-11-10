#include "PoseHook.h"

#include <Windows.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstring>
#include <mutex>

namespace {
// The pose update entry's arguments: (this, input pose, output pose, world matrix, then up to
// three more that are passed through untouched).
using UpdateFn = void (*)(void*, void*, uint8_t*, const void*, uint64_t, uint64_t, uint64_t);

constexpr size_t c_UpdateSlot = 1;

std::mutex g_Mutex;
void** g_Table = nullptr;
UpdateFn g_Original = nullptr;

constexpr size_t c_Channels = 2;
constexpr size_t c_Extras = 64;  // the partner's victim and their NPCs near us (puppets)
constexpr size_t c_Watched = 40; // our NPCs near the partner, whose poses we send (Session's npcPoseMax and some)

std::atomic<void*> g_Sources[c_Channels] {};
std::atomic<void*> g_Target { nullptr };
std::atomic<void*> g_Extras[c_Extras] {};
std::vector<PoseHook::Bone> g_ExtraPoses[c_Extras];
std::vector<PoseHook::Bone> g_ExtraNative[c_Extras]; // the target's own first bones, before ours
uint32_t g_ExtraNativeCount[c_Extras] {};
bool g_ExtraExactCount[c_Extras] {};
std::atomic<void*> g_Watched[c_Watched] {};
std::vector<PoseHook::Bone> g_WatchedPoses[c_Watched];
bool g_WatchedFresh[c_Watched] {};
std::atomic<uint64_t> g_Calls { 0 };
std::atomic<uint64_t> g_TargetCalls { 0 }; // the target's own pose updates (is it still animating?)
std::atomic<uint32_t> g_SourceCount { 0 };
std::atomic<uint32_t> g_TargetCount { 0 };

std::vector<PoseHook::Bone> g_SourcePoses[c_Channels];
bool g_SourceFresh[c_Channels] {};
PoseHook::Frame g_SourceFrames[c_Channels];
std::vector<PoseHook::Bone> g_TargetPose;
bool g_HasTargetPose = false;
bool g_AllPositions = false;
std::vector<PoseHook::Bone> g_TargetNative; // the target's own pose, before ours
std::vector<uint8_t> g_TargetMask;          // bones of ours to use (empty: all)
std::vector<int32_t> g_TargetMap; // per target bone, the source bone it copies (-1: its own; empty: same index)

struct PlayerTarget {
    void* animator = nullptr;
    uint64_t incarnation = 0;
    std::vector<PoseHook::Bone> pose;
    std::vector<PoseHook::Bone> native;
    std::vector<int32_t> map;
    std::vector<uint8_t> mask;
    bool allPositions = false;
    uint32_t expectedBoneCount = 0;
    PoseHook::Frame frame;
    PoseHook::FrameStats stats;
};
// Atomic pointers are only a fast rejection filter. Every match is checked
// again under g_Mutex before touching the corresponding player record.
std::atomic<void*> g_PlayerAnimators[4] {};
PlayerTarget g_PlayerTargets[4];

// Writes p_Rot over p_Bone's rotation, on the same side of the 4D sphere as the rotation already
// there. q and -q are the same rotation, but the engine blends with the bone's own value, and our
// sign-normalised copies (smallest-three packing) flipped an NPC upside down.
void WriteRotation(PoseHook::Bone& p_Bone, const float* p_Rot) {
    const float s_Dot = p_Bone.rot[0] * p_Rot[0] + p_Bone.rot[1] * p_Rot[1] + p_Bone.rot[2] * p_Rot[2] + p_Bone.rot[3] * p_Rot[3];
    const float s_Sign = s_Dot < 0.f ? -1.f : 1.f;

    for (int i = 0; i < 4; ++i)
        p_Bone.rot[i] = p_Rot[i] * s_Sign;
}

bool ReadablePose(const uint8_t* p_Out, PoseHook::Bone*& p_Bones, uint32_t& p_Count) {
    if (!p_Out)
        return false;

    std::memcpy(&p_Bones, p_Out + 0x08, sizeof(p_Bones));
    std::memcpy(&p_Count, p_Out + 0x10, sizeof(p_Count));
    return p_Bones && p_Count > 0 && p_Count < 1024;
}

// The fourth argument is the animation world matrix in HITMAN 3.290.1.
// Read only committed, readable pages; never touch a guard page.
PoseHook::Frame ReadFrame(const void* world) {
    PoseHook::Frame frame;
    if (!world) return frame;
    auto address = reinterpret_cast<uintptr_t>(world);
    if (address > UINTPTR_MAX - 64) return frame;
    for (auto cursor = address; cursor < address + 64;) {
        MEMORY_BASIC_INFORMATION info {};
        if (!VirtualQuery(reinterpret_cast<void*>(cursor), &info, sizeof(info)) ||
            info.State != MEM_COMMIT || (info.Protect & (PAGE_GUARD | PAGE_NOACCESS)) ||
            !(info.Protect & (PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY |
                             PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY))) return frame;
        const auto end = reinterpret_cast<uintptr_t>(info.BaseAddress) + info.RegionSize;
        if (end <= cursor) return frame;
        cursor = end;
    }
    float matrix[16];
    SIZE_T bytes = 0;
    if (ReadProcessMemory(GetCurrentProcess(), world, matrix, sizeof(matrix), &bytes) && bytes == sizeof(matrix))
        frame.hasWorldRotation = coop::PoseWorldRotation(matrix, frame.worldRotation);
    return frame;
}

void Update(void* p_This, void* p_In, uint8_t* p_Out, const void* p_World, uint64_t p_A5, uint64_t p_A6, uint64_t p_A7) {
    g_Original(p_This, p_In, p_Out, p_World, p_A5, p_A6, p_A7);
    g_Calls.fetch_add(1, std::memory_order_relaxed);

    int s_Source = -1;
    int s_Extra = -1;

    for (size_t i = 0; i < c_Channels; ++i)
        if (p_This == g_Sources[i].load(std::memory_order_relaxed))
            s_Source = static_cast<int>(i);

    for (size_t i = 0; i < c_Extras; ++i)
        if (p_This == g_Extras[i].load(std::memory_order_relaxed))
            s_Extra = static_cast<int>(i);

    const bool s_IsTarget = p_This == g_Target.load(std::memory_order_relaxed);
    int s_Player = -1;
    for (size_t i = 0; i < 4; ++i)
        if (p_This == g_PlayerAnimators[i].load(std::memory_order_relaxed))
            s_Player = static_cast<int>(i);
    int s_Watched = -1;

    for (size_t i = 0; i < c_Watched; ++i)
        if (p_This == g_Watched[i].load(std::memory_order_relaxed))
            s_Watched = static_cast<int>(i);

    if (s_Source < 0 && s_Extra < 0 && !s_IsTarget && s_Watched < 0 && s_Player < 0)
        return;

    PoseHook::Bone* s_Bones = nullptr;
    uint32_t s_Count = 0;

    if (!ReadablePose(p_Out, s_Bones, s_Count))
        return;

    std::lock_guard s_Lock(g_Mutex);

    if (s_Player >= 0 && g_PlayerTargets[s_Player].animator == p_This) {
        auto& s_Target = g_PlayerTargets[s_Player];
        s_Target.native.assign(s_Bones, s_Bones + s_Count);
        // A body can change on an animation worker before the game thread has
        // rematched it. Record its native pose, but never apply the stale map
        // to a different-sized rig in that intervening frame.
        const size_t s_Copy = s_Target.expectedBoneCount && s_Target.expectedBoneCount != s_Count ? 0 :
            std::min<size_t>(s_Count, s_Target.map.empty() ? s_Target.pose.size() : s_Target.map.size());
        const auto targetFrame = s_Target.frame.hasWorldRotation ? ReadFrame(p_World) : PoseHook::Frame{};
        const bool rebase = s_Target.frame.hasWorldRotation && targetFrame.hasWorldRotation;
        if (s_Copy && !rebase) ++s_Target.stats.unavailable;
        const auto delta = coop::Multiply(coop::InverseRotation(targetFrame.worldRotation), s_Target.frame.worldRotation);
        for (size_t i = 0; i < s_Copy; ++i) {
            if (i < s_Target.mask.size() && !s_Target.mask[i])
                continue;
            const int32_t s_From = s_Target.map.empty() ? static_cast<int32_t>(i) : s_Target.map[i];
            if (s_From < 0 || static_cast<size_t>(s_From) >= s_Target.pose.size())
                continue;
            auto bone = s_Target.pose[s_From];
            // Only the true skeleton root changes reference frame. Child bones
            // remain parent-relative; movement direction never substitutes for facing.
            if (rebase && i == 0 && s_From == 0) {
                ++s_Target.stats.rebased;
                s_Target.stats.lastCorrectionRadians = coop::AngleBetween({}, delta);
                const auto q = coop::Normalize(coop::Multiply(delta, {bone.rot[0],bone.rot[1],bone.rot[2],bone.rot[3]}));
                const auto pos = coop::Rotate(delta, {bone.pos[0],bone.pos[1],bone.pos[2]});
                bone.rot[0]=q.x; bone.rot[1]=q.y; bone.rot[2]=q.z; bone.rot[3]=q.w;
                bone.pos[0]=pos.x; bone.pos[1]=pos.y; bone.pos[2]=pos.z;
            }
            WriteRotation(s_Bones[i], bone.rot);
            if (s_Target.allPositions || i < 2)
                std::memcpy(s_Bones[i].pos, bone.pos, sizeof(s_Bones[i].pos));
        }
    }

    if (s_Watched >= 0 && g_Watched[s_Watched] == p_This) {
        g_WatchedPoses[s_Watched].assign(s_Bones, s_Bones + s_Count);
        g_WatchedFresh[s_Watched] = true;
    }

    if (s_Source >= 0 && p_This == g_Sources[s_Source].load()) {
        if (s_Source == 0)
            g_SourceCount = s_Count;

        g_SourcePoses[s_Source].assign(s_Bones, s_Bones + s_Count);
        g_SourceFrames[s_Source] = ReadFrame(p_World);
        g_SourceFresh[s_Source] = true;
    }

    if (s_Extra >= 0 && p_This == g_Extras[s_Extra].load(std::memory_order_relaxed)) {
        const auto& s_Pose = g_ExtraPoses[s_Extra];
        const uint32_t s_Copy = g_ExtraExactCount[s_Extra] && s_Count!=s_Pose.size() ? 0 :
            std::min<uint32_t>(s_Count, static_cast<uint32_t>(s_Pose.size()));
        g_ExtraNative[s_Extra].assign(s_Bones, s_Bones + s_Count);
        g_ExtraNativeCount[s_Extra] = s_Count;

        for (uint32_t i = 0; i < s_Copy; ++i) {
            WriteRotation(s_Bones[i], s_Pose[i].rot);

            if (i < 2)
                std::memcpy(s_Bones[i].pos, s_Pose[i].pos, sizeof(s_Bones[i].pos));
        }
    }

    if (s_IsTarget && p_This == g_Target.load(std::memory_order_relaxed)) {
        g_TargetCalls.fetch_add(1, std::memory_order_relaxed);
        g_TargetCount = s_Count;
        g_TargetNative.assign(s_Bones, s_Bones + s_Count);

        if (!g_HasTargetPose)
            return;

        const uint32_t s_Copy = g_TargetMap.empty() ? std::min<uint32_t>(s_Count, static_cast<uint32_t>(g_TargetPose.size()))
                                                    : std::min<uint32_t>(s_Count, static_cast<uint32_t>(g_TargetMap.size()));

        for (uint32_t i = 0; i < s_Copy; ++i) {
            if (i < g_TargetMask.size() && !g_TargetMask[i])
                continue;

            const int32_t s_From = g_TargetMap.empty() ? static_cast<int32_t>(i) : g_TargetMap[i];

            if (s_From < 0 || static_cast<size_t>(s_From) >= g_TargetPose.size())
                continue;

            const PoseHook::Bone& s_Source = g_TargetPose[s_From];

            WriteRotation(s_Bones[i], s_Source.rot);

            if (g_AllPositions || i < 2)
                std::memcpy(s_Bones[i].pos, s_Source.pos, sizeof(s_Bones[i].pos));
        }
    }
}

bool Patch(void** p_Slot, void* p_Value) {
    DWORD s_Old = 0;

    if (!VirtualProtect(p_Slot, sizeof(void*), PAGE_READWRITE, &s_Old))
        return false;

    InterlockedExchangePointer(p_Slot, p_Value);
    VirtualProtect(p_Slot, sizeof(void*), s_Old, &s_Old);
    return true;
}
}

bool PoseHook::Install(void* p_AnyMorphemeAnimator) {
    std::lock_guard s_Lock(g_Mutex);

    if (g_Table)
        return true;

    if (!p_AnyMorphemeAnimator)
        return false;

    void** s_Table = *static_cast<void***>(p_AnyMorphemeAnimator);

    if (!s_Table || s_Table[c_UpdateSlot] == reinterpret_cast<void*>(&Update))
        return false;

    g_Original = reinterpret_cast<UpdateFn>(s_Table[c_UpdateSlot]);

    if (!Patch(&s_Table[c_UpdateSlot], reinterpret_cast<void*>(&Update)))
        return false;

    g_Table = s_Table;
    return true;
}

void PoseHook::Uninstall() {
    std::lock_guard s_Lock(g_Mutex);

    if (g_Table && g_Original)
        Patch(&g_Table[c_UpdateSlot], reinterpret_cast<void*>(g_Original));

    g_Table = nullptr;

    for (size_t i=0; i<c_Channels; ++i) {
        g_Sources[i] = nullptr;
        g_SourceFresh[i] = false;
        g_SourcePoses[i].clear();
        g_SourceFrames[i] = {};
    }

    for (auto& s_Extra : g_Extras)
        s_Extra = nullptr;

    for (auto& s_Watched : g_Watched)
        s_Watched = nullptr;

    g_Target = nullptr;
    g_HasTargetPose = false;
    for (size_t i = 0; i < 4; ++i) {
        g_PlayerAnimators[i] = nullptr;
        g_PlayerTargets[i] = {};
    }
}

bool PoseHook::Installed() {
    return g_Table != nullptr;
}

void PoseHook::SetSource(void* p_Animator, size_t p_Channel) {
    if (p_Channel >= c_Channels) return;
    std::lock_guard lock(g_Mutex);
    if (g_Sources[p_Channel].load() != p_Animator) {
        g_SourcePoses[p_Channel].clear();
        g_SourceFrames[p_Channel] = {};
        g_SourceFresh[p_Channel] = false;
        g_Sources[p_Channel] = p_Animator;
    }
}

void PoseHook::SetTarget(void* p_Animator) {
    std::lock_guard s_Lock(g_Mutex);
    if (p_Animator)
        for (const auto& s_Target : g_PlayerTargets)
            if (s_Target.animator == p_Animator)
                return;
    g_Target = p_Animator;
}

bool PoseHook::BindPlayerTarget(uint8_t p_Id, uint64_t p_Incarnation, void* p_Animator) {
    if (p_Id < 1 || p_Id > 4 || !p_Incarnation || !p_Animator)
        return false;
    std::lock_guard s_Lock(g_Mutex);
    if (p_Animator == g_Target.load())
        return false;
    for (const auto& s_Extra : g_Extras)
        if (s_Extra.load() == p_Animator)
            return false;
    size_t s_Count = 0;
    for (size_t i = 0; i < 4; ++i) {
        const auto& s_Target = g_PlayerTargets[i];
        if (i != p_Id - 1 && s_Target.animator == p_Animator)
            return false;
        if (i != p_Id - 1 && s_Target.animator)
            ++s_Count;
    }
    if (s_Count >= 3)
        return false; // At most three remote players on any one PC.
    auto& s_Target = g_PlayerTargets[p_Id - 1];
    if (s_Target.animator == p_Animator && s_Target.incarnation == p_Incarnation)
        return true; // Repeated roster snapshots must not reset a moving player.
    s_Target = {};
    s_Target.animator = p_Animator;
    s_Target.incarnation = p_Incarnation;
    g_PlayerAnimators[p_Id - 1] = p_Animator;
    return true;
}

bool PoseHook::SetPlayerTargetPose(uint8_t p_Id, uint64_t p_Incarnation,
                                  const std::vector<Bone>& p_Pose, bool p_AllPositions,
                                  const std::vector<int32_t>& p_Map,
                                  const std::vector<uint8_t>& p_Mask, uint32_t p_ExpectedBoneCount, const Frame& p_Frame) {
    if (p_Id < 1 || p_Id > 4 || !p_Incarnation ||
        p_Pose.size() > 1023 || p_Map.size() > 1023 || p_Mask.size() > 1023 || p_ExpectedBoneCount > 1023 ||
        (p_Frame.hasWorldRotation && (!std::isfinite(coop::Dot(p_Frame.worldRotation, p_Frame.worldRotation)) ||
            coop::Dot(p_Frame.worldRotation, p_Frame.worldRotation) < .001f)))
        return false;
    std::lock_guard s_Lock(g_Mutex);
    auto& s_Target = g_PlayerTargets[p_Id - 1];
    if (!s_Target.animator || s_Target.incarnation != p_Incarnation)
        return false;
    // Commit rig and pose together: a worker cannot see one player's new pose
    // with a prior rig map while their disguise is changing.
    s_Target.pose = p_Pose;
    s_Target.map = p_Map;
    s_Target.mask = p_Mask;
    s_Target.allPositions = p_AllPositions;
    s_Target.expectedBoneCount = p_ExpectedBoneCount;
    s_Target.frame = p_Frame;
    return true;
}

bool PoseHook::PlayerTargetNativePose(uint8_t p_Id, uint64_t p_Incarnation, std::vector<Bone>& p_Out) {
    p_Out.clear();
    if (p_Id < 1 || p_Id > 4 || !p_Incarnation)
        return false;
    std::lock_guard s_Lock(g_Mutex);
    const auto& s_Target = g_PlayerTargets[p_Id - 1];
    if (!s_Target.animator || s_Target.incarnation != p_Incarnation)
        return false;
    p_Out = s_Target.native;
    return !p_Out.empty();
}

bool PoseHook::PlayerFrameStats(uint8_t p_Id, uint64_t p_Incarnation, FrameStats& p_Out) {
    if (p_Id < 1 || p_Id > 4 || !p_Incarnation) return false;
    std::lock_guard lock(g_Mutex);
    const auto& target = g_PlayerTargets[p_Id-1];
    if (!target.animator || target.incarnation != p_Incarnation) return false;
    p_Out = target.stats;
    return true;
}

bool PoseHook::ClearPlayerTarget(uint8_t p_Id, uint64_t p_Incarnation) {
    if (p_Id < 1 || p_Id > 4 || !p_Incarnation)
        return false;
    std::lock_guard s_Lock(g_Mutex);
    auto& s_Target = g_PlayerTargets[p_Id - 1];
    if (!s_Target.animator || s_Target.incarnation != p_Incarnation)
        return false;
    g_PlayerAnimators[p_Id - 1] = nullptr;
    s_Target = {};
    return true;
}

void PoseHook::ClearPlayerTargets() {
    std::lock_guard s_Lock(g_Mutex);
    for (size_t i = 0; i < 4; ++i) {
        g_PlayerAnimators[i] = nullptr;
        g_PlayerTargets[i] = {};
    }
}

bool PoseHook::TakeSourcePose(std::vector<Bone>& p_Out, size_t p_Channel, Frame* p_Frame) {
    std::lock_guard s_Lock(g_Mutex);

    if (p_Channel >= c_Channels || !g_SourceFresh[p_Channel])
        return false;

    p_Out = g_SourcePoses[p_Channel];
    if (p_Frame) *p_Frame = g_SourceFrames[p_Channel];
    g_SourceFresh[p_Channel] = false;
    return true;
}

bool PoseHook::SetPoseFor(void* p_Animator, const std::vector<Bone>& p_Pose, bool exactBoneCount) {
    if (!p_Animator)
        return false;

    std::lock_guard s_Lock(g_Mutex);
    for (const auto& s_Target : g_PlayerTargets)
        if (s_Target.animator == p_Animator)
            return false;
    int s_Free = -1;

    for (size_t i = 0; i < c_Extras; ++i) {
        if (g_Extras[i] == p_Animator) {
            g_ExtraPoses[i] = p_Pose;
            g_ExtraExactCount[i] = exactBoneCount;
            return true;
        }

        if (!g_Extras[i] && s_Free < 0)
            s_Free = static_cast<int>(i);
    }

    if (s_Free < 0)
        return false;

    g_ExtraPoses[s_Free] = p_Pose;
    g_ExtraExactCount[s_Free] = exactBoneCount;
    g_ExtraNativeCount[s_Free] = 0;
    g_ExtraNative[s_Free].clear();
    g_Extras[s_Free] = p_Animator;
    return true;
}

void PoseHook::ClearPoseFor(void* p_Animator) {
    std::lock_guard s_Lock(g_Mutex);

    for (auto& s_Extra : g_Extras)
        if (s_Extra == p_Animator)
            s_Extra = nullptr;
}

void PoseHook::SetWatched(std::span<void* const> p_Animators) {
    std::lock_guard s_Lock(g_Mutex);

    // Each keeps its slot, and the pose recorded there, while it stays watched. The list comes
    // sorted by distance, and slots were given out in that order: every time two NPCs swapped
    // places, all after them moved slot, their poses were dropped as stale and not sent that round
    // (an NPC walking towards the partner kept losing its pose, and walked on its own legs there).
    for (size_t i = 0; i < c_Watched; ++i) {
        if (g_Watched[i] && std::find(p_Animators.begin(), p_Animators.end(), g_Watched[i].load()) == p_Animators.end()) {
            g_Watched[i] = nullptr;
            g_WatchedFresh[i] = false;
        }
    }

    for (void* s_Want : p_Animators) {
        if (!s_Want)
            continue;

        int s_Free = -1;
        bool s_Have = false;

        for (size_t i = 0; i < c_Watched && !s_Have; ++i) {
            if (g_Watched[i] == s_Want)
                s_Have = true;
            else if (!g_Watched[i] && s_Free < 0)
                s_Free = static_cast<int>(i);
        }

        if (!s_Have && s_Free >= 0) {
            g_WatchedFresh[s_Free] = false;
            g_Watched[s_Free] = s_Want;
        }
    }
}

bool PoseHook::WatchedPose(void* p_Animator, std::vector<Bone>& p_Out, bool consume) {
    std::lock_guard s_Lock(g_Mutex);

    for (size_t i = 0; i < c_Watched; ++i) {
        if (p_Animator && g_Watched[i] == p_Animator && g_WatchedFresh[i]) {
            p_Out = g_WatchedPoses[i];
            if (consume) g_WatchedFresh[i] = false;
            return !p_Out.empty();
        }
    }

    return false;
}

void PoseHook::ClearExtraPoses() {
    std::lock_guard s_Lock(g_Mutex);

    for (auto& s_Extra : g_Extras)
        s_Extra = nullptr;
}

void PoseHook::SetTargetPose(const std::vector<Bone>& p_Pose, bool p_AllPositions) {
    std::lock_guard s_Lock(g_Mutex);
    g_TargetPose = p_Pose;
    g_AllPositions = p_AllPositions;
    g_HasTargetPose = !p_Pose.empty();
}

bool PoseHook::TargetNativePose(std::vector<Bone>& p_Out) {
    std::lock_guard s_Lock(g_Mutex);
    p_Out = g_TargetNative;
    return !p_Out.empty();
}

void PoseHook::SetTargetMap(const std::vector<int32_t>& p_Map) {
    std::lock_guard s_Lock(g_Mutex);
    g_TargetMap = p_Map;
}

void PoseHook::SetTargetMask(const std::vector<uint8_t>& p_Mask) {
    std::lock_guard s_Lock(g_Mutex);
    g_TargetMask = p_Mask;
}

void PoseHook::ClearTargetPose() {
    std::lock_guard s_Lock(g_Mutex);
    g_HasTargetPose = false;
}

uint32_t PoseHook::SourceBoneCount() {
    return g_SourceCount;
}

uint32_t PoseHook::TargetBoneCount() {
    return g_TargetCount;
}

uint64_t PoseHook::Calls() {
    return g_Calls;
}

uint64_t PoseHook::TargetCalls() {
    return g_TargetCalls;
}

uint32_t PoseHook::NativePoseOf(void* p_Animator, std::vector<Bone>& p_Out) {
    std::lock_guard s_Lock(g_Mutex);

    for (size_t i = 0; i < c_Extras; ++i) {
        if (g_Extras[i] == p_Animator) {
            p_Out = g_ExtraNative[i];
            return g_ExtraNativeCount[i];
        }
    }

    return 0;
}
