#include "coop/PartyNpcAuthority.h"
#include <cmath>
#include <limits>

namespace coop {
namespace {
bool Same(PartyIdentity a, PartyIdentity b) { return a.id==b.id && a.incarnation==b.incarnation; }
bool Time(double now) { return std::isfinite(now) && now>=0.; }
bool Position(Vec3 p) { return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z) &&
    std::fabs(p.x)<=100000.f && std::fabs(p.y)<=100000.f && std::fabs(p.z)<=100000.f; }
bool ScopeValid(const PartyNpcScope& scope) {
    size_t count=0; for(auto value:scope.incarnations) count+=value!=0;
    return scope.session && scope.epoch && scope.incarnations[0] && count>=2;
}
bool Progress(const PartyNpcState& before, const PartyNpcState& after) {
    // Waking a body needs its own validated interaction, not an old snapshot.
    return ValidPartyNpcState(after) && uint8_t(after.life)>=uint8_t(before.life) && (!before.stripped || after.stripped);
}
PartyNpcMessage MakeNpcMessage(const PartyNpcScope& scope, uint64_t npc, PartyNpcKind kind, uint64_t generation,
    uint64_t revision, PartyIdentity owner, const PartyNpcState& state, uint64_t sequence=0) {
    PartyNpcMessage message;
    message.kind=kind; message.session=scope.session; message.epoch=scope.epoch; message.npc=npc;
    message.generation=generation; message.revision=revision; message.owner=owner; message.state=state; message.sequence=sequence;
    return message;
}
}

bool PartyNpcState::operator==(const PartyNpcState& other) const {
    return xf.pos.x==other.xf.pos.x && xf.pos.y==other.xf.pos.y && xf.pos.z==other.xf.pos.z &&
        xf.rot.x==other.xf.rot.x && xf.rot.y==other.xf.rot.y && xf.rot.z==other.xf.rot.z && xf.rot.w==other.xf.rot.w &&
        life==other.life && stripped==other.stripped && engaged==other.engaged && interacting==other.interacting;
}
bool ValidPartyNpcState(const PartyNpcState& state) {
    const float length=Dot(state.xf.rot,state.xf.rot);
    return Position(state.xf.pos) && std::isfinite(length) && length>=0.25f && length<=4.f &&
        uint8_t(state.life)<=uint8_t(PartyNpcLife::Dead);
}
bool PartyNpcScope::FromRoom(const PartyRoomStatus& room, PartyNpcScope& result) {
    if (room.phase!=LobbyRoster::Phase::Playing || !room.session || !room.epoch || room.members.size()<2 || room.members.size()>4 ||
        (room.state!=PartyRoomStatus::State::Hosting && room.state!=PartyRoomStatus::State::Connected)) return false;
    PartyNpcScope scope; scope.session=room.session; scope.epoch=room.epoch;
    bool self=false;
    for (const auto& member:room.members) {
        if (!member.id || member.id>4 || !member.incarnation || !member.loaded || scope.incarnations[member.id-1]) return false;
        scope.incarnations[member.id-1]=member.incarnation;
        self=self || member.id==room.self;
    }
    if (!self || !ScopeValid(scope)) return false;
    result=scope; return true;
}
bool PartyNpcScope::Contains(PartyIdentity peer) const {
    return peer.id>=1 && peer.id<=4 && peer.incarnation && incarnations[peer.id-1]==peer.incarnation;
}

