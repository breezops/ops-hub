#pragma once
#include "PartyAvatarPlayback.h"
#include "Session.h"
#include "PartyNpcSync.h"
#include "PartyNpcRoster.h"
#include <set>

namespace coop {
class IPartyFreeformLaunch {
public:
    enum class Result { Waiting, Submitted, Failed };
    virtual ~IPartyFreeformLaunch() = default;
    virtual bool NativeFreeformContract() const = 0;
    virtual Result PollFreeformLaunch(double now) = 0;
    virtual void CancelFreeformLaunch() = 0;
};

// Movement/disguise by default. NPC development requires an explicit adapter.
class PartyPreviewRuntime {
public:
    PartyPreviewRuntime(std::unique_ptr<PartyRoom> room, IGameWorld& source, IPartyAvatarWorld& sink,
                        IPartyNpcWorld* npcWorld = nullptr, PartyNpcOptions npcOptions = {},
                        bool followHostRestart = false, bool autoInitialLoad = false,
                        IPartyFreeformLaunch* launcher = nullptr, bool exchangeNpcRoster = false,
                        bool prepareAgreedNpcCatalog = false);
    ~PartyPreviewRuntime();
    void Tick(double now);
    bool Ready();
    bool Start();
    bool Lobby();
    bool Reconnect(double now);
    bool HostMissionRestarted();
    void Leave(double now);
    void SceneCleared(); // Existing SDK callback is pre-clear; begins retirement.
    const PartyRoomStatus& Status() const { return m_Room->Status(); }
    bool NativeReady() const;
    bool LocalReady() const;
    std::string Message() const {
        const auto fault = m_Sink.PartyPreviewError();
        if (!fault.empty()) return fault;
        if (m_NpcRetirementPending) return "Waiting for native NPC scene teardown to finish.";
        if (m_NpcCatalogSpent) return m_Message + " Reload Freeform Training before starting another session.";
        return m_NpcStopFault ? "NPC simulation did not confirm it stopped. Reload Freeform Training on this PC." : m_Message;
    }
    bool SharedNpcsActive() const { return m_Npcs && m_Npcs->Active(); }
    bool RequestNpcHandoff(uint64_t npc, PartyIdentity next, double now) {
        return m_NpcOptions.policy==PartyNpcPolicy::Explicit && m_Npcs && m_Npcs->Propose(npc,next,now);
    }
    uint8_t ConfirmedNpcOwner(uint64_t npc) const {
        const auto* authority=m_Npcs && m_Npcs->Active() ? m_Npcs->Authority() : nullptr;
        const auto* record=authority ? authority->Find(npc) : nullptr;
        return record && record->confirmed && record->phase==PartyNpcAuthority::Phase::Active ? record->owner.id : 0;
    }
    void ObserveNpcRoster(std::span<const uint64_t> ids);
    const PartyNpcRoster* NpcRoster() const { return m_NpcRoster.get(); }
    uint64_t Accepted() const { return m_Accepted; }
    uint64_t Rejected() const { return m_Rejected; }
private:
    std::unique_ptr<PartyRoom> m_Room;
    IGameWorld& m_Source;
    IPartyAvatarWorld& m_Sink;
    PartyAvatarPlayback m_Playback;
    IPartyNpcWorld* m_NpcWorld = nullptr;
    PartyNpcOptions m_NpcOptions;
    std::unique_ptr<PartyNpcSync> m_Npcs;
    std::unique_ptr<PartyNpcRoster> m_NpcRoster;
    bool m_PrepareAgreedNpcCatalog = false;
    std::optional<std::vector<uint64_t>> m_LastSharedNpcIds;
    std::set<uint64_t> m_NpcCatalog;
    double m_NextNpcCatalog = 0;
    bool m_NpcStopFault = false;
    bool m_NpcRetirementPending = false;
    uint64_t m_NpcRetirement = 0;
    bool m_NpcScenePrepared = false;
    bool m_NpcCatalogStarted = false, m_NpcCatalogSpent = false;
    void StopNpcs();
    bool InterruptNpcs(const char* reason);
    void PrepareNpcs();
    bool PrepareAgreedNpcs();
    bool NativeSceneAvailable() const;
    bool RegisterFixedCatalog(double now);
    bool UpdateNpcs(double now, bool presenting);
    uint32_t m_Epoch = 0;
    double m_NextSend = 0, m_NextMetadata = 0, m_NextLoaded = 0;
    std::string m_Message;
    uint64_t m_Accepted = 0, m_Rejected = 0;
    bool m_HadNativeScene = false;
    bool m_AutoInitialLoad = false, m_AutoLoadArmed = true;
    double m_InitialLoadDeadline = -1.;
    void UpdateInitialLoad(double now, bool native);
    IPartyFreeformLaunch* m_Launcher=nullptr;
    uint64_t m_LaunchSession=0;
    uint32_t m_LaunchEpoch=0;
    bool m_LaunchPending=false, m_LaunchFinished=false;
    void CancelGuestLaunch();
    void UpdateGuestLaunch(double now,bool native);
    bool m_FollowHostRestart=false, m_RestartPending=false, m_RestartFailed=false;
    uint32_t m_RestartRequested=0, m_RestartCleared=0;
    double m_RestartSince=-1.;
    void CancelRestart();
    void UpdateRestart(double now,bool native);
};
}
