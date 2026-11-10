#include "coop/PartyNpcSync.h"
#include <cmath>

namespace coop {
namespace {
bool HostKind(PartyNpcKind kind) {
    return kind==PartyNpcKind::Commit || kind==PartyNpcKind::Canonical || kind==PartyNpcKind::Quiesce || kind==PartyNpcKind::Prepare || kind==PartyNpcKind::Freeze;
}
bool Direct(PartyNpcKind kind) { return kind==PartyNpcKind::Quiesce || kind==PartyNpcKind::Prepare; }
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
    m_Replica=std::make_unique<PartyNpcReplica>(m_Scope,m_Self,m_World,m_Options.capacity);
    m_Host.reset();
    if(m_Room.Status().host) m_Host=std::make_unique<PartyNpcAuthority>(m_Scope,m_Options);
    if(m_Replica->Stopped() || (m_Host && m_Host->Failed())) { Fail(); return false; }
    m_Known.clear(); m_Locations.clear(); m_NextReport=m_NextPolicy=m_NextRefresh=0; m_Active=true;
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
    PartyNpcMessage message;
    if(!DecodePartyNpc(event.payload,message) || message.session!=m_Scope.session || message.epoch!=m_Scope.epoch) return false;
    if(event.channel==PartyChannel::HostWorld && event.source==1 && HostKind(message.kind)) {
        if(message.kind!=PartyNpcKind::Canonical && !event.reliable) return false;
        return ApplyHost(message);
    }
    if(event.channel==PartyChannel::Interaction && m_Host && !HostKind(message.kind)) {
        if(message.kind!=PartyNpcKind::Report && !event.reliable) return false;
        return m_Host->Receive({event.source,event.incarnation},message,now);
    }
    return false;
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
void PartyNpcSync::Tick(double now) {
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
        if(now>=m_NextPolicy) {
            m_NextPolicy=now+0.5;
            std::vector<PartyNpcLocation> locations;
            for(const auto& [id,location]:m_Locations) locations.push_back(location);
            for(uint64_t npc:m_Known) m_Host->ConsiderNearest(npc,locations,now);
        }
    }
    Flush(now);
}
}
