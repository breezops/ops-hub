#include "coop/PartyAvatarPlayback.h"
#include <cmath>

namespace coop {
namespace {
template<class T> std::vector<uint8_t> Packet(MsgType type, const T& message) {
    auto payload = Encode(message);
    payload.insert(payload.begin(), static_cast<uint8_t>(type));
    return payload.size() <= partywire::MaxPayload - 4 ? payload : std::vector<uint8_t>{};
}
}
std::vector<uint8_t> PartyAvatarPacket(const MsgAvatarState& m) { return Packet(MsgType::AvatarState, m); }
std::vector<uint8_t> PartyAvatarPacket(const MsgPose& m) { return Packet(MsgType::Pose, m); }
std::vector<uint8_t> PartyAvatarPacket(const MsgOutfit& m) { return Packet(MsgType::Outfit, m); }
std::vector<uint8_t> PartyAvatarPacket(const MsgSkeleton& m) { return Packet(MsgType::Skeleton, m); }

void PartyAvatarPlayback::Reset() {
    m_World.HidePartyAvatars();
    m_Roster.Reset();
    for (auto& peer : m_Peers)
        peer = {};
}

bool PartyAvatarPlayback::UpdateRoster(const PartyRoomStatus& status) {
    if (status.state == PartyRoomStatus::State::Closed || status.state == PartyRoomStatus::State::Failed ||
        status.state == PartyRoomStatus::State::Leaving || status.state == PartyRoomStatus::State::Rejected) {
        Reset();
        return false;
    }
    auto candidate = m_Roster;
    if (!candidate.Update(status) || !m_World.ApplyPartyRoster(status))
        return false;
    m_Roster = std::move(candidate);
    for (uint8_t id = 1; id <= 4; ++id) {
        auto& peer = m_Peers[id - 1];
        const PartyMember* member = nullptr;
        for (const auto& current : status.members)
            if (m_Roster.Remote(current.id, current.incarnation, status.epoch) && current.id == id)
                member = &current;
        if (!member) {
            peer = {};
        } else if (peer.incarnation != member->incarnation || peer.epoch != status.epoch) {
            peer = {};
            peer.incarnation = member->incarnation;
            peer.epoch = status.epoch;
        }
    }
    return true;
}

bool PartyAvatarPlayback::Receive(const PartyGameEvent& event, double now, std::optional<Vec3>* acceptedPosition) {
    if (acceptedPosition) acceptedPosition->reset();
    if (!std::isfinite(now) || !m_Roster.Remote(event.source, event.incarnation, event.epoch) ||
        event.payload.empty() || event.payload.size() > partywire::MaxPayload - 4)
        return false;
    auto& peer = m_Peers[event.source - 1];
    if (peer.incarnation != event.incarnation || peer.epoch != event.epoch)
        return false;
    const auto type = static_cast<MsgType>(event.payload[0]);
    const std::vector<uint8_t> body(event.payload.begin() + 1, event.payload.end());
    if (type == MsgType::AvatarState && event.channel == PartyChannel::Avatar) {
        MsgAvatarState message;
        if (!Decode(body, message) || message.epoch != event.epoch ||
            (peer.hasTransform && static_cast<int32_t>(message.timeMs - peer.transformMs) <= 0))
            return false;
        if (now - peer.lastTransform > 2.)
            peer.transforms.Clear();
        peer.transforms.Push(now, message.timeMs, message.xf);
        peer.hasTransform = true;
        peer.transformMs = message.timeMs;
        peer.lastTransform = now;
        if (acceptedPosition) *acceptedPosition=message.xf.pos;
        return true;
    }
    if (type == MsgType::Pose && event.channel == PartyChannel::Pose) {
        MsgPose message;
        if (!Decode(body, message) || message.epoch != event.epoch || message.pose.rotations.size() < 2 ||
            (peer.hasPose && static_cast<int32_t>(message.timeMs - peer.poseMs) <= 0))
            return false;
        if (now - peer.lastPose > 2.)
            peer.poses.Clear();
        peer.poses.Push(now, message.timeMs, message.pose);
        peer.hasPose = true;
        peer.poseMs = message.timeMs;
        peer.lastPose = now;
        return true;
    }
    if (type == MsgType::Outfit && event.channel == PartyChannel::Avatar && event.reliable) {
        MsgOutfit message;
        return Decode(body, message) && m_World.SetPartyOutfit(event.source, event.incarnation, event.epoch, message.outfit);
    }
    if (type == MsgType::Skeleton && event.channel == PartyChannel::Avatar && event.reliable) {
        MsgSkeleton message;
        if (!Decode(body, message) || message.parents.size() < 2 || message.parents[0] != -1)
            return false;
        for (size_t i = 1; i < message.parents.size(); ++i)
            if (message.parents[i] < 0)
                return false;
        if (!m_World.SetPartySkeleton(event.source, event.incarnation, event.epoch, message.parents))
            return false;
        if (peer.parents != message.parents) {
            peer.poses.Clear();
            peer.lastPose = -100.;
            peer.parents = std::move(message.parents);
        }
        return true;
    }
    return false;
}

void PartyAvatarPlayback::Tick(double now) {
    if (!std::isfinite(now))
        return;
    for (uint8_t id = 1; id <= 4; ++id) {
        auto& peer = m_Peers[id - 1];
        if (!m_Roster.Remote(id, peer.incarnation, peer.epoch))
            continue;
        if (now - peer.lastTransform > 2.) {
            m_World.HidePartyAvatar(id, peer.incarnation, peer.epoch);
            continue;
        }
        const double delay = peer.transforms.Wanted(0.08, 0.25);
        peer.transforms.Retarget(delay);
        peer.poses.Retarget(delay);
        Transform transform;
        if (peer.transforms.Sample(now, transform))
            m_World.ShowPartyAvatar(id, peer.incarnation, peer.epoch, transform);
        Pose pose;
        if (now - peer.lastPose <= 2. && peer.poses.Sample(now, pose))
            m_World.ShowPartyPose(id, peer.incarnation, peer.epoch, pose);
    }
}
}
