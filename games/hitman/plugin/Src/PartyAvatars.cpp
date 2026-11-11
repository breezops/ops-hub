#include "SdkWorld.h"
#include "CrashGuards.h"
#include "coop/LauncherPolicy.h"
#include "IModSDK.h"
#include <Glacier/ZActor.h>
#include <Glacier/ZPhysics.h>
#include <Glacier/ZSpatialEntity.h>
#include <cmath>
#include <cstdio>

namespace {
bool Finite(coop::Vec3 v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z) &&
        std::fabs(v.x) < 100000.f && std::fabs(v.y) < 100000.f && std::fabs(v.z) < 100000.f;
}
bool Finite(coop::Quat q) {
    return std::isfinite(q.x) && std::isfinite(q.y) && std::isfinite(q.z) && std::isfinite(q.w) &&
        coop::Dot(q, q) > 0.001f && coop::Dot(q, q) < 4.f;
}
}

std::string SdkWorld::PartyPreviewError() const {
#ifdef COOP_NPC_CATALOG
    if (auto fault=NpcCatalogError(); !fault.empty()) return fault;
#endif
    if(m_NpcEventOverflow)
        return "Native NPC event queue overflowed. Reload Freeform Training before starting again.";
    if (!CrashGuards::InteractionGuardActive())
        return "Preview protection could not be installed for this game build. Close HITMAN and send the log.";
    if (m_PartyAvatarDamaged)
        return "A remote stand-in was knocked down on this PC. Restart Freeform Training here, then stop the preview and ready again.";
    return {};
}

void SdkWorld::ApplyAvatarPose(RemoteAvatarState& avatar) {
    if (!avatar.playerId) {
        PoseHook::SetTargetMap(avatar.boneMap);
        if (avatar.partnerRig == 1)
            PoseHook::SetTargetPose(avatar.remoteBones, false);
        else
            PoseHook::ClearTargetPose();
    } else {
        PoseHook::SetPlayerTargetPose(avatar.playerId, avatar.incarnation,
            avatar.partnerRig == 1 ? avatar.remoteBones : std::vector<PoseHook::Bone>{}, false, avatar.boneMap, {},
            avatar.partnerRig == 1 ? (avatar.rigParents.empty() ? AvatarBoneCount(avatar) : static_cast<uint32_t>(avatar.rigParents.size())) : 0, avatar.remotePoseFrame);
    }
}

void SdkWorld::ClearAvatarPose(RemoteAvatarState& avatar) {
    if (!avatar.playerId)
        PoseHook::ClearTargetPose();
    else
        PoseHook::SetPlayerTargetPose(avatar.playerId, avatar.incarnation, {}, false, avatar.boneMap);
}

void SdkWorld::DetachAvatarPose(RemoteAvatarState& avatar) {
    if (!avatar.playerId)
        PoseHook::SetTarget(nullptr);
    else
        PoseHook::ClearPlayerTarget(avatar.playerId, avatar.incarnation);
    avatar.poseAnimator = nullptr;
}

bool SdkWorld::AvatarNativePose(const RemoteAvatarState& avatar, std::vector<PoseHook::Bone>& out) const {
    return avatar.playerId ? PoseHook::PlayerTargetNativePose(avatar.playerId, avatar.incarnation, out) : PoseHook::TargetNativePose(out);
}

uint32_t SdkWorld::AvatarBoneCount(const RemoteAvatarState& avatar) const {
    if (!avatar.playerId)
        return PoseHook::TargetBoneCount();
    std::vector<PoseHook::Bone> native;
    return AvatarNativePose(avatar, native) ? static_cast<uint32_t>(native.size()) : 0;
}

bool SdkWorld::ApplyPartyRoster(const coop::PartyRoomStatus& status) {
    if (!PartyPreviewError().empty()) {
        HidePartyAvatars();
        return false;
    }
    if (status.state == coop::PartyRoomStatus::State::Closed ||
        status.state == coop::PartyRoomStatus::State::Leaving ||
        status.state == coop::PartyRoomStatus::State::Failed ||
        status.state == coop::PartyRoomStatus::State::Rejected) {
        HidePartyAvatars();
        return false;
    }
    // Mixing the old Session and party actors would run two authority models.
    if (m_LegacyAvatar.entity)
        return false;
    if (status.phase == coop::LobbyRoster::Phase::Playing &&
        (!m_InScene || !coop::IsFreeformTrainingScene(m_Scene.scene)))
        return false;
    const bool newEpoch = m_PartyPresentation.Status().epoch != status.epoch;
    if (!m_PartyPresentation.Update(status))
        return false;
    for (uint8_t id = 1; id <= 4; ++id) {
        auto& avatar = m_PlayerAvatars[id - 1];
        const coop::PartyMember* member = nullptr;
        for (const auto& candidate : status.members)
            if (candidate.id == id && id != status.self)
                member = &candidate;
        const bool active = member && status.phase == coop::LobbyRoster::Phase::Playing;
        if (!active) {
            HideRemoteAvatar(avatar);
            avatar.present = false;
            continue;
        }
        if (newEpoch || avatar.incarnation != member->incarnation || avatar.playerId != id) {
            HideRemoteAvatar(avatar);
            avatar.remoteBones.clear();
            avatar.remotePoseFrame = {};
            avatar.remoteParents.clear();
            avatar.remotePoseTime = -100.;
            avatar.partnerWantedTime = 0.;
            avatar.partnerSpeed = 0.;
            avatar.partnerVelocity = {};
            avatar.hasPartnerOutfit = false;
            avatar.handSlot.wanted.clear();
            avatar.handSlot.failedFor.clear();
            avatar.playerId = id;
            avatar.incarnation = member->incarnation;
            if (avatar.actor) {
                CrashGuards::SetPlayerActor(id, avatar.incarnation, avatar.actor);
                CrashGuards::SetPlayerKnowledge(id, avatar.incarnation, avatar.actor->m_pKnowledge, false);
            }
            ResetPartnerRig(avatar, 0.05);
        }
        avatar.name = member->name;
        avatar.present = true;
    }
    return true;
}

