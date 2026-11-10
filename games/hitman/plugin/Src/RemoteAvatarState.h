#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include "coop/Protocol.h"
#include "coop/AvatarAppearance.h"
#include "PartnerPuppet.h"
#include "PoseHook.h"
#include "Glacier/ZEntity.h"

class ZActor;
class IItem;

// Native presentation state for ONE remote player. Engine resources/helpers
// cannot be safely copied or moved between players. Shared scene/NPC authority,
// factories, local-player capture and world items remain owned by SdkWorld.
// The legacy Session uses one record until the room adapter is connected.
struct RemoteAvatarState {
    RemoteAvatarState() = default;
    RemoteAvatarState(const RemoteAvatarState&) = delete;
    RemoteAvatarState& operator=(const RemoteAvatarState&) = delete;
    RemoteAvatarState(RemoteAvatarState&&) = delete;
    RemoteAvatarState& operator=(RemoteAvatarState&&) = delete;

    struct ItemSlot {
        std::string wanted;
        std::string shown; // what it carries, or was asked for (ticket pending)
        TEntityRef<IItem> item;
        uint32_t ticket = 0;
        double askedAt = -1.0;
        double failedAt = -100.0;
        std::string failedFor;
        int attach = 0; // EAttachLocation
        bool weapon = false;
    };

    // Zero identifies the legacy two-player adapter; room IDs are 1..4.
    uint8_t playerId = 0;
    uint64_t incarnation = 0;
    bool present = false;
    void* poseAnimator = nullptr;
    ZEntityRef entity;
    ZActor* actor = nullptr;
    bool partnerSpawnFailed = false;
    bool partnerHidden = false;
    bool puppetFailed = false;
    double puppetRequestStart = -1.0;
    std::vector<PoseHook::Bone> remoteBones;
    PoseHook::Frame remotePoseFrame;
    double frameTraceAt = -100.0;
    ZActor* remoteVictim = nullptr;
    uint64_t remoteVictimId = 0;
    uint8_t remoteVictimFlags = 0;
    std::vector<PoseHook::Bone> remoteVictimBones;
    uint64_t victimPosesShown = 0;
    int remoteVictimRig = 0; // 0: not checked yet, 1: its skeleton fits the pose, -1: it doesn't
    int remoteVictimRigDiffer = 0;
    coop::Transform partnerSentXf; // where the partner said they are, before the stand-in follows
    bool hasPartnerSentXf = false;
    ZEntityRef partnerAi;
    ZEntityRef partnerUnseen;
    int partnerRig = 0; // 0: skeletons not compared yet, 1: compared (m_BoneMap)
    std::vector<int32_t> rigParents; // the stand-in's skeleton
    std::vector<int32_t> boneMap;    // its bone -> the partner's 47 bone (empty: the same)
    double partnerRigCheckAt = 0.0;
    double nextRigProbeAt = 0.0;
    bool rigHidden = false;
    coop::AvatarAppearance appearance;
    double rigHiddenSince = -1.0;
    double partnerRigReadyAt = 0.0;
    const void* rigBody = nullptr;     // the animated actor and animator the match was made for
    const void* rigAnimator = nullptr;
    const void* rigSkeleton = nullptr;  // its animator's rig (+0x50), which a new outfit replaces
    uint32_t rigBones = 0;              // and its bone count when matched
    int skeletonRematches = 0;          // since spawn or the last outfit: more than a few, stop watching
    std::vector<int32_t> remoteParents;
    double layoutMismatchSince = -1.0;
    bool remoteVictimDowned = false; // put down here already
    bool remoteVictimDragged = false; // and moved since (a drag), so posed again
    bool remoteVictimStilled = false;
    coop::Vec3 remoteVictimDownPos;
    double remotePoseTime = -100.0;
    double partnerShownTime = 0.0;
    coop::Vec3 partnerWanted {};
    double partnerWantedTime = 0.0;
    float partnerSpeed = 0.f;
    coop::Vec3 partnerVelocity {};
    PartnerPuppet puppet;
    bool hasPartnerOutfit = false;
    double partnerOutfitWaitStart = -1.0;
    const char* spawnWait = nullptr;
    double spawnWaitSince = -1.0;
    int spawnWaitTold = 0; // 1: logged, 2: told too
    std::string spawnFailure;
    std::string standInWears;
    bool standInShownLogged = false;
    bool toldCivilianStandIn = false;
    coop::OutfitDesc outfit;
    std::string name = "Partner";
    ItemSlot handSlot;
    ItemSlot backSlot { .attach = 3 }; // eALBack

