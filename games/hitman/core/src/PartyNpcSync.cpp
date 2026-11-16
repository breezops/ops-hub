#include "coop/PartyNpcSync.h"
#include <cmath>
#include <compare>

namespace coop {
namespace {
bool HostKind(PartyNpcKind kind) {
    return kind==PartyNpcKind::Commit || kind==PartyNpcKind::Canonical || kind==PartyNpcKind::Quiesce || kind==PartyNpcKind::Prepare || kind==PartyNpcKind::Freeze;
}
bool Direct(PartyNpcKind kind) { return kind==PartyNpcKind::Quiesce || kind==PartyNpcKind::Prepare; }
constexpr uint8_t kNpcPosePacket=static_cast<uint8_t>(MsgType::NpcPose);
constexpr double kPoseTimeout=0.75;
constexpr double kPoseInterval=1./24.;
constexpr float kNearbyPoseRadius=25.f;
constexpr float kMovingPoseRadius=40.f;
constexpr float kPoseRetentionRadius=5.f;
constexpr float kPoseRetentionBias=3.f;
constexpr double kRecentMotionSeconds=0.75;
constexpr double kHandoffPoseSeconds=1.5;
constexpr size_t kNearbyPoseLimit=12;
constexpr float kGuardApproachRadius=14.f;
constexpr float kGuardReleaseRadius=18.f;
constexpr float kGuardDistanceMargin=3.f;
struct NpcPresentationPacket {
    uint64_t generation=0;
    Transform transform;
    MsgNpcPose pose;
    void Write(ByteWriter& writer) const { writer.U64(generation); WriteTransform(writer,transform); pose.Write(writer); }
    bool Read(ByteReader& reader) { return reader.U64(generation) && ReadTransform(reader,transform) && pose.Read(reader); }
};
bool ValidFullPose(const Pose& pose) {
    const auto position=[](Vec3 p) {
        return std::fabs(p.x)<=32 && std::fabs(p.y)<=32 && std::fabs(p.z)<=32;
    };
    return pose.rotations.size()>=2 && pose.rotations.size()<=kMaxPoseBones && position(pose.rootPos) &&
        position(pose.pelvisPos) && std::all_of(pose.rotations.begin(),pose.rotations.end(),[](Quat q) {
            const auto length=Dot(q,q);
            return length>=0.25f && length<=4.f;
        });
}
}
PartyNpcSync::PartyNpcSync(PartyRoom& room,IPartyNpcWorld& world,PartyNpcOptions options)
    :m_Room(room),m_World(world),m_Options(options) {}
PartyNpcSync::~PartyNpcSync() { Stop(); }
bool PartyNpcSync::Current() const {
    PartyNpcScope scope;
    return PartyNpcScope::FromRoom(m_Room.Status(),scope) && scope==m_Scope;
}
bool PartyNpcSync::Start() {
    PartyNpcScope next;
    if(m_Active || m_Failed || !m_Options.ValidCatalog() || !PartyNpcScope::FromRoom(m_Room.Status(),next) || next==m_Scope) return false;
    m_Scope=next;
    m_Self={m_Room.Status().self,m_Scope.incarnations[m_Room.Status().self-1]};
    m_Replica=std::make_unique<PartyNpcReplica>(m_Scope,m_Self,m_World,m_Options.capacity,m_Options.confirmInitialGuestBaselines);
    m_Host.reset();
    if(m_Room.Status().host) m_Host=std::make_unique<PartyNpcAuthority>(m_Scope,m_Options);
    if(m_Replica->Stopped() || (m_Host && m_Host->Failed())) { Fail(); return false; }
    m_Known.clear(); m_Locations.clear(); m_NextReport=m_NextPolicy=m_NextRefresh=m_NextPose=0; m_Active=true;
    m_AutomaticGuards={};
    return true;
}
void PartyNpcSync::Fail() {
    m_Failed=true;
    Stop();
    m_Room.ReportSceneLost();
}
void PartyNpcSync::Stop() {
    const bool notify=m_Active && Current();
    m_Active=false;
    if(m_Options.streamPoses) m_World.SetNpcPoseSources({});
    for(const auto& [npc,stream]:m_Poses) if(stream.applied) m_World.ClearNpcPresentationPose(npc);
    m_Poses.clear();
    m_PoseSources.clear();
    if(m_Host) m_Host->Stop();
    if(m_Replica) m_Replica->Stop();
    m_Known.clear(); m_Locations.clear();
    if(notify) m_Room.ReportSceneLost();
}
bool PartyNpcSync::Register(uint64_t npc,const PartyNpcState& state,double now) {
    if(!m_Active || !Current() || !m_Host || !m_Host->Register(npc,state,now)) {
        if(m_Host && m_Host->Failed()) Fail();
        return false;
    }
    m_Known.insert(npc); Flush(now); return !m_Failed;
}
bool PartyNpcSync::ObservePlayer(PartyIdentity peer,Vec3 position,double now) {
    PartyNpcState check; check.xf.pos=position;
    if(!m_Active || !Current() || !m_Host || !m_Scope.Contains(peer) || !std::isfinite(now) || now<0 || !ValidPartyNpcState(check)) return false;
    m_Locations[peer.id]={peer,position,now}; return true;
}
bool PartyNpcSync::Propose(uint64_t npc,PartyIdentity peer,double now) {
    if(!m_Active || !Current() || !m_Host || !m_Host->Propose(npc,peer,now)) return false;
    Flush(now); return !m_Failed;
}
bool PartyNpcSync::ApplyHost(const PartyNpcMessage& message) {
    if(!HostKind(message.kind) || !m_Scope.Contains(message.owner)) return false;
    if(!m_Options.AcceptsNpc(message.npc)) { Fail(); return false; }
    if(Direct(message.kind) && message.owner.id!=m_Self.id) return true;
    if(message.kind==PartyNpcKind::Commit && !m_Known.contains(message.npc)) {
        if(m_Known.size()>=m_Options.capacity) { Fail(); return false; }
        m_Known.insert(message.npc);
    }
    return m_Replica->Receive({1,m_Scope.incarnations[0]},PartyChannel::HostWorld,message);
}
bool PartyNpcSync::Receive(const PartyGameEvent& event,double now) {
    if(!m_Active || !Current()) { if(m_Active) Stop(); return false; }
    if(event.epoch!=m_Scope.epoch || !m_Scope.Contains({event.source,event.incarnation}) || !std::isfinite(now) || now<0) return false;
    if(!event.payload.empty() && event.payload.front()==kNpcPosePacket) return ReceivePose(event,now);
    PartyNpcMessage message;
    if(!DecodePartyNpc(event.payload,message) || message.session!=m_Scope.session || message.epoch!=m_Scope.epoch) return false;
    if(event.channel==PartyChannel::HostWorld && event.source==1 && HostKind(message.kind)) {
        if(message.kind!=PartyNpcKind::Canonical && !event.reliable) return false;
        return ApplyHost(message);
    }
    if(event.channel==PartyChannel::Interaction && m_Host && !HostKind(message.kind)) {
        if(message.kind!=PartyNpcKind::Report && !event.reliable) return false;
        if(!m_Host->Receive({event.source,event.incarnation},message,now)) return false;
        if(message.kind==PartyNpcKind::Prepared && m_Host->InitialGuestsPrepared(message.npc))
            m_Replica->ConfirmInitialGuests(message.npc);
        return true;
    }
    return false;
}
bool PartyNpcSync::ReceivePose(const PartyGameEvent& event,double now) {
    if(!m_Options.streamPoses || event.reliable) return false;
    if(event.source==1 ? event.channel!=PartyChannel::HostWorld :
        event.channel!=PartyChannel::Interaction || m_Options.policy!=PartyNpcPolicy::Explicit) return false;
    NpcPresentationPacket frame;
    const auto& packet=frame.pose;
    const std::vector<uint8_t> body(event.payload.begin()+1,event.payload.end());
    if(!Decode(body,frame) || packet.epoch!=m_Scope.epoch || !packet.full ||
        !m_Options.AcceptsNpc(packet.id) || !m_Replica->PresentingNpc(packet.id,event.source,frame.generation)) return false;
    if(m_Host) {
        const auto* record=m_Host->Find(packet.id);
        if(!record || !record->confirmed || record->phase!=PartyNpcAuthority::Phase::Active ||
            record->generation!=frame.generation || record->owner.id!=event.source ||
            record->owner.incarnation!=event.incarnation) return false;
    }
    Pose pose;
    if(!packet.ApplyTo(pose) || !ValidFullPose(pose)) return false;
    auto [it,inserted]=m_Poses.try_emplace(packet.id);
    auto& stream=it->second;
    if(!inserted && stream.generation==frame.generation) {
        if(static_cast<int32_t>(packet.timeMs-stream.timeMs)<=0) return false;
        if(now-stream.received>=kPoseTimeout) stream.buffer.Clear();
    } else {
        if(stream.applied) m_World.ClearNpcPresentationPose(packet.id);
        stream.buffer.Clear();
        stream.applied=false;
        stream.generation=frame.generation;
        stream.owner=event.source;
    }
    stream.buffer.Push(now,packet.timeMs,{frame.transform,std::move(pose)});
    stream.timeMs=packet.timeMs;
    stream.received=now;
    if(m_Host) m_Room.ApproveInteraction(event);
    return true;
}
void PartyNpcSync::SendPoses(double now) {
    if(now<m_NextPose) return;
    if(m_NextPose==0 || now-m_NextPose>=kPoseInterval) m_NextPose=now;
    m_NextPose+=kPoseInterval;
    struct Candidate {
        bool outsideHandoff, stationary;
        float distance;
        uint64_t npc, generation;
        auto operator<=>(const Candidate&) const = default;
    };
    std::vector<Candidate> candidates;
    for(const auto npc:m_Known) {
        const auto generation=m_Replica->RunningNpcGeneration(npc);
        if(!generation) continue;
        const auto* record=m_Host ? m_Host->Find(npc) : nullptr;
        if(m_Host && (!record || !record->confirmed || record->phase!=PartyNpcAuthority::Phase::Active ||
            record->generation!=generation || record->state.life!=PartyNpcLife::Alive || record->state.stripped)) continue;
        float nearest=0.f;
        bool moving=false;
        bool recentHandoff=false;
        if (record && m_Options.nearbyNpcPoses) {
            moving=record->lastMovement>=0. && now-record->lastMovement<=kRecentMotionSeconds;
            recentHandoff=record->generation>1 && now-record->changed<=kHandoffPoseSeconds;
            const bool wasStreaming=std::find(m_PoseSources.begin(),m_PoseSources.end(),npc)!=m_PoseSources.end();
            const float radius=(moving || recentHandoff ? kMovingPoseRadius : kNearbyPoseRadius) +
                (wasStreaming ? kPoseRetentionRadius : 0.f);
            nearest=radius;
            for (const auto& [seat,player]:m_Locations) {
                if (seat!=1 && now-player.observed<=m_Options.locationMaxAge)
                    nearest=std::min(nearest,Distance(player.position,record->state.xf.pos));
            }
            if (nearest>=radius) continue;
            if (wasStreaming) nearest=std::max(0.f,nearest-kPoseRetentionBias);
        }
        candidates.push_back({!recentHandoff,!moving,nearest,npc,generation});
    }
    if (m_Options.nearbyNpcPoses || !m_Host) {
        std::sort(candidates.begin(),candidates.end());
        if (candidates.size()>kNearbyPoseLimit) candidates.resize(kNearbyPoseLimit);
    }
    std::vector<uint64_t> ids;
    ids.reserve(candidates.size());
    for (const auto& candidate:candidates) ids.push_back(candidate.npc);
    m_World.SetNpcPoseSources(ids);
    m_PoseSources=std::move(ids);
    for (const auto& candidate:candidates) {
        const auto npc=candidate.npc;
        Pose pose;
        PartyNpcState state;
        if (!m_World.CaptureNpcPose(npc,pose) || !ValidFullPose(pose) || !m_World.CaptureNpc(npc,state) ||
            !ValidPartyNpcState(state) || state.life!=PartyNpcLife::Alive || state.stripped) continue;
        NpcPresentationPacket frame;
        frame.generation=candidate.generation;
        frame.transform=state.xf;
        auto& packet=frame.pose;
        packet.epoch=m_Scope.epoch;
        packet.timeMs=static_cast<uint32_t>(static_cast<uint64_t>(now*1000.));
        packet.id=npc;
        packet.boneCount=static_cast<uint16_t>(pose.rotations.size());
        packet.rootPos=pose.rootPos;
        packet.pelvisPos=pose.pelvisPos;
        packet.rotations=std::move(pose.rotations);
        auto bytes=Encode(frame);
        bytes.insert(bytes.begin(),kNpcPosePacket);
        m_Room.SendGame(m_Host ? PartyChannel::HostWorld : PartyChannel::Interaction,bytes,false);
    }
}
void PartyNpcSync::TickPoses(double now) {
    if(m_Host || m_Options.policy==PartyNpcPolicy::Explicit) SendPoses(now);
    for(auto& [npc,stream]:m_Poses) {
        PresentationFrame frame;
        const bool available=m_Replica->PresentingNpc(npc,stream.owner,stream.generation) &&
            now-stream.received<kPoseTimeout && stream.buffer.Sample(now,frame);
        if(available && m_World.PresentNpcPose(npc,frame.transform,frame.pose)) stream.applied=true;
        else if(stream.applied) {
            m_World.ClearNpcPresentationPose(npc);
            stream.applied=false;
        }
    }
}
void PartyNpcSync::Flush(double now) {
    if(!m_Active) return;
    for(size_t round=0;round<16;++round) {
        bool work=false;
        if(m_Host) {
            if(m_Host->Failed()) { Fail(); return; }
            PartyNpcDelivery delivery;
            while(m_Host->Pop(delivery)) {
                work=true;
                const auto bytes=EncodePartyNpc(delivery.message);
                if(bytes.empty()) { Fail(); return; }
                const bool reliable=delivery.message.kind!=PartyNpcKind::Canonical || delivery.message.state.life!=PartyNpcLife::Alive || delivery.message.state.stripped;
                if(!m_Room.SendGame(PartyChannel::HostWorld,bytes,reliable)) { Fail(); return; }
                if(!delivery.target || delivery.target==m_Self.id) ApplyHost(delivery.message);
            }
        }
        PartyNpcMessage reply;
        while(m_Replica->Pop(reply)) {
            work=true;
            if(m_Host) m_Host->Receive(m_Self,reply,now);
            else if(!m_Room.SendGame(PartyChannel::Interaction,EncodePartyNpc(reply),reply.kind!=PartyNpcKind::Report ||
                reply.state.life!=PartyNpcLife::Alive || reply.state.stripped)) { Fail(); return; }
        }
        if(m_Replica->Stopped() || (m_Host && m_Host->Failed())) { Fail(); return; }
        if(!work) return;
    }
    // A malformed callback cannot produce an unbounded game-thread message loop.
    Fail();
}
void PartyNpcSync::ConsiderAutomaticGuards(double now) {
    const auto freshLocation=[&](uint8_t seat) -> const PartyNpcLocation* {
        const auto location=m_Locations.find(seat);
        if (location==m_Locations.end() || location->second.observed>now ||
            now-location->second.observed>m_Options.locationMaxAge) return nullptr;
        return &location->second;
    };
    const auto* host=freshLocation(1);
    if (!host) return;
    std::array<const PartyNpcLocation*,4> guests {};
    for (const auto& member:m_Room.Status().members) {
        if (member.id!=1) guests[member.id-1]=freshLocation(member.id);
    }
    const auto canTransfer=[&](const PartyNpcAuthority::Record& record) {
        return record.phase==PartyNpcAuthority::Phase::Active && record.confirmed &&
            record.state.life==PartyNpcLife::Alive && !record.state.engaged && !record.state.interacting && !record.recovering &&
            now-record.changed>=m_Options.minimumHold &&
            (record.lastBusy<0. || now-record.lastBusy>=m_Options.minimumHold);
    };

    std::array<uint64_t*,4> freeSlots {};
    for (size_t seat=1;seat<guests.size();++seat) {
        const auto* guest=guests[seat];
        for (auto& npc:std::span(m_AutomaticGuards[seat]).first(m_Options.automaticGuardsPerGuest)) {
            if (npc) {
                const auto* record=m_Host->Find(npc);
                if (!record || record->phase!=PartyNpcAuthority::Phase::Active || !record->confirmed) continue;
                if (record->owner.id==1) {
                    npc=0;
                } else if (guest && record->owner.id==guest->peer.id && canTransfer(*record)) {
                    const float fromGuest=Distance(guest->position,record->state.xf.pos);
                    const float fromHost=Distance(host->position,record->state.xf.pos);
                    if (fromGuest>kGuardReleaseRadius || fromHost+kGuardDistanceMargin<fromGuest)
                        m_Host->Propose(npc,host->peer,now);
                }
            }
            if (!npc && !freeSlots[seat]) freeSlots[seat]=&npc;
        }
    }

    struct Candidate {
        uint64_t npc=0;
        float distance=kGuardApproachRadius;
    };
    std::array<Candidate,4> candidates {};
    for (const auto npc:m_Known) {
        const auto* record=m_Host->Find(npc);
        if (!record || record->owner.id!=1 || !canTransfer(*record)) continue;
        const float fromHost=Distance(host->position,record->state.xf.pos);
        size_t nearestSeat=0;
        float nearestDistance=kGuardApproachRadius;
        for (size_t seat=1;seat<guests.size();++seat) {
            if (!guests[seat]) continue;
            const float distance=Distance(guests[seat]->position,record->state.xf.pos);
            if (distance>=nearestDistance || distance+kGuardDistanceMargin>=fromHost) continue;
            nearestSeat=seat;
            nearestDistance=distance;
        }
        // The closest guest still takes precedence when all their slots are occupied.
        if (nearestSeat && freeSlots[nearestSeat] && nearestDistance<candidates[nearestSeat].distance)
            candidates[nearestSeat]={npc,nearestDistance};
    }
    for (size_t seat=1;seat<guests.size();++seat) {
        const auto npc=candidates[seat].npc;
        if (npc && m_Host->Propose(npc,guests[seat]->peer,now)) *freeSlots[seat]=npc;
    }
}

void PartyNpcSync::Tick(double now,bool allowOwnershipChanges) {
    if(!m_Active || !std::isfinite(now) || now<0) return;
    if(!Current()) { Stop(); return; }
    m_Replica->RetryPending();
    if(now>=m_NextReport) {
        m_NextReport=now+0.1;
        const bool refresh=now>=m_NextRefresh;
        if(refresh) m_NextRefresh=now+1.;
        for(uint64_t npc:m_Known) m_Replica->Report(npc,refresh);
    }
    if(m_Host) {
        m_Host->Tick(now);
        if(allowOwnershipChanges && now>=m_NextPolicy && m_Options.automaticGuardsPerGuest) {
            m_NextPolicy=now+0.2;
            ConsiderAutomaticGuards(now);
        } else if(allowOwnershipChanges && now>=m_NextPolicy && m_Options.policy==PartyNpcPolicy::NearestPlayer) {
            m_NextPolicy=now+0.5;
            std::vector<PartyNpcLocation> locations;
            for(const auto& [id,location]:m_Locations) locations.push_back(location);
            for(uint64_t npc:m_Known) m_Host->ConsiderNearest(npc,locations,now);
        }
    }
    Flush(now);
    if(m_Active && m_Options.streamPoses) TickPoses(now);
}
}