RemoteAvatarState* SdkWorld::PartyAvatar(uint8_t id, uint64_t incarnation, uint32_t epoch) {
    if (!m_InScene || !coop::IsFreeformTrainingScene(m_Scene.scene) ||
        !m_PartyPresentation.Remote(id, incarnation, epoch))
        return nullptr;
    auto& avatar = m_PlayerAvatars[id - 1];
    return avatar.present && avatar.playerId == id && avatar.incarnation == incarnation ? &avatar : nullptr;
}

bool SdkWorld::ShowPartyAvatar(uint8_t id, uint64_t incarnation, uint32_t epoch, const coop::Transform& xf) {
    auto* avatar = PartyAvatar(id, incarnation, epoch);
    if (!avatar || !PartyPreviewError().empty() || !Finite(xf.pos) || !Finite(xf.rot))
        return false;
    ShowRemoteAvatar(*avatar, xf);
    return static_cast<bool>(avatar->entity) && !avatar->partnerSpawnFailed;
}

bool SdkWorld::ShowPartyPose(uint8_t id, uint64_t incarnation, uint32_t epoch, const coop::Pose& pose) {
    auto* avatar = PartyAvatar(id, incarnation, epoch);
    if (!avatar || pose.rotations.size() < 2 || pose.rotations.size() > coop::kMaxPoseBones ||
        !Finite(pose.rootPos) || !Finite(pose.pelvisPos) ||
        (pose.hasWorldRotation && !Finite(pose.worldRotation)))
        return false;
    for (const auto& rotation : pose.rotations)
        if (!Finite(rotation))
            return false;
    ShowRemotePose(*avatar, pose);
    return true;
}

bool SdkWorld::SetPartyOutfit(uint8_t id, uint64_t incarnation, uint32_t epoch, const coop::OutfitDesc& outfit) {
    auto* avatar = PartyAvatar(id, incarnation, epoch);
    if (!avatar || outfit.repositoryId.empty() || outfit.repositoryId.size() > 64 ||
        outfit.charset < 0 || outfit.charset > 255 || outfit.variation < 0 || outfit.variation > 255)
        return false;
    if (!avatar->hasPartnerOutfit || avatar->outfit != outfit)
        SetRemoteOutfit(*avatar, outfit);
    return true;
}

bool SdkWorld::SetPartySkeleton(uint8_t id, uint64_t incarnation, uint32_t epoch, const std::vector<int16_t>& parents) {
    auto* avatar = PartyAvatar(id, incarnation, epoch);
    if (!avatar || parents.size() < 2 || parents.size() > coop::kMaxPoseBones || parents[0] != -1)
        return false;
    for (size_t i = 1; i < parents.size(); ++i)
        if (parents[i] < 0 || static_cast<size_t>(parents[i]) >= i)
            return false;
    if (std::vector<int32_t>(parents.begin(), parents.end()) != avatar->remoteParents)
        SetRemoteSkeleton(*avatar, parents);
    return true;
}

void SdkWorld::SetPartyHeldItem(uint8_t id, uint64_t incarnation, uint32_t epoch, const std::string& item) {
    if (auto* avatar=PartyAvatar(id,incarnation,epoch)) avatar->handSlot.wanted=item;
}

void SdkWorld::HidePartyAvatars() {
    for (auto& avatar : m_PlayerAvatars) {
        HideRemoteAvatar(avatar);
        avatar.present = false;
    }
}

bool SdkWorld::HidePartyAvatar(uint8_t id, uint64_t incarnation, uint32_t epoch) {
    auto* avatar = PartyAvatar(id, incarnation, epoch);
    if (!avatar)
        return false;
    HideRemoteAvatar(*avatar);
    return true;
}

