#include "TestFramework.h"
#include "SimNetwork.h"
#include "coop/PartyRoom.h"
#include "coop/PartyPresentationRoster.h"
#include <iomanip>
#include <set>
#include <sstream>

using namespace coop;
namespace {
const Endpoint hostEp{0x0a000001,47047};
const Endpoint endpoints[]{{0x0a000002,50001},{0x0a000002,50002},{0x0a000003,50003}};
PartyRoomConfig Config(std::string name) { PartyRoomConfig config; config.name=std::move(name); return config; }
struct Room {
    std::shared_ptr<SimNetwork> net=SimNetwork::Create(813);
    std::unique_ptr<PartyRoom> host=PartyRoom::Host(net->Open(hostEp),Config("Host"));
    std::array<std::unique_ptr<PartyRoom>,3> clients;
    double now=0;
    explicit Room(bool connect=true) {
        if(connect) for(size_t i=0;i<3;++i) clients[i]=PartyRoom::Join(net->Open(endpoints[i]),hostEp,Config("Guest"+std::to_string(i)),now);
    }
    void Step(double seconds) {
        const double end=now+seconds;
        while(now<end) { now+=1.0/60; net->SetTime(now); host->Tick(now); for(auto& client:clients) if(client) client->Tick(now); }
    }
    bool Connected() const {
        if(host->Status().members.size()!=4) return false;
        for(const auto& client:clients) if(!client || client->Status().state!=PartyRoomStatus::State::Connected || client->Status().members.size()!=4) return false;
        return true;
    }
    bool Playing() {
        if(!host->SelectMission(kPartyFreeformContract)) return false;
        Step(0.5);
        if(!host->SetReady(true,true)) return false;
        for(auto& client:clients) if(!client->SetReady(true,true)) return false;
        Step(0.5);
        if(!host->BeginLoad()) return false;
        Step(0.5);
        if(!host->ReportLoaded()) return false;
        for(auto& client:clients) if(!client->ReportLoaded()) return false;
        Step(0.5);
        return host->Status().phase==LobbyRoster::Phase::Playing;
    }
};
uint64_t SelfKey(const PartyRoom& room) {
    for(const auto& member:room.Status().members) if(member.id==room.Status().self) return member.incarnation;
    return 0;
}
class TapSocket:public IDatagramSocket {
    std::unique_ptr<IDatagramSocket> socket;
    std::shared_ptr<std::vector<std::vector<uint8_t>>> capture;
public:
    TapSocket(std::unique_ptr<IDatagramSocket> socket,std::shared_ptr<std::vector<std::vector<uint8_t>>> capture):socket(std::move(socket)),capture(std::move(capture)){}
    bool SendTo(const Endpoint& endpoint,const uint8_t* bytes,size_t size) override {
        capture->emplace_back(bytes,bytes+size); return socket->SendTo(endpoint,bytes,size);
    }
    int RecvFrom(Endpoint& endpoint,uint8_t* bytes,size_t size) override { return socket->RecvFrom(endpoint,bytes,size); }
};
}
TEST(PartyRoom_HmacMatchesIndependentSha256Vector) {
    std::array<uint8_t,32> key{}; key[0]='k';key[1]='e';key[2]='y';
    const std::string text="The quick brown fox jumps over the lazy dog";
    const auto digest=partycrypto::Hmac(key,{text.begin(),text.end()});
    std::ostringstream hex; for(uint8_t byte:digest) hex<<std::hex<<std::setw(2)<<std::setfill('0')<<unsigned(byte);
    CHECK(hex.str()=="f7bc83f430538424b13298e6aa6fb143ef4d59a14946175997479dbc2d1a3cd8");
}
TEST(PartyRoom_ThreeAutomaticJoinsConvergeOnFourMemberRoster) {
    Room room; room.Step(3); REQUIRE(room.Connected());
    std::set<uint8_t> ids;
    for(const auto& client:room.clients) {
        ids.insert(client->Status().self);
        CHECK(client->Status().session==room.host->Status().session);
        CHECK(client->Status().revision==room.host->Status().revision);
        CHECK(SelfKey(*client)!=0);
    }
    CHECK(ids==std::set<uint8_t>({2,3,4}));
    room.Step(18); CHECK(room.Connected()); // idle rooms keep their links alive
}
TEST(PartyRoom_RejectsMismatchedBuildAndFullParty) {
    Room room; room.Step(3); REQUIRE(room.Connected());
    auto fifth=PartyRoom::Join(room.net->Open({0x0a000004,50004}),hostEp,Config("Fifth"),room.now);
    auto config=Config("Mismatch"); config.build="wrong-build";
    auto mismatch=PartyRoom::Join(room.net->Open({0x0a000005,50005}),hostEp,config,room.now);
    for(double end=room.now+2;room.now<end;) { room.Step(1.0/60); fifth->Tick(room.now); mismatch->Tick(room.now); }
    CHECK(fifth->Status().state==PartyRoomStatus::State::Rejected);
    CHECK(fifth->Status().message=="Party is full");
    CHECK(mismatch->Status().state==PartyRoomStatus::State::Rejected);
    CHECK(mismatch->Status().message=="Game or mod build mismatch"); CHECK(room.Connected());
}
TEST(PartyRoom_JoinAndRosterSurviveLossDuplicatesAndReordering) {
    Room room; room.net->config.loss=0.2; room.net->config.duplicate=0.3; room.net->config.maxLatency=0.1;
    room.Step(8); CHECK(room.Connected());
    std::set<uint8_t> ids;
    for(const auto& client:room.clients) ids.insert(client->Status().self);
    CHECK(ids.size()==3);
}
TEST(PartyRoom_ReturnAddressProofCannotBeReplayedFromAnotherEndpoint) {
    Room room(false);
    auto capture=std::make_shared<std::vector<std::vector<uint8_t>>>();
    room.clients[0]=PartyRoom::Join(std::make_unique<TapSocket>(room.net->Open(endpoints[0]),capture),hostEp,Config("Guest"),0);
    room.Step(2); REQUIRE(room.host->Status().members.size()==2);
    auto attacker=room.net->Open({0x0a000099,55000}); bool replayed=false;
    for(const auto& packet:*capture) if(packet.size()>=80 && packet[0]=='H' && packet[1]=='4' && packet[2]=='J' && packet[4]==3) {
        attacker->SendTo(hostEp,packet.data(),packet.size()); replayed=true;
    }
    REQUIRE(replayed); room.Step(1); CHECK(room.host->Status().members.size()==2);
    CHECK(room.clients[0]->Status().state==PartyRoomStatus::State::Connected);
}
TEST(PartyRoom_ReconnectOnNewEndpointPreservesSlotAndChangesIncarnation) {
    Room room; room.Step(3); REQUIRE(room.Connected());
    size_t high=0,low=0;
    for(size_t i=0;i<3;++i) { if(room.clients[i]->Status().self==4) high=i; if(room.clients[i]->Status().self==2) low=i; }
    const auto previous=SelfKey(*room.clients[high]);
    room.clients[low]->Leave(room.now); room.Step(1);
    CHECK(room.clients[low]->Status().state==PartyRoomStatus::State::Closed);
    REQUIRE(room.clients[high]->Reconnect(room.net->Open({0x0a000006,56000}),room.now));
    room.Step(3);
    CHECK(room.clients[high]->Status().state==PartyRoomStatus::State::Connected);
    CHECK(room.clients[high]->Status().self==4); CHECK(SelfKey(*room.clients[high])!=previous);
    CHECK(room.host->Status().members.size()==3);
}
TEST(PartyRoom_AllReadyAllLoadedAndAllCompleteRequiredForReplayLoop) {
    Room room; room.Step(3); REQUIRE(room.Connected());
    CHECK(!room.clients[0]->SelectMission(kPartyFreeformContract));
    CHECK(!room.host->SelectMission("unverified-contract"));
    REQUIRE(room.host->SelectMission(kPartyFreeformContract)); room.Step(0.4);
    REQUIRE(room.host->SetReady(true,true));
    for(size_t i=0;i<2;++i) REQUIRE(room.clients[i]->SetReady(true,true)); room.Step(0.4);
    CHECK(!room.host->BeginLoad()); REQUIRE(room.clients[2]->SetReady(false,true)); room.Step(0.4);
    CHECK(!room.host->BeginLoad()); REQUIRE(room.clients[2]->SetReady(true,true)); room.Step(0.4);
    REQUIRE(room.host->BeginLoad()); room.Step(0.4);
    REQUIRE(room.host->ReportLoaded()); REQUIRE(room.clients[0]->ReportLoaded()); REQUIRE(room.clients[1]->ReportLoaded()); room.Step(0.4);
    CHECK(room.host->Status().phase==LobbyRoster::Phase::Loading);
    REQUIRE(room.clients[2]->ReportLoaded()); room.Step(0.4);
    CHECK(room.host->Status().phase==LobbyRoster::Phase::Playing);
    REQUIRE(room.host->ReportNativeCompletion()); REQUIRE(room.clients[0]->ReportNativeCompletion()); REQUIRE(room.clients[1]->ReportNativeCompletion()); room.Step(0.4);
    CHECK(!room.host->ReturnToLobby()); REQUIRE(room.clients[2]->ReportNativeCompletion()); room.Step(0.4);
    REQUIRE(room.host->ReturnToLobby()); const auto old=room.host->Status().epoch;
    REQUIRE(room.host->SelectMission(kPartyFreeformContract)); room.Step(0.4);
    CHECK(room.host->Status().epoch==old+1); CHECK(!room.host->BeginLoad());
    for(const auto& client:room.clients) { CHECK(client->Status().phase==LobbyRoster::Phase::Lobby); CHECK(client->Status().members.size()==4); }
}
TEST(PartyRoom_InteractionsRequireHostApprovalAndStaleEventsDoNotLeakIntoReplay) {
    Room room; room.Step(3); REQUIRE(room.Connected()); REQUIRE(room.Playing());
    REQUIRE(room.clients[0]->SendGame(PartyChannel::Interaction,{'k'},true));
    CHECK(!room.clients[0]->SendGame(PartyChannel::HostWorld,{'w'},true)); room.Step(0.5);
    PartyGameEvent event; REQUIRE(room.host->PopGame(event)); CHECK(event.payload==std::vector<uint8_t>({'k'}));
    PartyGameEvent other; CHECK(!room.clients[1]->PopGame(other)); CHECK(!room.clients[2]->PopGame(other));
    auto stale=event; --stale.epoch; CHECK(!room.host->ApproveInteraction(stale));
    REQUIRE(room.host->ApproveInteraction(event)); room.Step(0.5);
    REQUIRE(room.clients[1]->PopGame(other)); CHECK(other.source==room.clients[0]->Status().self);
    REQUIRE(room.clients[2]->PopGame(other)); CHECK(other.incarnation==event.incarnation);
    REQUIRE(room.clients[0]->SendGame(PartyChannel::Avatar,{'a'},false)); room.Step(0.4);
    REQUIRE(room.host->ReportNativeCompletion()); for(auto& client:room.clients) REQUIRE(client->ReportNativeCompletion()); room.Step(0.5);
    REQUIRE(room.host->ReturnToLobby()); REQUIRE(room.host->SelectMission(kPartyFreeformContract)); room.Step(0.4);
    CHECK(!room.host->ApproveInteraction(event)); CHECK(!room.host->PopGame(other));
    for(auto& client:room.clients) CHECK(!client->PopGame(other));
}
TEST(PartyRoom_DisconnectInterruptsAnActiveMissionWithoutFalseVictory) {
    Room room; room.Step(3); REQUIRE(room.Connected()); REQUIRE(room.Playing());
    room.clients[0]->Leave(room.now); room.Step(1);
    CHECK(room.host->Status().phase==LobbyRoster::Phase::Interrupted);
    CHECK(room.host->Status().members.size()==3);
    CHECK(room.clients[1]->Status().phase==LobbyRoster::Phase::Interrupted);
    CHECK(!room.host->ReportNativeCompletion());
    REQUIRE(room.host->ReturnToLobby()); room.Step(0.5);
    CHECK(room.clients[1]->Status().members.size()==3); CHECK(room.clients[2]->Status().members.size()==3);
}
TEST(PartyRoom_AutomaticJoinOverFourRealUdpSockets) {
    std::string error; auto socket=UdpSocket::Open(0,error); REQUIRE(socket);
    Endpoint address{0x7f000001,socket->LocalPort()};
    auto host=PartyRoom::Host(std::move(socket),Config("Host"));
    std::array<std::unique_ptr<PartyRoom>,3> clients;
    for(size_t i=0;i<3;++i) { auto guest=UdpSocket::Open(0,error); REQUIRE(guest); clients[i]=PartyRoom::Join(std::move(guest),address,Config("Guest"+std::to_string(i)),0); }
    for(double now=0;now<5;now+=1.0/120) { host->Tick(now); for(auto& client:clients) client->Tick(now); }
    CHECK(host->Status().members.size()==4);
    for(const auto& client:clients) { CHECK(client->Status().state==PartyRoomStatus::State::Connected); CHECK(client->Status().members.size()==4); }
}
TEST(PartyRoom_PendingGuestPreventsStartingBehindTheirBack) {
    Room room(false);
    room.clients[0]=PartyRoom::Join(room.net->Open(endpoints[0]),hostEp,Config("First"),0);
    room.Step(2); REQUIRE(room.host->Status().members.size()==2);
    REQUIRE(room.host->SelectMission(kPartyFreeformContract)); room.Step(0.4);
    REQUIRE(room.host->SetReady(true,true)); REQUIRE(room.clients[0]->SetReady(true,true)); room.Step(0.4);
    room.clients[1]=PartyRoom::Join(room.net->Open(endpoints[1]),hostEp,Config("Joining"),room.now);
    const double deadline=room.now+2;
    while(!room.clients[1]->Status().self && room.now<deadline) room.Step(1.0/60);
    REQUIRE(room.clients[1]->Status().self!=0);
    CHECK(room.host->Status().members.size()==2); // welcome received, Joined ACK still in flight
    CHECK(!room.host->BeginLoad());
    room.Step(0.5); REQUIRE(room.host->Status().members.size()==3);
    REQUIRE(room.clients[1]->SetReady(true,true)); room.Step(0.4); CHECK(room.host->BeginLoad());
}
TEST(PartyRoom_NewGuestsCannotJoinAnAlreadyRunningMission) {
    Room room; room.Step(3); REQUIRE(room.Connected()); REQUIRE(room.Playing());
    auto late=PartyRoom::Join(room.net->Open({0x0a000009,59000}),hostEp,Config("Late"),room.now);
    for(double end=room.now+2;room.now<end;) { room.Step(1.0/60); late->Tick(room.now); }
    CHECK(late->Status().state==PartyRoomStatus::State::Rejected);
    CHECK(room.host->Status().phase==LobbyRoster::Phase::Playing); CHECK(room.Connected());
}
TEST(PartyRoom_AbsentHostTimesOutAndFreshNonceRetryCanJoin) {
    auto net=SimNetwork::Create(81);
    auto client=PartyRoom::Join(net->Open(endpoints[0]),hostEp,Config("Guest"),0);
    double now=0;
    for(;now<11;now+=1.0/60) { net->SetTime(now); client->Tick(now); }
    CHECK(client->Status().state==PartyRoomStatus::State::Failed);
    auto host=PartyRoom::Host(net->Open(hostEp),Config("Host")); REQUIRE(client->Reconnect(now));
    for(double end=now+3;now<end;now+=1.0/60) { net->SetTime(now); host->Tick(now); client->Tick(now); }
    CHECK(client->Status().state==PartyRoomStatus::State::Connected); CHECK(host->Status().members.size()==2);
    host->Leave(now); CHECK(host->Status().state==PartyRoomStatus::State::Closed);
    CHECK(!host->SelectMission(kPartyFreeformContract)); CHECK(!host->SetReady(true,true));
}

