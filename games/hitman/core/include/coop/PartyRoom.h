#pragma once
#include "LobbyRoster.h"
#include "PartyTransport.h"
#include <array>

namespace coop {
inline constexpr const char* kPartyFreeformContract = "b573932d-7a34-44f1-bcf4-ea8f79f75710";
struct PartyRoomConfig {
    uint32_t protocol=2;
    std::string build="party-prototype-2", game="3.290.1", name="Agent";
};
struct PartyMember {
    uint8_t id=0;
    uint64_t incarnation=0;
    std::string name;
    bool available=false,ready=false,loaded=false,complete=false;
};
struct PartyRoomStatus {
    enum class State { Hosting, Connecting, Connected, Leaving, Closed, Rejected, Failed } state=State::Connecting;
    bool host=false;
    uint8_t self=0;
    uint64_t session=0;
    uint32_t revision=0, epoch=0;
    LobbyRoster::Phase phase=LobbyRoster::Phase::Lobby;
    std::string mission, message;
    std::vector<PartyMember> members;
};
struct PartyGameEvent {
    uint8_t source=0;
    uint64_t incarnation=0;
    uint32_t epoch=0;
    PartyChannel channel=PartyChannel::Avatar;
    std::vector<uint8_t> payload;
    bool reliable=false;
};
// Development-only room controller over the four-player transport. Does not
// invoke HITMAN: native adapters must load/complete missions and validate actions.
class PartyRoom {
public:
    static std::unique_ptr<PartyRoom> Host(std::unique_ptr<IDatagramSocket> socket, PartyRoomConfig config);
    static std::unique_ptr<PartyRoom> Join(std::unique_ptr<IDatagramSocket> socket, Endpoint host, PartyRoomConfig config, double now);
    ~PartyRoom();
    void Tick(double now);
    const PartyRoomStatus& Status() const;
    bool Reconnect(double now);
    bool Reconnect(std::unique_ptr<IDatagramSocket> socket,double now);
    void Leave(double now);
    bool SelectMission(const std::string& contract);
    bool SetReady(bool available, bool ready);
    bool BeginLoad(bool requireReady = true);
    bool BeginRestart();
    bool ReportLoaded();
    bool ReportNativeCompletion();
    bool ReportSceneLost();
    bool ReturnToLobby();
    bool SendGame(PartyChannel channel,const std::vector<uint8_t>& payload,bool reliable);
    // Client interactions are delivered only to the host. The native authority
    // adapter can approve a validated interaction for replication explicitly.
    bool ApproveInteraction(const PartyGameEvent& event);
    bool PopGame(PartyGameEvent& event);
private:
    struct Impl;
    explicit PartyRoom(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> m_Impl;
};
namespace partycrypto {
std::array<uint8_t,32> Hmac(const std::array<uint8_t,32>& key,const std::vector<uint8_t>& bytes);
}
}