void SdkWorld::UpdatePlayerAvatarTargets() {
    for (auto& avatar : m_PlayerAvatars) {
        if (!avatar.playerId)
            continue;
        if (m_InScene && avatar.actor) {
            // Reassert native picker protection even while hidden/stopped.
            avatar.actor->m_bUnk38 = true;
            avatar.actor->m_bUnk39 = true;
            if (!m_PartyAvatarDamaged && m_Now - avatar.partnerShownTime > 2.0 &&
                (avatar.actor->IsDead() || avatar.actor->IsPacified())) {
                m_PartyAvatarDamaged = true;
                Log(coop::LogLevel::Info, "party", "Native stand-in damaged: player=" +
                    std::to_string(avatar.playerId) + " incarnation=" + std::to_string(avatar.incarnation) +
                    " dead=" + std::to_string(avatar.actor->IsDead()) +
                    " pacified=" + std::to_string(avatar.actor->IsPacified()) + "; mission reload required.");
                HidePartyAvatars();
            }
        }
        if (avatar.actor)
            CrashGuards::SetPlayerKnowledge(avatar.playerId, avatar.incarnation,
                avatar.actor->m_pKnowledge, m_Now - avatar.partnerShownTime < 2.0);
        if (!m_InScene || !avatar.present || avatar.partnerHidden || !avatar.actor || !avatar.actor->m_pAnimatedActor) {
            DetachAvatarPose(avatar);
            continue;
        }
        if (avatar.actor->m_pInventoryHandler && Globals::WorldInventory)
            UpdateStandInSlot(avatar,avatar.handSlot,"hand");
        void* animator = avatar.actor->m_pMorphemeEntity.m_entityRef.QueryInterface<IBoneAnimator>();
        if (!animator) {
            DetachAvatarPose(avatar);
            continue;
        }
        if (avatar.poseAnimator != animator) {
            DetachAvatarPose(avatar);
            if (!PoseHook::BindPlayerTarget(avatar.playerId, avatar.incarnation, animator))
                continue;
            avatar.poseAnimator = animator;
            ResetPartnerRig(avatar, avatar.appearance.KeepVisible() ? 0.0 : 0.3);
        }
        const void* body = avatar.actor->m_pAnimatedActor;
        const void* skeleton = *reinterpret_cast<void* const*>(static_cast<const uint8_t*>(animator) + 0x50);
        const uint32_t bones = AvatarBoneCount(avatar);
        // Bone-count changes always need a fresh map: the worker correctly
        // rejects the old map, so suppressing this after several changes would
        // leave the player in native pose indefinitely. Only pointer churn is
        // capped; ordinary pose traffic cannot select a new native bone count.
        const bool changed = avatar.partnerRig == 1 &&
            ((avatar.rigBones && bones && avatar.rigBones != bones) ||
             (avatar.skeletonRematches < 4 && ((avatar.rigBody && avatar.rigBody != body) ||
              (avatar.rigSkeleton && avatar.rigSkeleton != skeleton))));
        if (changed) {
            ++avatar.skeletonRematches;
            ResetPartnerRig(avatar, avatar.appearance.KeepVisible() ? 0.0 : 0.3);
        }
        avatar.rigBody = body;
        avatar.rigAnimator = animator;
        CheckPartnerSkeleton(avatar);
        if (avatar.partnerRig == 1) {
            avatar.rigSkeleton = skeleton;
            avatar.rigBones = bones;
        } else {
            avatar.rigSkeleton = nullptr;
            avatar.rigBones = 0;
        }
        if (m_Now - avatar.remotePoseTime < 2. &&
            (avatar.remoteParents.empty() || avatar.remoteParents.size() == avatar.remoteBones.size()))
            ApplyAvatarPose(avatar);
        else
            ClearAvatarPose(avatar);
        if (m_Trace && m_Now - avatar.frameTraceAt >= 10.) {
            avatar.frameTraceAt = m_Now;
            PoseHook::FrameStats stats;
            if (PoseHook::PlayerFrameStats(avatar.playerId, avatar.incarnation, stats)) {
                char line[200];
                std::snprintf(line, sizeof(line), "party-frame: player %u source=%s rebased=%llu unavailable=%llu correction=%.1fdeg",
                    avatar.playerId, avatar.remotePoseFrame.hasWorldRotation ? "world" : "legacy",
                    static_cast<unsigned long long>(stats.rebased), static_cast<unsigned long long>(stats.unavailable),
                    stats.lastCorrectionRadians * 57.29578f);
                m_Trace(line);
            }
        }
    }
}

void SdkWorld::ForgetPlayerAvatarScenes() {
    PoseHook::ClearPlayerTargets();
    for (auto& avatar : m_PlayerAvatars) {
        if (avatar.partnerUnseen && avatar.partnerUnseen.GetEntity()) {
            avatar.partnerUnseen.GetEntity()->Deactivate(0);
            avatar.partnerUnseen.SetProperty("m_rActor", TEntityRef<ZActor>());
        }
        avatar.ForgetScene();
    }
    CrashGuards::ClearPlayers();
    m_PartyAvatarDamaged = false;
    m_PartyPresentation.Reset();
}