    // Scene owns the actors/helpers. Forget references after engine scene clear;
    // never delete live actors or release puppet factories during a restart.
    void ForgetScene() {
        puppet.Forget();
        RemoteAvatarState defaults;
        playerId = defaults.playerId;
        incarnation = defaults.incarnation;
        present = defaults.present;
        poseAnimator = defaults.poseAnimator;
        entity = defaults.entity;
        actor = defaults.actor;
        partnerSpawnFailed = defaults.partnerSpawnFailed;
        partnerHidden = defaults.partnerHidden;
        puppetFailed = defaults.puppetFailed;
        puppetRequestStart = defaults.puppetRequestStart;
        remoteBones = defaults.remoteBones;
        remotePoseFrame = {};
        frameTraceAt = -100.0;
        remoteVictim = defaults.remoteVictim;
        remoteVictimId = defaults.remoteVictimId;
        remoteVictimFlags = defaults.remoteVictimFlags;
        remoteVictimBones = defaults.remoteVictimBones;
        victimPosesShown = defaults.victimPosesShown;
        remoteVictimRig = defaults.remoteVictimRig;
        remoteVictimRigDiffer = defaults.remoteVictimRigDiffer;
        partnerSentXf = defaults.partnerSentXf;
        hasPartnerSentXf = defaults.hasPartnerSentXf;
        partnerAi = defaults.partnerAi;
        partnerUnseen = defaults.partnerUnseen;
        partnerRig = defaults.partnerRig;
        rigParents = defaults.rigParents;
        boneMap = defaults.boneMap;
        partnerRigCheckAt = defaults.partnerRigCheckAt;
        nextRigProbeAt = defaults.nextRigProbeAt;
        rigHidden = defaults.rigHidden;
        appearance = {};
        rigHiddenSince = defaults.rigHiddenSince;
        partnerRigReadyAt = defaults.partnerRigReadyAt;
        rigBody = defaults.rigBody;
        rigAnimator = defaults.rigAnimator;
        rigSkeleton = defaults.rigSkeleton;
        rigBones = defaults.rigBones;
        skeletonRematches = defaults.skeletonRematches;
        remoteParents = defaults.remoteParents;
        layoutMismatchSince = defaults.layoutMismatchSince;
        remoteVictimDowned = defaults.remoteVictimDowned;
        remoteVictimDragged = defaults.remoteVictimDragged;
        remoteVictimStilled = defaults.remoteVictimStilled;
        remoteVictimDownPos = defaults.remoteVictimDownPos;
        remotePoseTime = defaults.remotePoseTime;
        partnerShownTime = defaults.partnerShownTime;
        partnerWanted = defaults.partnerWanted;
        partnerWantedTime = defaults.partnerWantedTime;
        partnerSpeed = defaults.partnerSpeed;
        partnerVelocity = defaults.partnerVelocity;
        hasPartnerOutfit = defaults.hasPartnerOutfit;
        partnerOutfitWaitStart = defaults.partnerOutfitWaitStart;
        spawnWait = defaults.spawnWait;
        spawnWaitSince = defaults.spawnWaitSince;
        spawnWaitTold = defaults.spawnWaitTold;
        spawnFailure = defaults.spawnFailure;
        standInWears = defaults.standInWears;
        standInShownLogged = defaults.standInShownLogged;
        toldCivilianStandIn = defaults.toldCivilianStandIn;
        outfit = defaults.outfit;
        name = defaults.name;
        handSlot = defaults.handSlot;
        backSlot = defaults.backSlot;
    }
};
