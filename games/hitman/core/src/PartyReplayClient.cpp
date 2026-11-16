#include "coop/PartyReplayClient.h"
#include <cmath>

namespace coop {
PartyReplayClient::PartyReplayClient(std::unique_ptr<PartyRoom> room,Vec3 offset,uint8_t source)
    :m_Room(std::move(room)),m_Offset(offset),m_Source(source) {}
bool PartyReplayClient::Forward(const PartyGameEvent& event) {
    const auto& status=m_Room->Status();
    if(status.phase!=LobbyRoster::Phase::Playing || event.epoch!=status.epoch || event.source!=m_Source ||
        event.source==status.self || event.payload.empty() || event.payload.size()>partywire::MaxPayload-4) return false;
    bool member=false;
    for(const auto& current:status.members)
        if(current.id==event.source && current.incarnation==event.incarnation) member=true;
    if(!member) return false;
    const auto kind=MsgType(event.payload[0]);
    const std::vector<uint8_t> body(event.payload.begin()+1,event.payload.end());
    std::vector<uint8_t> packet;
    if(kind==MsgType::AvatarState && event.channel==PartyChannel::Avatar) {
        MsgAvatarState message;
        if(!Decode(body,message) || message.epoch!=status.epoch) return false;
        message.xf.pos=message.xf.pos+m_Offset;
        if(!std::isfinite(message.xf.pos.x) || !std::isfinite(message.xf.pos.y) || !std::isfinite(message.xf.pos.z) ||
            std::fabs(message.xf.pos.x)>90000.f || std::fabs(message.xf.pos.y)>90000.f || std::fabs(message.xf.pos.z)>90000.f) return false;
        packet=PartyAvatarPacket(message);
    } else if(kind==MsgType::Pose && event.channel==PartyChannel::Pose) {
        MsgPose message;
        if(!Decode(body,message) || message.epoch!=status.epoch || message.pose.rotations.size()<2) return false;
        packet=PartyAvatarPacket(message);
    } else if(kind==MsgType::Outfit && event.channel==PartyChannel::Avatar && event.reliable) {
        MsgOutfit message; if(!Decode(body,message)) return false;
        packet=PartyAvatarPacket(message);
    } else if(kind==MsgType::Skeleton && event.channel==PartyChannel::Avatar && event.reliable) {
        MsgSkeleton message; if(!Decode(body,message) || message.parents.size()<2) return false;
        packet=PartyAvatarPacket(message);
    } else return false;
    if(packet.empty() || !m_Room->SendGame(event.channel,packet,event.reliable)) return false;
    ++m_Forwarded; return true;
}
void PartyReplayClient::Tick(double now) {
    if(!std::isfinite(now) || now<0) return;
    m_Room->Tick(now);
    const auto status=m_Room->Status();
    // Synthetic readiness is visibly labeled [LAB BOT] in the roster. Wait for
    // the real host's readiness/loading acknowledgement; never load a mission.
    const PartyMember* host=nullptr; const PartyMember* self=nullptr;
    for(const auto& member:status.members) {
        if(member.id==1) host=&member;
        if(member.id==status.self) self=&member;
    }
    if(status.state==PartyRoomStatus::State::Connected && host && self && status.epoch && status.mission==kPartyFreeformContract) {
        if(status.phase==LobbyRoster::Phase::Lobby && now>=m_NextReady && self->ready!=host->ready) {
            m_NextReady=now+0.5;
            m_Room->SetReady(host->available,host->ready);
        }
        if(status.phase==LobbyRoster::Phase::Loading && host->loaded && !self->loaded && now>=m_NextLoaded) {
            m_NextLoaded=now+0.5;
            m_Room->ReportLoaded(); // Synthetic playback readiness, NOT a native game load.
        }
    }
    PartyGameEvent event;
    while(m_Room->PopGame(event)) Forward(event);
}
void PartyReplayClient::Leave(double now) { m_Room->Leave(now); }
}
