#pragma once
#include "PartyPresentationRoster.h"
#include "Interpolation.h"
#include <optional>

namespace coop {
class IPartyAvatarWorld {
public:
    virtual ~IPartyAvatarWorld() = default;
    // A latched native presentation fault makes this scene unavailable until
    // the engine rebuilds it. Stop/Start cannot repair a downed native actor.
    virtual std::string PartyPreviewError() const { return {}; }
    virtual bool ApplyPartyRoster(const PartyRoomStatus&) = 0;
    virtual bool ShowPartyAvatar(uint8_t, uint64_t, uint32_t, const Transform&) = 0;
    virtual bool ShowPartyPose(uint8_t, uint64_t, uint32_t, const Pose&) = 0;
    virtual bool SetPartyOutfit(uint8_t, uint64_t, uint32_t, const OutfitDesc&) = 0;
    virtual bool SetPartySkeleton(uint8_t, uint64_t, uint32_t, const std::vector<int16_t>&) = 0;
    virtual PartyEquipment LocalPartyEquipment() const { return {}; }
    virtual void SetPartyEquipment(uint8_t, uint64_t, uint32_t, const PartyEquipment&) {}
    virtual void SetPartyPause(bool, bool) {}
    virtual bool HidePartyAvatar(uint8_t, uint64_t, uint32_t) = 0;
    virtual void HidePartyAvatars() = 0;
};

std::vector<uint8_t> PartyAvatarPacket(const MsgAvatarState& message);
std::vector<uint8_t> PartyAvatarPacket(const MsgPose& message);
std::vector<uint8_t> PartyAvatarPacket(const MsgOutfit& message);
std::vector<uint8_t> PartyAvatarPacket(const MsgSkeleton& message);

// Typed, scoped, independently buffered native presentation. It does not own a
// socket, change mission state, or grant NPC/interaction authority.
class PartyAvatarPlayback {
public:
    explicit PartyAvatarPlayback(IPartyAvatarWorld& world) : m_World(world) {}
    bool UpdateRoster(const PartyRoomStatus& status);
    bool Receive(const PartyGameEvent& event, double now, std::optional<Vec3>* acceptedPosition = nullptr);
    void Tick(double now);
    void Reset();
private:
    struct Peer {
        uint64_t incarnation = 0;
        uint32_t epoch = 0;
        TimedBuffer<Transform> transforms {0.1};
        TimedBuffer<Pose> poses {0.1};
        std::vector<int16_t> parents;
        double lastTransform = -100., lastPose = -100.;
        uint32_t transformMs = 0, poseMs = 0;
        bool hasTransform = false, hasPose = false;
    };
    IPartyAvatarWorld& m_World;
    PartyPresentationRoster m_Roster;
    std::array<Peer, 4> m_Peers;
};
}