std::vector<uint8_t> EncodePartyNpc(const PartyNpcMessage& message) {
    if (!message.session || !message.epoch || !message.npc || !message.generation || !message.revision ||
        message.owner.id<1 || message.owner.id>4 || !message.owner.incarnation || !ValidPartyNpcState(message.state) ||
        uint8_t(message.kind)<1 || uint8_t(message.kind)>9 ||
        ((message.kind==PartyNpcKind::Released || message.kind==PartyNpcKind::Report || message.kind==PartyNpcKind::Declined) && !message.sequence)) return {};
    ByteWriter w;
    w.U32(0x314e3448); // H4N1: experimental NPC codec, distinct from the avatar protocol.
    w.U8(uint8_t(message.kind)); w.U64(message.session); w.U32(message.epoch); w.U64(message.npc);
    w.U64(message.generation); w.U64(message.revision); w.U64(message.sequence);
    w.U8(message.owner.id); w.U64(message.owner.incarnation);
    w.F32(message.state.xf.pos.x); w.F32(message.state.xf.pos.y); w.F32(message.state.xf.pos.z);
    w.F32(message.state.xf.rot.x); w.F32(message.state.xf.rot.y); w.F32(message.state.xf.rot.z); w.F32(message.state.xf.rot.w);
    w.U8(uint8_t(message.state.life)); w.Bool(message.state.stripped); w.Bool(message.state.engaged); w.Bool(message.state.interacting);
    return w.Data();
}
bool DecodePartyNpc(const std::vector<uint8_t>& bytes, PartyNpcMessage& result) {
    if (bytes.empty() || bytes.size()>128) return false;
    PartyNpcMessage m; ByteReader r(bytes); uint32_t magic=0; uint8_t kind=0,life=0;
    if (!r.U32(magic) || magic!=0x314e3448 || !r.U8(kind) || kind<1 || kind>9 ||
        !r.U64(m.session) || !r.U32(m.epoch) || !r.U64(m.npc) || !r.U64(m.generation) || !r.U64(m.revision) || !r.U64(m.sequence) ||
        !r.U8(m.owner.id) || !r.U64(m.owner.incarnation) ||
        !r.F32(m.state.xf.pos.x) || !r.F32(m.state.xf.pos.y) || !r.F32(m.state.xf.pos.z) ||
        !r.F32(m.state.xf.rot.x) || !r.F32(m.state.xf.rot.y) || !r.F32(m.state.xf.rot.z) || !r.F32(m.state.xf.rot.w) ||
        !r.U8(life) || !r.Bool(m.state.stripped) || !r.Bool(m.state.engaged) || !r.Bool(m.state.interacting) || !r.AtEnd()) return false;
    m.kind=PartyNpcKind(kind); m.state.life=PartyNpcLife(life);
    if (EncodePartyNpc(m).empty()) return false;
    result=m; return true;
}