TEST(PartyRoom_PresentationScopesAcceptRealRoomRostersAndStopOnDisconnect) {
    Room room;
    room.Step(2.);
    REQUIRE(room.Connected());
    REQUIRE(room.Playing());
    std::array<PartyPresentationRoster, 4> views;
    std::array<PartyRoom*, 4> peers {room.host.get(), room.clients[0].get(), room.clients[1].get(), room.clients[2].get()};
    for (size_t i = 0; i < peers.size(); ++i) {
        REQUIRE(views[i].Update(peers[i]->Status()));
        size_t count = 0;
        for (const auto& member : peers[i]->Status().members)
            count += views[i].Remote(member.id, member.incarnation, peers[i]->Status().epoch) != nullptr;
        CHECK(count == 3);
    }
    const auto source = room.clients[2]->Status().self;
    const auto incarnation = SelfKey(*room.clients[2]);
    REQUIRE(room.clients[2]->SendGame(PartyChannel::Avatar, {9, 8, 7}, false));
    room.Step(0.5);
    for (size_t i = 0; i < peers.size(); ++i) {
        PartyGameEvent event;
        bool received = false;
        while (peers[i]->PopGame(event)) {
            REQUIRE(views[i].Update(peers[i]->Status()));
            CHECK(views[i].Remote(event.source, event.incarnation, event.epoch) != nullptr);
            received = true;
        }
        CHECK(received == (peers[i] != room.clients[2].get()));
    }
    room.clients[2]->Leave(room.now);
    room.Step(1.);
    for (size_t i = 0; i < 3; ++i) {
        REQUIRE(views[i].Update(peers[i]->Status()));
        CHECK(peers[i]->Status().phase == LobbyRoster::Phase::Interrupted);
        CHECK(!views[i].Remote(source, incarnation, peers[i]->Status().epoch));
        for (const auto& member : peers[i]->Status().members)
            CHECK(!views[i].Remote(member.id, member.incarnation, peers[i]->Status().epoch));
    }
}
