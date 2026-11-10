#pragma once
#include "PartyAvatarPlayback.h"
#include "Session.h"

namespace coop {
// Movement/disguise preview only. Owns no NPC, item or mission-completion sync.
class PartyPreviewRuntime {
public:
    PartyPreviewRuntime(std::unique_ptr<PartyRoom> room, IGameWorld& source, IPartyAvatarWorld& sink);
    void Tick(double now);
    bool Ready();
    bool Start();
    bool Lobby();
    bool Reconnect(double now);
    void Leave(double now);
    void SceneCleared();
    const PartyRoomStatus& Status() const { return m_Room->Status(); }
    bool NativeReady() const;
    bool LocalReady() const;
    std::string Message() const { const auto fault = m_Sink.PartyPreviewError(); return fault.empty() ? m_Message : fault; }
    uint64_t Accepted() const { return m_Accepted; }
    uint64_t Rejected() const { return m_Rejected; }
private:
    std::unique_ptr<PartyRoom> m_Room;
    IGameWorld& m_Source;
    IPartyAvatarWorld& m_Sink;
    PartyAvatarPlayback m_Playback;
    uint32_t m_Epoch = 0;
    double m_NextSend = 0, m_NextMetadata = 0, m_NextLoaded = 0;
    std::string m_Message;
    uint64_t m_Accepted = 0, m_Rejected = 0;
    bool m_HadNativeScene = false;
};
}
