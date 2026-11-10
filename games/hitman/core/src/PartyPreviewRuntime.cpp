#include "coop/PartyPreviewRuntime.h"
#include "coop/LauncherPolicy.h"
#include <cmath>

namespace coop {
PartyPreviewRuntime::PartyPreviewRuntime(std::unique_ptr<PartyRoom> room, IGameWorld& source, IPartyAvatarWorld& sink)
    : m_Room(std::move(room)), m_Source(source), m_Sink(sink), m_Playback(sink) {}

bool PartyPreviewRuntime::NativeReady() const {
    Transform transform;
    return m_Sink.PartyPreviewError().empty() && m_Source.IsInScene() && IsFreeformTrainingScene(m_Source.CurrentScene().scene) && m_Source.GetLocalAvatar(transform);
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
    if (Status().phase == LobbyRoster::Phase::Playing || Status().phase == LobbyRoster::Phase::Loading || Status().phase == LobbyRoster::Phase::Completing)
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
void PartyPreviewRuntime::Leave(double now) {
    m_Playback.Reset();
    m_Room->Leave(now);
    m_Message="Leaving preview. Reload the mission before creating a new room.";
}
void PartyPreviewRuntime::SceneCleared() {
    m_Room->ReportSceneLost();
    m_Playback.Reset();
    m_HadNativeScene=false;
    m_Epoch=0;
    m_NextSend=m_NextMetadata=m_NextLoaded=0;
}
void PartyPreviewRuntime::Tick(double now) {
    if (!std::isfinite(now)) return;
    m_Room->Tick(now);
    const bool native=NativeReady();
    if (m_HadNativeScene && !native) {
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
        m_Epoch=status.epoch;
        m_NextSend=m_NextMetadata=m_NextLoaded=0;
    }
    // Availability must not remain true after leaving the native scene.
    if (status.phase==LobbyRoster::Phase::Lobby && !native && LocalReady())
        m_Room->SetReady(false,false);
    if (status.phase==LobbyRoster::Phase::Loading && native && now>=m_NextLoaded) {
        m_NextLoaded=now+0.5;
        bool loaded=false;
        for (const auto& member : status.members) if (member.id==status.self) loaded=member.loaded;
        if (!loaded) m_Room->ReportLoaded();
        status=m_Room->Status();
    }
    const bool presenting=m_Playback.UpdateRoster(status);
    PartyGameEvent event;
    while (m_Room->PopGame(event)) {
        if (presenting && native && m_Playback.Receive(event,now)) ++m_Accepted;
        else ++m_Rejected;
    }
    if (native && status.phase==LobbyRoster::Phase::Playing && presenting) {
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