PartyNpcAuthority::PartyNpcAuthority(PartyNpcScope scope, PartyNpcOptions options):m_Scope(scope),m_Options(options) {
    if ((options.policy!=PartyNpcPolicy::HostOnly && options.policy!=PartyNpcPolicy::NearestPlayer) ||
        !ScopeValid(scope) || !options.capacity || options.capacity>4096 || !options.queueLimit || options.queueLimit>16384 ||
        !Time(options.retry) || options.retry<0.05 || !Time(options.transferTimeout) || options.transferTimeout<=options.retry ||
        !Time(options.minimumHold) || !Time(options.locationMaxAge) || options.locationMaxAge<=0 ||
        !Time(options.baselineInterval) || options.baselineInterval<options.retry ||
        !std::isfinite(options.hysteresis) || options.hysteresis<0 || !std::isfinite(options.delegationRadius) || options.delegationRadius<=0)
        m_Failed=m_Stopped=true;
}
const PartyNpcAuthority::Record* PartyNpcAuthority::Find(uint64_t npc) const {
    const auto it=m_Npcs.find(npc); return it==m_Npcs.end()?nullptr:&it->second;
}
bool PartyNpcAuthority::Emit(uint8_t target,uint64_t npc,PartyNpcKind kind,const Record& record) {
    if (m_Failed) return false;
    if (m_Out.size()>=m_Options.queueLimit) { m_Out.clear(); m_Failed=m_Stopped=true; return false; }
    const bool pending=kind==PartyNpcKind::Quiesce || kind==PartyNpcKind::Prepare;
    m_Out.push_back({target,MakeNpcMessage(m_Scope,npc,kind,pending?record.nextGeneration:record.generation,record.revision,
        kind==PartyNpcKind::Prepare?record.next:record.owner,record.state)});
    return true;
}
bool PartyNpcAuthority::Register(uint64_t npc,const PartyNpcState& state,double now) {
    if (m_Stopped || !npc || !Time(now) || !ValidPartyNpcState(state) || m_Npcs.contains(npc) || m_Npcs.size()>=m_Options.capacity) return false;
    Record record; record.owner={1,m_Scope.incarnations[0]}; record.state=state; record.changed=now; record.retryAt=now+m_Options.retry; record.snapshotAt=now+m_Options.baselineInterval;
    m_Npcs.emplace(npc,record); return Emit(0,npc,PartyNpcKind::Commit,record);
}
bool PartyNpcAuthority::Propose(uint64_t npc,PartyIdentity next,double now) {
    if (m_Options.policy==PartyNpcPolicy::HostOnly) return false;
    auto it=m_Npcs.find(npc);
    if (m_Stopped || !Time(now) || !m_Scope.Contains(next) || it==m_Npcs.end()) return false;
    auto& record=it->second;
    if (record.phase!=Phase::Active || Same(record.owner,next) || record.state.life!=PartyNpcLife::Alive || record.state.engaged || record.state.interacting)
        return false;
    if (record.issuedGeneration==std::numeric_limits<uint64_t>::max()) { Stop(); m_Failed=true; return false; }
    record.phase=Phase::Releasing; record.next=next; record.nextGeneration=++record.issuedGeneration;
    record.deadline=now+m_Options.transferTimeout; record.retryAt=now+m_Options.retry;
    return Emit(record.owner.id,npc,PartyNpcKind::Quiesce,record);
}
bool PartyNpcAuthority::ConsiderNearest(uint64_t npc,const std::vector<PartyNpcLocation>& locations,double now) {
    const auto* record=Find(npc);
    if (!record || !Time(now) || locations.size()>4 || now-record->changed<m_Options.minimumHold) return false;
    const PartyNpcLocation* best=nullptr; float bestDistance=m_Options.delegationRadius, currentDistance=std::numeric_limits<float>::infinity();
    bool seen[5] {};
    for (const auto& location:locations) {
        if (!m_Scope.Contains(location.peer) || seen[location.peer.id] || !Position(location.position) || !Time(location.observed)) return false;
        seen[location.peer.id]=true;
        if(location.observed>now || now-location.observed>m_Options.locationMaxAge) continue;
        const float distance=Distance(record->state.xf.pos,location.position);
        if (Same(location.peer,record->owner)) currentDistance=distance;
        if (distance<bestDistance || (distance==bestDistance && best && location.peer.id<best->peer.id)) { best=&location; bestDistance=distance; }
    }
    // Missing current-owner position is not evidence to move its simulation.
    if (!best || !std::isfinite(currentDistance) || bestDistance+m_Options.hysteresis>=currentDistance) return false;
    return Propose(npc,best->peer,now);
}
bool PartyNpcAuthority::Adopt(Record& record,const PartyNpcState& state) {
    if (!Progress(record.state,state)) return false;
    if (!(record.state==state)) {
        if (record.revision==std::numeric_limits<uint64_t>::max()) { m_Failed=m_Stopped=true; m_Out.clear(); return false; }
        record.state=state; ++record.revision;
    }
    return true;
}
bool PartyNpcAuthority::Receive(PartyIdentity source,const PartyNpcMessage& message,double now) {
    if (m_Stopped || !Time(now) || !m_Scope.Contains(source) || !Same(source,message.owner) ||
        message.session!=m_Scope.session || message.epoch!=m_Scope.epoch || EncodePartyNpc(message).empty()) return false;
    auto it=m_Npcs.find(message.npc); if(it==m_Npcs.end()) return false;
    auto& record=it->second;
    if(message.kind==PartyNpcKind::Declined) {
        if(record.phase!=Phase::Releasing || !Same(source,record.owner) || message.generation!=record.nextGeneration ||
            message.sequence<=record.reportSequence || (!message.state.engaged && !message.state.interacting && message.state.life==PartyNpcLife::Alive) ||
            !Adopt(record,message.state)) return false;
        record.reportSequence=message.sequence; record.confirmed=true;
        record.phase=Phase::Active; record.next={}; record.nextGeneration=0; record.changed=now;
        return Emit(0,message.npc,PartyNpcKind::Canonical,record);
    }
    if (message.kind==PartyNpcKind::Report) {
        if ((record.phase!=Phase::Active && record.phase!=Phase::Releasing) || !Same(source,record.owner) ||
            message.generation!=record.generation || message.sequence<=record.reportSequence) return false;
        const auto previous=record.revision;
        if (!Adopt(record,message.state)) return false;
        record.reportSequence=message.sequence; record.confirmed=true;
        return previous==record.revision || Emit(0,message.npc,PartyNpcKind::Canonical,record);
    }
    if (message.kind==PartyNpcKind::Released) {
        if (record.phase==Phase::Preparing && Same(source,record.owner) && message.generation==record.nextGeneration && message.sequence==record.reportSequence)
            return Emit(record.next.id,message.npc,PartyNpcKind::Prepare,record);
        if (record.phase!=Phase::Releasing || !Same(source,record.owner) || message.generation!=record.nextGeneration ||
            message.sequence<=record.reportSequence || !Adopt(record,message.state)) return false;
        record.reportSequence=message.sequence;
        // Preserve a fight/takedown that began while the release was in flight.
        if (record.state.engaged || record.state.interacting || record.state.life!=PartyNpcLife::Alive) record.next=record.owner;
        record.phase=Phase::Preparing; record.retryAt=now+m_Options.retry;
        return Emit(record.next.id,message.npc,PartyNpcKind::Prepare,record);
    }
    if (message.kind==PartyNpcKind::Prepared) {
        if (record.phase!=Phase::Preparing || !Same(source,record.next) || message.generation!=record.nextGeneration || message.revision!=record.revision) return false;
        if (record.revision==std::numeric_limits<uint64_t>::max()) { Stop(); m_Failed=true; return false; }
        record.owner=record.next; record.generation=record.nextGeneration; record.next={}; record.nextGeneration=0;
        record.phase=Phase::Active; record.changed=now; record.reportSequence=0; record.confirmed=false; ++record.revision;
        record.snapshotAt=now+m_Options.baselineInterval;
        record.retryAt=now+m_Options.retry;
        return Emit(0,message.npc,PartyNpcKind::Commit,record);
    }
    return false;
}
void PartyNpcAuthority::Suspend(uint64_t npc,Record& record) {
    record.phase=Phase::Suspended;
    if(record.nextGeneration) record.generation=record.nextGeneration;
    record.nextGeneration=0;
    Emit(0,npc,PartyNpcKind::Freeze,record);
}
void PartyNpcAuthority::Tick(double now) {
    if (m_Stopped || !Time(now)) return;
    for (auto& [npc,record]:m_Npcs) {
        if ((record.phase==Phase::Releasing || record.phase==Phase::Preparing) && now>=record.deadline) { Suspend(npc,record); continue; }
        if(record.phase==Phase::Active && record.confirmed && now>=record.snapshotAt) {
            record.snapshotAt=now+m_Options.baselineInterval;
            Emit(0,npc,PartyNpcKind::Canonical,record);
        }
        if (now<record.retryAt) continue;
        record.retryAt=now+m_Options.retry;
        if(record.phase==Phase::Releasing) Emit(record.owner.id,npc,PartyNpcKind::Quiesce,record);
        else if(record.phase==Phase::Preparing) Emit(record.next.id,npc,PartyNpcKind::Prepare,record);
        else if(record.phase==Phase::Active && !record.confirmed) Emit(record.owner.id,npc,PartyNpcKind::Commit,record);
        else if(record.phase==Phase::Suspended) Emit(0,npc,PartyNpcKind::Freeze,record);
        if (m_Failed) break;
    }
}
void PartyNpcAuthority::Stop() {
    if (m_Stopped) return;
    m_Stopped=true;
    for(auto& [npc,record]:m_Npcs) {
        if(record.issuedGeneration<std::numeric_limits<uint64_t>::max()) record.nextGeneration=++record.issuedGeneration;
        Suspend(npc,record);
        if(m_Failed) break;
    }
}
bool PartyNpcAuthority::Pop(PartyNpcDelivery& delivery) {
    if (m_Failed || m_Out.empty()) return false;
    delivery=std::move(m_Out.front()); m_Out.pop_front(); return true;
}

