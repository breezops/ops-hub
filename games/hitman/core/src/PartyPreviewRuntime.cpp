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
                                       IPartyNpcWorld* npcWorld, PartyNpcOptions npcOptions, bool followHostRestart)
    : m_Room(std::move(room)), m_Source(source), m_Sink(sink), m_Playback(sink),
      m_NpcWorld(npcWorld), m_NpcOptions(npcOptions), m_FollowHostRestart(followHostRestart && !npcWorld) {}

PartyPreviewRuntime::~PartyPreviewRuntime() {
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

bool PartyPreviewRuntime::UpdateNpcs(double now, bool presenting) {
    if (!m_NpcWorld) return false;
    if (m_NpcCatalogStarted && Status().phase==LobbyRoster::Phase::Loading && presenting && NativeReady()) return false;
    if (!presenting || !NativeReady() || Status().phase!=LobbyRoster::Phase::Playing) {
        if (m_NpcCatalogStarted) return InterruptNpcs("Shared NPC session stopped.");
        StopNpcs(); return false;
    }
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

bool PartyPreviewRuntime::NativeReady() const {
    Transform transform;
    return !m_NpcStopFault && !m_NpcCatalogSpent && !m_NpcRetirementPending && m_Sink.PartyPreviewError().empty() && m_Source.IsInScene() && IsFreeformTrainingScene(m_Source.CurrentScene().scene) && m_Source.GetLocalAvatar(transform);
}
bool PartyPreviewRuntime::LocalReady() const {
    for (const auto& member : Status().members)
        if (member.id == Status().self)
            return member.ready;
    return false;
}
bool PartyPreviewRuntime::Ready() {
    if (!NativeReady()) { m_Message="Load Freeform Training first, then click Ready."; return false; }
    const bool ok = m_Room->SetReady(true, !LocalReady());
    m_Message = ok ? "Ready state sent." : "Ready is available in the preview lobby.";
    return ok;
}
bool PartyPreviewRuntime::Start() {
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
    if (ok) m_Playback.Reset();
    m_Message = ok ? "Preview stopped. Everyone must ready again." : "There is no running or interrupted preview to stop.";
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
    }
}
void PartyPreviewRuntime::Leave(double now) {
    CancelRestart();
    StopNpcs();
    m_Playback.Reset();
    m_Room->Leave(now);
    m_Message="Leaving preview. Reload the mission before creating a new room.";
}
void PartyPreviewRuntime::SceneCleared() {
    const bool restarting=m_FollowHostRestart && Status().phase==LobbyRoster::Phase::Restarting;
    if (restarting) m_RestartCleared=Status().epoch;
    CancelRestart();
    const bool retire=m_NpcWorld && (m_NpcOptions.fixedCatalog.empty() || m_NpcCatalogStarted || m_NpcCatalogSpent);
    StopNpcs();
    m_Npcs.reset(); m_NpcCatalog.clear(); m_NextNpcCatalog=0;
    if (retire) {
        m_NpcRetirementPending=true;
        m_NpcRetirement=NextRetirement();
        if (m_NpcRetirement) m_NpcWorld->BeginNpcSceneRetirement(m_NpcRetirement);
    }
    m_NpcScenePrepared=false;
    if (!restarting) m_Room->ReportSceneLost();
    m_Playback.Reset();
    m_HadNativeScene=false;
    m_Epoch=0;
    m_NextSend=m_NextMetadata=m_NextLoaded=0;
}
void PartyPreviewRuntime::Tick(double now) {
    if (!std::isfinite(now)) return;
    m_Room->Tick(now);
    if (m_NpcWorld && m_NpcRetirementPending && m_NpcRetirement &&
        m_NpcWorld->ConfirmNpcSceneRetired(m_NpcRetirement)) {
        m_NpcRetirementPending=false;
        m_NpcStopFault=false;
        m_NpcCatalogSpent=false;
    }
    const bool native=NativeReady();
    if (m_HadNativeScene && !native) {
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
    UpdateRestart(now,native);
    status=m_Room->Status();
    const bool restarted=status.phase==LobbyRoster::Phase::Restarting && !m_RestartFailed && m_RestartCleared==status.epoch;
    if ((status.phase==LobbyRoster::Phase::Loading || restarted) && native && now>=m_NextLoaded) {
        m_NextLoaded=now+0.5;
        bool loaded=false;
        for (const auto& member : status.members) if (member.id==status.self) loaded=member.loaded;
        if (!loaded) {
            if (m_NpcWorld && !m_NpcScenePrepared) PrepareNpcs();
            if (!m_NpcWorld || m_NpcScenePrepared) m_Room->ReportLoaded();
        }
        status=m_Room->Status();
    }
    const bool presenting=m_Playback.UpdateRoster(status);
    const bool npcs=UpdateNpcs(now,presenting);
    const bool observeLocations=npcs && status.host && m_NpcOptions.policy==PartyNpcPolicy::NearestPlayer;
    if (observeLocations) {
        Transform local;
        if (m_Source.GetLocalAvatar(local)) {
            for (const auto& member:status.members)
                if (member.id==status.self) m_Npcs->ObservePlayer({member.id,member.incarnation},local.pos,now);
        }
    }
    PartyGameEvent event;
    while (m_Room->PopGame(event)) {
        const bool npcChannel=event.channel==PartyChannel::HostWorld || event.channel==PartyChannel::Interaction;
        std::optional<Vec3> acceptedPosition;
        const bool accepted=presenting && native && (npcChannel ?
            (npcs && m_Npcs->Receive(event,now)) : m_Playback.Receive(event,now,observeLocations?&acceptedPosition:nullptr));
        if (acceptedPosition) m_Npcs->ObservePlayer({event.source,event.incarnation},*acceptedPosition,now);
        if (accepted) ++m_Accepted;
        else ++m_Rejected;
    }
    if (npcs && m_Npcs) {
        m_Npcs->Tick(now);
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
            if (m_Source.GetLocalAvatar(avatar.xf))
                m_Room->SendGame(PartyChannel::Avatar,PartyAvatarPacket(avatar),false);
            MsgPose pose; pose.epoch=status.epoch; pose.timeMs=ms;
            if (m_Source.GetLocalPose(pose.pose))
                m_Room->SendGame(PartyChannel::Pose,PartyAvatarPacket(pose),false);
        }
        m_Playback.Tick(now);
    }
}
}
