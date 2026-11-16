#include "coop/PartyPreviewRuntime.h"
#include "coop/LauncherPolicy.h"
#include <cmath>
#include <atomic>
#include <limits>

namespace coop {
namespace {
uint64_t NextRetirement() {
    static std::atomic<uint64_t> issued=0;
    auto previous=issued.load();
    while (previous!=std::numeric_limits<uint64_t>::max()) {
        if (issued.compare_exchange_weak(previous,previous+1)) return previous+1;
    }
    return 0; // Exhaustion cannot recycle a receipt from another scene/room.
}
}
PartyPreviewRuntime::PartyPreviewRuntime(std::unique_ptr<PartyRoom> room, IGameWorld& source, IPartyAvatarWorld& sink,
                                       IPartyNpcWorld* npcWorld, PartyNpcOptions npcOptions,
                                       bool followHostRestart, bool autoInitialLoad, IPartyFreeformLaunch* launcher,
                                       bool exchangeNpcRoster, bool prepareAgreedNpcCatalog)
    : m_Room(std::move(room)), m_Source(source), m_Sink(sink), m_Playback(sink),
      m_NpcWorld(npcWorld), m_NpcOptions(npcOptions), m_PrepareAgreedNpcCatalog(npcWorld && prepareAgreedNpcCatalog),
      m_AutoInitialLoad(autoInitialLoad && (!npcWorld || m_PrepareAgreedNpcCatalog)),
      m_Launcher(m_AutoInitialLoad ? launcher : nullptr),
      m_FollowHostRestart(followHostRestart && (!npcWorld || m_PrepareAgreedNpcCatalog)) {
    if (exchangeNpcRoster || m_PrepareAgreedNpcCatalog) m_NpcRoster=std::make_unique<PartyNpcRoster>(*m_Room);
    if (m_PrepareAgreedNpcCatalog) {
        m_NpcOptions.fixedCatalog.clear();
        m_NpcOptions.confirmInitialGuestBaselines=true;
    }
}

void PartyPreviewRuntime::ObserveNpcRoster(std::span<const uint64_t> ids) {
    if (m_NpcRoster) m_NpcRoster->Update(ids);
}

PartyPreviewRuntime::~PartyPreviewRuntime() {
    m_Sink.SetPartyPause(false,false);
    CancelGuestLaunch();
    CancelRestart();
    if (m_NpcCatalogStarted) StopNpcs();
}

void PartyPreviewRuntime::StopNpcs() {
    if (m_Npcs) {
        if (m_Npcs->Active() || !m_Npcs->StopConfirmed()) m_Npcs->Stop();
        if (!m_Npcs->StopConfirmed()) m_NpcStopFault=true;
    }
    if (m_NpcCatalogStarted) {
        m_Npcs.reset();
        m_NpcWorld->StopNpcCatalog();
        m_NpcCatalogStarted=false;
        m_NpcScenePrepared=false;
        m_NpcCatalogSpent=true;
    }
}

bool PartyPreviewRuntime::InterruptNpcs(const char* reason) {
    StopNpcs();
    m_Message=reason;
    m_Room->ReportSceneLost();
    return false;
}

void PartyPreviewRuntime::PrepareNpcs() {
    if (m_NpcOptions.fixedCatalog.empty()) {
        m_NpcScenePrepared=m_NpcWorld->PrepareNpcScene();
    } else {
        if (!m_NpcOptions.ValidCatalog()) {
            InterruptNpcs("Invalid shared NPC catalog.");
            return;
        }
        m_NpcCatalogStarted=true;
        using Preparation=IPartyNpcWorld::CatalogPreparation;
        const auto result=m_NpcWorld->PrepareNpcCatalog(m_NpcOptions.fixedCatalog);
        if (result==Preparation::Refused) {
            InterruptNpcs("Native preparation refused the shared NPC catalog.");
            return;
        }
        m_NpcScenePrepared=result==Preparation::Ready;
    }
    if (!m_NpcScenePrepared) m_Message="Waiting for native NPC simulation to stop before starting.";
}

bool PartyPreviewRuntime::RegisterFixedCatalog(double now) {
    std::vector<std::pair<uint64_t,PartyNpcState>> baselines;
    for (const auto id:m_NpcOptions.fixedCatalog) {
        PartyNpcState state;
        if (!m_NpcWorld->CaptureNpc(id,state) || !ValidPartyNpcState(state))
            return InterruptNpcs("A prepared shared NPC could not be captured.");
        baselines.emplace_back(id,state);
    }
    for (const auto& [id,state]:baselines) {
        if (!m_Npcs->Register(id,state,now))
            return InterruptNpcs("Shared NPC catalog registration failed.");
        m_NpcCatalog.insert(id);
    }
    return true;
}

bool PartyPreviewRuntime::PrepareAgreedNpcs() {
    const auto& shared=m_NpcRoster->Shared();
    if (m_NpcCatalogStarted) {
        if (!shared || !std::includes(shared->begin(),shared->end(),m_NpcOptions.fixedCatalog.begin(),m_NpcOptions.fixedCatalog.end()))
            return InterruptNpcs("The agreed NPC catalog changed. Reload the mission.");
    } else {
        if (!shared || shared->empty()) return false;
        m_NpcOptions.fixedCatalog=std::set<uint64_t>(shared->begin(),shared->end());
    }
    if (!m_NpcScenePrepared) PrepareNpcs();
    return m_NpcScenePrepared;
}

bool PartyPreviewRuntime::UpdateNpcs(double now, bool presenting) {
    if (!m_NpcWorld) return false;
    if (m_PrepareAgreedNpcCatalog && Status().phase==LobbyRoster::Phase::Restarting) return false;
    if (m_NpcCatalogStarted && Status().phase==LobbyRoster::Phase::Loading && presenting && NativeReady()) return false;
    if (!presenting || !NativeReady() || Status().phase!=LobbyRoster::Phase::Playing) {
        if (m_NpcCatalogStarted) return InterruptNpcs("Shared NPC session stopped.");
        StopNpcs(); return false;
    }
    if (m_PrepareAgreedNpcCatalog && !PrepareAgreedNpcs()) return false;
    if (!m_NpcScenePrepared) return InterruptNpcs("Native NPC preparation was not confirmed.");
    if (!m_Npcs) m_Npcs=std::make_unique<PartyNpcSync>(*m_Room,*m_NpcWorld,m_NpcOptions);
    if (!m_Npcs->Active()) {
        if (!m_Npcs->Start()) return InterruptNpcs("Shared NPC controller could not start. Stop the session and reload the mission.");
        m_Message="Shared NPC development session running.";
    }
    if (!m_NpcOptions.fixedCatalog.empty())
        return !Status().host || !m_NpcCatalog.empty() || RegisterFixedCatalog(now);
    if (Status().host && now>=m_NextNpcCatalog) {
        m_NextNpcCatalog=now+1.;
        std::vector<NpcState> candidates;
        m_Source.ListNpcs(candidates); // Existing stable-ID catalog, not a new scanner.
        std::map<uint64_t,size_t> counts;
        for (const auto& npc:candidates) ++counts[npc.id];
        for (uint64_t id:m_NpcCatalog) {
            if (counts[id]>1) {
                m_Message="A shared NPC ID became ambiguous. Reload the mission.";
                StopNpcs(); m_Room->ReportSceneLost(); return false;
            }
        }
        for (const auto& npc:candidates) {
            if (!npc.id || counts[npc.id]!=1 || m_NpcCatalog.contains(npc.id)) continue;
            if (m_NpcCatalog.size()>=m_NpcOptions.capacity) {
                m_Message="Shared NPC catalog exceeded its supported capacity.";
                StopNpcs(); m_Room->ReportSceneLost(); return false;
            }
            PartyNpcState state;
            if (!m_NpcWorld->CaptureNpc(npc.id,state)) continue;
            if (!ValidPartyNpcState(state) || !m_Npcs->Register(npc.id,state,now)) {
                m_Message="Shared NPC catalog registration failed.";
                StopNpcs(); m_Room->ReportSceneLost(); return false;
            }
            m_NpcCatalog.insert(npc.id);
        }
    }
    return m_Npcs->Active();
}

bool PartyPreviewRuntime::NativeSceneAvailable() const {
    Transform transform;
    return m_Source.IsInScene() && IsFreeformTrainingScene(m_Source.CurrentScene().scene) &&
        (!m_Launcher || m_Launcher->NativeFreeformContract()) && m_Source.GetLocalAvatar(transform);
}
bool PartyPreviewRuntime::NativeReady() const {
    return !m_NpcStopFault && !m_NpcCatalogSpent && !m_NpcRetirementPending && m_Sink.PartyPreviewError().empty() &&
        NativeSceneAvailable();
}
bool PartyPreviewRuntime::LocalReady() const {
    for (const auto& member : Status().members)
        if (member.id == Status().self)
            return member.ready;
    return false;
}
bool PartyPreviewRuntime::Ready() {
    if (m_AutoInitialLoad) return false;
    if (!NativeReady()) { m_Message="Load Freeform Training first, then click Ready."; return false; }
    const bool ok = m_Room->SetReady(true, !LocalReady());
    m_Message = ok ? "Ready state sent." : "Ready is available in the preview lobby.";
    return ok;
}
bool PartyPreviewRuntime::Start() {
    if (m_AutoInitialLoad) return false;
    const bool ok = NativeReady() && m_Room->BeginLoad();
    m_Message = ok ? "Checking that every player has Freeform Training loaded." : "The host can start after every player is ready.";
    return ok;
}
bool PartyPreviewRuntime::Lobby() {
    if (!Status().host) { m_Message="Only the host can stop the preview."; return false; }
    StopNpcs();
    if (Status().phase == LobbyRoster::Phase::Playing || LobbyRoster::AwaitingLoad(Status().phase) || Status().phase == LobbyRoster::Phase::Completing)
        m_Room->ReportSceneLost();
    const bool ok = m_Room->ReturnToLobby() && m_Room->SelectMission(kPartyFreeformContract);
    if (ok) {
        m_Playback.Reset();
        m_InitialLoadDeadline=-1.;
    }
    if (!ok) m_Message="There is no running or interrupted preview to stop.";
    else if (m_AutoInitialLoad) m_Message="Session stopped. Return to HITMAN's menu and load Freeform Training to try again.";
    else m_Message="Preview stopped. Everyone must ready again.";
    return ok;
}
bool PartyPreviewRuntime::Reconnect(double now) {
    const bool ok = !Status().host && m_Room->Reconnect(now);
    m_Message = ok ? "Reconnecting to the host." : "Reconnect is available after a guest loses the host.";
    return ok;
}
bool PartyPreviewRuntime::HostMissionRestarted() {
    if (!m_FollowHostRestart || !Status().host || !NativeReady() || !m_Room->BeginRestart()) return false;
    m_Message="Host restarted the mission. Waiting for every PC to reload.";
    return true;
}
void PartyPreviewRuntime::CancelRestart() {
    if (!m_RestartPending) return;
    m_Source.CancelRestartMission();
    m_RestartPending=false;
}
void PartyPreviewRuntime::UpdateRestart(double now,bool native) {
    const auto& status=Status();
    if (status.phase!=LobbyRoster::Phase::Restarting ||
        (status.state!=PartyRoomStatus::State::Hosting && status.state!=PartyRoomStatus::State::Connected)) {
        if (m_RestartSince>=0. && status.phase==LobbyRoster::Phase::Playing && m_RestartCleared==status.epoch) {
            m_Message="Every player reloaded. Preview resumed.";
            m_Source.Log(LogLevel::Info,"party-restart","All PCs loaded mission epoch "+std::to_string(status.epoch)+" in the same room.");
        }
        CancelRestart();
        m_RestartSince=-1.;
        m_RestartFailed=false;
        return;
    }
    if (m_RestartFailed) return;
    if (m_RestartSince<0.) m_RestartSince=now;
    if (!m_FollowHostRestart || now-m_RestartSince>=60.) {
        CancelRestart();
        m_RestartFailed=m_Room->ReportSceneLost();
        if (m_RestartFailed) m_Message="Mission restart did not finish. Return to the lobby to retry.";
        return;
    }
    if (!status.host && native && m_RestartCleared!=status.epoch && m_RestartRequested!=status.epoch) {
        m_RestartRequested=status.epoch;
        m_RestartPending=true;
        m_Source.RestartMission();
        m_Message="Following the host's restart.";
        m_Source.Log(LogLevel::Info,"party-restart","Queued native guest restart for epoch "+std::to_string(status.epoch)+".");
    }
}
void PartyPreviewRuntime::Leave(double now) {
    m_Sink.SetPartyPause(false,false);
    CancelGuestLaunch();
    CancelRestart();
    StopNpcs();
    m_Playback.Reset();
    m_Room->Leave(now);
    m_Message="Leaving preview. Reload the mission before creating a new room.";
}
void PartyPreviewRuntime::SceneCleared() {
    m_Sink.SetPartyPause(false,false);
    const bool restarting=m_FollowHostRestart && Status().phase==LobbyRoster::Phase::Restarting;
    if (restarting) {
        m_RestartCleared=Status().epoch;
        m_Source.Log(LogLevel::Info,"party-restart","Native scene clear observed for epoch "+std::to_string(m_RestartCleared)+".");
    }
    CancelRestart();
    const bool retire=m_NpcWorld && ((!m_PrepareAgreedNpcCatalog && m_NpcOptions.fixedCatalog.empty()) ||
        m_NpcCatalogStarted || m_NpcCatalogSpent);
    StopNpcs();
    m_Npcs.reset(); m_NpcCatalog.clear(); m_NextNpcCatalog=0;
    if (m_PrepareAgreedNpcCatalog) m_NpcOptions.fixedCatalog.clear();
    if (retire) {
        m_NpcRetirementPending=true;
        m_NpcRetirement=NextRetirement();
        if (m_NpcRetirement) m_NpcWorld->BeginNpcSceneRetirement(m_NpcRetirement);
    }
    m_NpcScenePrepared=false;
    // Clearing the front end is part of loading the mission. Losing a
    // reported native scene still interrupts the room.
    const bool entering=m_AutoInitialLoad && Status().phase==LobbyRoster::Phase::Loading && !m_HadNativeScene;
    if (!restarting && !entering) m_Room->ReportSceneLost();
    m_Playback.Reset();
    m_HadNativeScene=false;
    m_Epoch=0;
    m_NextSend=m_NextMetadata=m_NextLoaded=0;
}
void PartyPreviewRuntime::UpdateInitialLoad(double now, bool native) {
    if (!m_AutoInitialLoad || !Status().host) return;
    if (Status().phase==LobbyRoster::Phase::Lobby) {
        if (!native) m_AutoLoadArmed=true;
        if (native && m_AutoLoadArmed && m_Room->BeginLoad(false)) {
            m_AutoLoadArmed=false;
            m_InitialLoadDeadline=now+90.;
            m_Message="Host loaded Freeform Training. Waiting for each guest's game to load.";
            m_Source.Log(LogLevel::Info,"party-load",m_Message);
        }
        return;
    }
    if (m_InitialLoadDeadline<0.) return;
    if (Status().phase==LobbyRoster::Phase::Loading) {
        if (now<m_InitialLoadDeadline) return;
        if (m_Room->ReportSceneLost()) m_Message="A guest did not load Freeform Training in time. Stop the session to retry.";
    } else if (Status().phase==LobbyRoster::Phase::Playing) {
        m_Message="Every player loaded Freeform Training.";
        m_Source.Log(LogLevel::Info,"party-load",m_Message);
    }
    m_InitialLoadDeadline=-1.;
}
void PartyPreviewRuntime::CancelGuestLaunch() {
    if (!m_LaunchPending) return;
    m_Launcher->CancelFreeformLaunch();
    m_LaunchPending=false;
}
void PartyPreviewRuntime::UpdateGuestLaunch(double now,bool native) {
    if (!m_Launcher) return;
    const auto& status=Status();
    if (status.host || status.state!=PartyRoomStatus::State::Connected ||
        status.phase!=LobbyRoster::Phase::Loading || !status.session || !status.epoch ||
        status.mission!=kPartyFreeformContract || native) {
        CancelGuestLaunch();
        return;
    }
    if (m_LaunchSession!=status.session || m_LaunchEpoch!=status.epoch) {
        CancelGuestLaunch();
        m_LaunchSession=status.session;
        m_LaunchEpoch=status.epoch;
        m_LaunchFinished=false;
    }
    if (m_LaunchFinished) return;
    m_LaunchPending=true;
    const auto result=m_Launcher->PollFreeformLaunch(now);
    if (result==IPartyFreeformLaunch::Result::Waiting) return;
    m_LaunchPending=false;
    m_LaunchFinished=true;
    if (result==IPartyFreeformLaunch::Result::Submitted) {
        m_Message="Following the host into Freeform Training.";
        m_Source.Log(LogLevel::Info,"launch","Submitted guest Freeform launch for epoch "+std::to_string(m_LaunchEpoch)+".");
    } else {
        m_Message="HITMAN could not start Freeform automatically. See HitmanCoop.log.";
        m_Room->ReportSceneLost();
    }
}
void PartyPreviewRuntime::UpdatePartyPause(const PartyRoomStatus& status,bool localMenuPause) {
    const auto epoch=status.phase==LobbyRoster::Phase::Playing ? status.epoch : 0;
    if (epoch!=m_PauseEpoch) {
        m_PauseEpoch=epoch;
        m_PauseVotes={};
        m_NextPauseVote=0;
    }
    bool remote=false;
    if (epoch) {
        for (const auto& member:status.members) {
            if (member.id==status.self || !member.loaded || member.id<1 || member.id>m_PauseVotes.size()) continue;
            const auto& vote=m_PauseVotes[member.id-1];
            remote|=vote.incarnation==member.incarnation && vote.paused;
        }
    }
    const bool local=epoch && localMenuPause;
    if (local!=m_LocalPause || remote!=m_RemotePause)
        m_Source.Log(LogLevel::Info,"party-pause","Menu holds epoch="+std::to_string(epoch)+
            " local="+std::to_string(local)+" remote="+std::to_string(remote));
    m_LocalPause=local;
    m_RemotePause=remote;
    m_PartyPaused=m_LocalPause || m_RemotePause;
    m_Sink.SetPartyPause(m_LocalPause,m_RemotePause);
}

bool PartyPreviewRuntime::ReceivePartyPause(const PartyGameEvent& event) {
    const auto& status=m_Room->Status();
    if (!m_PauseEpoch || event.epoch!=m_PauseEpoch || status.phase!=LobbyRoster::Phase::Playing ||
        event.source==status.self || event.source<1 || event.source>m_PauseVotes.size()) return false;
    const auto member=std::find_if(status.members.begin(),status.members.end(),[&](const PartyMember& value) {
        return value.id==event.source && value.incarnation==event.incarnation && value.loaded;
    });
    if (member==status.members.end()) return false;
    ByteReader reader(event.payload);
    uint8_t kind=0;
    uint32_t sequence=0;
    bool paused=false;
    if (!reader.U8(kind) || kind!=uint8_t(MsgType::Pause) || !reader.U32(sequence) ||
        !reader.Bool(paused) || !reader.AtEnd()) return false;
    auto& vote=m_PauseVotes[event.source-1];
    if (vote.incarnation==event.incarnation && static_cast<int32_t>(sequence-vote.sequence)<=0) return false;
    vote={event.incarnation,sequence,paused};
    return true;
}

void PartyPreviewRuntime::Tick(double now,bool localMenuPause) {
    if (!std::isfinite(now)) return;
    m_Room->Tick(now);
    if (m_PartyPaused && m_PauseTick>=0) m_PauseElapsed+=std::max(0.,now-m_PauseTick);
    m_PauseTick=now;
    const double npcNow=now-m_PauseElapsed;
    const bool previousLocalPause=m_LocalPause;
    UpdatePartyPause(Status(),localMenuPause);
    if (m_PauseEpoch && (m_LocalPause!=previousLocalPause || now>=m_NextPauseVote)) {
        m_NextPauseVote=now+0.25;
        ByteWriter writer;
        writer.U8(uint8_t(MsgType::Pause)); writer.U32(++m_PauseSequence); writer.Bool(m_LocalPause);
        m_Room->SendGame(PartyChannel::Avatar,writer.Data(),false);
    }
    if (m_NpcWorld && m_NpcRetirementPending && m_NpcRetirement &&
        m_NpcWorld->ConfirmNpcSceneRetired(m_NpcRetirement)) {
        m_NpcRetirementPending=false;
        m_NpcStopFault=false;
        m_NpcCatalogSpent=false;
    }
    const bool native=NativeReady();
    if (m_HadNativeScene && !native && !(m_FollowHostRestart && Status().phase==LobbyRoster::Phase::Restarting)) {
        StopNpcs();
        m_Room->ReportSceneLost();
        m_Playback.Reset();
    }
    m_HadNativeScene=native;
    auto status=m_Room->Status();
    if (status.host && status.phase==LobbyRoster::Phase::Lobby && !status.epoch) {
        m_Room->SelectMission(kPartyFreeformContract);
        status=m_Room->Status();
    }
    if (status.epoch!=m_Epoch) {
        StopNpcs();
        m_Npcs.reset(); m_NpcCatalog.clear(); m_NextNpcCatalog=0;
        m_NpcScenePrepared=false;
        m_Epoch=status.epoch;
        m_NextSend=m_NextMetadata=m_NextLoaded=0;
    }
    // Availability must not remain true after leaving the native scene.
    if (status.phase==LobbyRoster::Phase::Lobby && !native && LocalReady())
        m_Room->SetReady(false,false);
    UpdateInitialLoad(now,native);
    UpdateGuestLaunch(now,native);
    UpdateRestart(now,m_PrepareAgreedNpcCatalog && status.phase==LobbyRoster::Phase::Restarting ? NativeSceneAvailable() : native);
    status=m_Room->Status();
    const bool restarted=status.phase==LobbyRoster::Phase::Restarting && !m_RestartFailed && m_RestartCleared==status.epoch;
    if ((status.phase==LobbyRoster::Phase::Loading || restarted) && native && now>=m_NextLoaded) {
        m_NextLoaded=now+0.5;
        bool loaded=false;
        for (const auto& member : status.members) if (member.id==status.self) loaded=member.loaded;
        if (!loaded) {
            if (m_NpcWorld && !m_PrepareAgreedNpcCatalog && !m_NpcScenePrepared) PrepareNpcs();
            if (!m_NpcWorld || m_PrepareAgreedNpcCatalog || m_NpcScenePrepared) m_Room->ReportLoaded();
        }
        status=m_Room->Status();
    }
    const bool presenting=m_Playback.UpdateRoster(status);
    const bool npcs=UpdateNpcs(npcNow,presenting);
    const bool observeLocations=npcs && status.host &&
        (m_NpcOptions.policy==PartyNpcPolicy::NearestPlayer || m_NpcOptions.nearbyNpcPoses || m_NpcOptions.automaticGuardsPerGuest);
    if (observeLocations) {
        Transform local;
        if (m_Source.GetLocalAvatar(local)) {
            for (const auto& member:status.members)
                if (member.id==status.self) m_Npcs->ObservePlayer({member.id,member.incarnation},local.pos,npcNow);
        }
    }
    PartyGameEvent event;
    while (m_Room->PopGame(event)) {
        if (event.channel==PartyChannel::Avatar && !event.payload.empty() && event.payload.front()==uint8_t(MsgType::Pause)) {
            ReceivePartyPause(event);
            continue;
        }
        const bool npcChannel=event.channel==PartyChannel::HostWorld || event.channel==PartyChannel::Interaction;
        std::optional<Vec3> acceptedPosition;
        const bool rosterPacket=m_NpcRoster && npcChannel && !event.payload.empty() && event.payload.front()==PartyNpcRoster::Packet;
        const bool accepted=native && (rosterPacket ? m_NpcRoster->Receive(event) :
            presenting && (npcChannel ? npcs && m_Npcs->Receive(event,npcNow) :
                m_Playback.Receive(event,now,observeLocations?&acceptedPosition:nullptr)));
        if (acceptedPosition) m_Npcs->ObservePlayer({event.source,event.incarnation},*acceptedPosition,npcNow);
        if (accepted) ++m_Accepted;
        else ++m_Rejected;
    }
    UpdatePartyPause(Status(),localMenuPause);
    if (m_NpcRoster) {
        m_NpcRoster->Tick();
        const auto& shared=m_NpcRoster->Shared();
        if (shared!=m_LastSharedNpcIds) {
            if (shared) m_Source.Log(LogLevel::Info,"npc-roster",
                "Room agreed on "+std::to_string(shared->size())+" shared NPC IDs in epoch "+std::to_string(Status().epoch)+".");
            m_LastSharedNpcIds=shared;
        }
    }
    if (npcs && m_Npcs) {
        m_Npcs->Tick(npcNow,!m_PartyPaused);
        if (!m_Npcs->Active()) InterruptNpcs("Shared NPC synchronization stopped.");
    }
    status=m_Room->Status();
    if (native && !m_NpcStopFault && !m_NpcCatalogSpent && status.phase==LobbyRoster::Phase::Playing && presenting) {
        // Reliable refresh also supplies a resumed member with current metadata.
        if (now>=m_NextMetadata) {
            m_NextMetadata=now+1.;
            MsgOutfit outfit;
            if (m_Source.GetLocalOutfit(outfit.outfit))
                m_Room->SendGame(PartyChannel::Avatar,PartyAvatarPacket(outfit),true);
            MsgSkeleton skeleton;
            if (m_Source.GetLocalSkeleton(skeleton.parents))
                m_Room->SendGame(PartyChannel::Avatar,PartyAvatarPacket(skeleton),true);
        }
        if (now>=m_NextSend) {
            m_NextSend=now+1./30.;
            const uint32_t ms=static_cast<uint32_t>(static_cast<uint64_t>(now*1000.));
            MsgAvatarState avatar; avatar.epoch=status.epoch; avatar.timeMs=ms;
            if (m_Source.GetLocalAvatar(avatar.xf)) {
                avatar.equipment=m_Sink.LocalPartyEquipment();
                m_Room->SendGame(PartyChannel::Avatar,PartyAvatarPacket(avatar),false);
            }
            MsgPose pose; pose.epoch=status.epoch; pose.timeMs=ms;
            if (m_Source.GetLocalPose(pose.pose))
                m_Room->SendGame(PartyChannel::Pose,PartyAvatarPacket(pose),false);
        }
        m_Playback.Tick(now);
    }
}
}