PartyNpcReplica::PartyNpcReplica(PartyNpcScope scope,PartyIdentity self,IPartyNpcWorld& world,size_t limit)
    :m_Scope(scope),m_Self(self),m_World(world),m_Limit(limit) {
    m_Stopped=!ScopeValid(scope) || !scope.Contains(self) || !limit || limit>4096;
}
PartyNpcReplica::~PartyNpcReplica() { Stop(); }
bool PartyNpcReplica::Emit(PartyNpcMessage message) {
    if(m_Out.size()>=4096) { Stop(); return false; }
    m_Out.push_back(std::move(message)); return true;
}
bool PartyNpcReplica::Receive(PartyIdentity source,PartyChannel channel,const PartyNpcMessage& message) {
    if(m_Stopped || source.id!=1 || !m_Scope.Contains(source) || channel!=PartyChannel::HostWorld ||
        message.session!=m_Scope.session || message.epoch!=m_Scope.epoch || !m_Scope.Contains(message.owner) || EncodePartyNpc(message).empty()) return false;
    const auto kind=message.kind;
    if(kind!=PartyNpcKind::Commit && kind!=PartyNpcKind::Canonical && kind!=PartyNpcKind::Quiesce && kind!=PartyNpcKind::Prepare && kind!=PartyNpcKind::Freeze) return false;
    auto it=m_Npcs.find(message.npc);
    if(it==m_Npcs.end()) {
        if(kind!=PartyNpcKind::Commit || m_Npcs.size()>=m_Limit) return false;
        it=m_Npcs.emplace(message.npc,Local{}).first;
    }
    auto& local=it->second;
    const auto defer=[&]() { local.pending=message; return false; };
    if(kind==PartyNpcKind::Freeze) {
        if(message.generation<local.generation || message.generation<local.fence) return false;
        local.suspended=true; local.fence=message.generation;
        if(!m_World.FreezeNpc(message.npc)) return defer();
        local.running=false; local.frozen=true; local.generation=message.generation; local.prepared=0;
        local.pending.reset();
        return true;
    }
    if(local.suspended || message.generation<local.fence) return false;
    if(kind==PartyNpcKind::Quiesce) {
        if(!Same(local.owner,m_Self) || !Same(message.owner,m_Self) || message.generation<=local.generation || message.generation<local.attempt) return false;
        local.attempt=message.generation;
        if(local.cachedRelease.generation==message.generation) return Emit(local.cachedRelease);
        PartyNpcState before;
        if(!local.frozen && m_World.CaptureNpc(message.npc,before) && Progress(local.state,before) &&
            (before.engaged || before.interacting || before.life!=PartyNpcLife::Alive)) {
            if(local.sequence==std::numeric_limits<uint64_t>::max()) return false;
            local.state=before;
            local.cachedRelease=MakeNpcMessage(m_Scope,message.npc,PartyNpcKind::Declined,message.generation,local.revision,m_Self,before,++local.sequence);
            return Emit(local.cachedRelease);
        }
        if(!m_World.FreezeNpc(message.npc)) return defer();
        local.fence=message.generation;
        local.running=false; local.frozen=true;
        PartyNpcState state;
        if(!m_World.CaptureNpc(message.npc,state)) return defer();
        if(!Progress(local.state,state) || local.sequence==std::numeric_limits<uint64_t>::max()) return false;
        local.state=state;
        local.cachedRelease=MakeNpcMessage(m_Scope,message.npc,PartyNpcKind::Released,message.generation,local.revision,m_Self,state,++local.sequence);
        local.pending.reset();
        return Emit(local.cachedRelease);
    }
    if(kind==PartyNpcKind::Prepare) {
        if(!Same(message.owner,m_Self) || message.generation<=local.generation || !Progress(local.state,message.state)) return false;
        if(local.prepared==message.generation && local.revision==message.revision)
            return Emit(MakeNpcMessage(m_Scope,message.npc,PartyNpcKind::Prepared,message.generation,message.revision,m_Self,message.state));
        local.fence=message.generation;
        if(!m_World.FreezeNpc(message.npc)) return defer();
        local.running=false; local.frozen=true;
        if(!m_World.ApplyNpcBaseline(message.npc,message.state)) return defer();
        local.state=message.state; local.prepared=message.generation; local.revision=message.revision;
        local.pending.reset();
        return Emit(MakeNpcMessage(m_Scope,message.npc,PartyNpcKind::Prepared,message.generation,message.revision,m_Self,message.state));
    }
    if(kind==PartyNpcKind::Canonical && message.generation>local.generation && !Same(message.owner,m_Self)) {
        auto baseline=message; baseline.kind=PartyNpcKind::Commit;
        return Receive(source,channel,baseline);
    }
    if(kind==PartyNpcKind::Canonical && local.pending && local.pending->kind==PartyNpcKind::Commit &&
        local.pending->generation==message.generation && Same(local.pending->owner,message.owner) && message.revision>local.pending->revision) {
        auto baseline=message; baseline.kind=PartyNpcKind::Commit;
        return Receive(source,channel,baseline);
    }
    if(message.generation<local.generation || (message.generation==local.generation && !Same(message.owner,local.owner))) return false;
    if(kind==PartyNpcKind::Canonical) {
        if(message.generation!=local.generation || message.revision<=local.revision || !Progress(local.state,message.state)) return false;
        if(!Same(local.owner,m_Self) && !m_World.ApplyNpcBaseline(message.npc,message.state)) return defer();
        local.state=message.state; local.revision=message.revision;
        local.pending.reset();
        return true;
    }
    if(message.generation==local.generation && Same(message.owner,local.owner) && message.revision>local.revision) {
        // Refreshing one lease must never reset its report sequence or restart AI.
        auto canonical=message; canonical.kind=PartyNpcKind::Canonical;
        return Receive(source,channel,canonical);
    }
    if(message.generation==local.generation && message.revision<=local.revision) return true;
    const bool mine=Same(message.owner,m_Self);
    if(mine && message.generation>local.generation && local.prepared!=message.generation && !(local.generation==0 && message.generation==1 && m_Self.id==1)) return false;
    if(!Progress(local.state,message.state)) return false;
    if(!m_World.FreezeNpc(message.npc)) return defer();
    local.running=false; local.frozen=true;
    if(!m_World.ApplyNpcBaseline(message.npc,message.state)) return defer();
    if(mine && message.state.life==PartyNpcLife::Alive && !m_World.ResumeNpc(message.npc)) return defer();
    local.running=mine && message.state.life==PartyNpcLife::Alive; local.frozen=!mine;
    local.owner=message.owner; local.generation=message.generation; local.revision=message.revision;
    local.fence=message.generation;
    local.prepared=0; local.sequence=0; local.cachedRelease={}; local.state=message.state;
    local.reported.reset();
    local.pending.reset();
    return true;
}
bool PartyNpcReplica::Report(uint64_t npc,bool refresh) {
    if(m_Stopped) return false;
    auto it=m_Npcs.find(npc); if(it==m_Npcs.end()) return false;
    auto& local=it->second;
    if(!Same(local.owner,m_Self) || local.frozen || local.suspended || local.sequence==std::numeric_limits<uint64_t>::max()) return false;
    PartyNpcState state;
    if(!m_World.CaptureNpc(npc,state) || !Progress(local.state,state)) return false;
    if(state.life!=PartyNpcLife::Alive && local.running) {
        if(!m_World.FreezeNpc(npc)) return false;
        local.running=false;
    }
    if(!refresh && local.reported && *local.reported==state) return true;
    local.state=state;
    if(!Emit(MakeNpcMessage(m_Scope,npc,PartyNpcKind::Report,local.generation,local.revision,m_Self,state,++local.sequence))) return false;
    local.reported=state; return true;
}
bool PartyNpcReplica::Pop(PartyNpcMessage& message) {
    if(m_Out.empty()) return false;
    message=std::move(m_Out.front()); m_Out.pop_front(); return true;
}
bool PartyNpcReplica::Running(uint64_t npc) const {
    const auto it=m_Npcs.find(npc); return it!=m_Npcs.end() && it->second.running;
}
void PartyNpcReplica::RetryPending() {
    if(m_Stopped) return;
    for(auto& [npc,local]:m_Npcs) {
        if(!local.pending) continue;
        auto pending=*local.pending;
        local.pending.reset();
        Receive({1,m_Scope.incarnations[0]},PartyChannel::HostWorld,pending);
    }
}
bool PartyNpcReplica::Stop() {
    m_Stopped=true; m_Out.clear();
    m_StopConfirmed=true;
    for(auto& [npc,local]:m_Npcs) {
        if(m_World.FreezeNpc(npc)) local.running=false;
        else m_StopConfirmed=false;
        local.frozen=true;
    }
    return m_StopConfirmed;
}
}
