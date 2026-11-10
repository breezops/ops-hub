#include "TestFramework.h"
#include "SimNetwork.h"
#include "coop/PartyTransport.h"
#include <set>

using namespace coop;
namespace {
const Endpoint hostEp {0x0a000001,47047};
const Endpoint peers[] {{0x0a000002,50001},{0x0a000002,50002},{0x0a000003,50003}};
struct Party {
    std::shared_ptr<SimNetwork> net=SimNetwork::Create(401);
    PartyHost host {net->Open(hostEp),77};
    std::array<std::unique_ptr<PartyClient>,3> clients;
    double now=0;
    Party() {
        for(size_t i=0;i<3;++i) {
            const auto id=host.Admit(peers[i],100+i,0);
            clients[i]=std::make_unique<PartyClient>(net->Open(peers[i]),hostEp,77,id,100+i);
        }
    }
    void Step(double seconds) {
        const double end=now+seconds;
        while(now<end) { now+=1.0/60; net->SetTime(now); host.Tick(now); for(auto& client:clients) if(client) client->Tick(now); }
    }
};
}
TEST(Party_HostAndThreeGuestsForwardIdentifiedUpdates) {
    Party party;
    REQUIRE(party.host.Guests()==3);
    REQUIRE(party.host.Broadcast(PartyChannel::HostWorld,{'w'},true));
    for(size_t i=0;i<3;++i) REQUIRE(party.clients[i]->Send(PartyChannel::Avatar,{uint8_t(i)},true));
    party.Step(2);
    PartyDelivery frame; std::set<uint8_t> sources;
    while(party.host.Pop(frame)) { sources.insert(frame.source); CHECK(frame.incarnation==100+frame.source-2); }
    CHECK(sources==std::set<uint8_t>({2,3,4}));
    for(size_t i=0;i<3;++i) {
        sources.clear();
        while(party.clients[i]->Pop(frame)) {
            sources.insert(frame.source); CHECK(frame.source!=i+2);
            if(frame.channel==PartyChannel::HostWorld) CHECK(frame.source==1);
        }
        CHECK(sources.size()==3); CHECK(sources.contains(1));
    }
}
TEST(Party_AdmissionIsBoundedAndEndpointIdentityIsStable) {
    Party party;
    CHECK(party.host.Admit(peers[0],100,0)==2);
    CHECK(party.host.Admit({0x0a000004,50100},400,0)==0);
    CHECK(party.host.Admit({0x0a000004,50100},100,0)==0);
    CHECK(!party.host.Remove(1)); REQUIRE(party.host.Remove(3));
    CHECK(party.host.Admit({0x0a000004,50100},400,0)==3);
    CHECK(!party.clients[0]->Send(PartyChannel::HostWorld,{'x'},true));
    CHECK(!party.clients[0]->Send(PartyChannel::Avatar,std::vector<uint8_t>(partywire::MaxPayload+1),true));
}
TEST(Party_ReliableActionsReachAllPeersUnderLossDuplicationAndReordering) {
    Party party; party.net->config.loss=0.22; party.net->config.duplicate=0.35;
    party.net->config.maxLatency=0.12;
    for(auto& client:party.clients) for(uint8_t action=0;action<40;++action)
        REQUIRE(client->Send(PartyChannel::Interaction,{action},true));
    party.Step(10);
    PartyDelivery frame; std::set<std::pair<uint8_t,uint8_t>> seen; size_t count=0;
    while(party.host.Pop(frame)) { ++count; seen.emplace(frame.source,frame.payload.at(0)); }
    CHECK_MSG(count==120,"host got %zu unique %zu rejected %llu guests %zu",count,seen.size(),(unsigned long long)party.host.Rejected(),party.host.Guests()); CHECK(seen.size()==120);
    for(auto& client:party.clients) {
        count=0; seen.clear();
        while(client->Pop(frame)) { ++count; seen.emplace(frame.source,frame.payload.at(0)); }
        CHECK_MSG(count==80,"client got %zu unique %zu",count,seen.size()); CHECK(seen.size()==80);
    }
}
TEST(Party_ReusedSlotRejectsOldGenerationBeforeReliableSequenceState) {
    auto net=SimNetwork::Create(4); net->config.minLatency=net->config.maxLatency=0.05;
    PartyHost host(net->Open(hostEp),77);
    REQUIRE(host.Admit(peers[0],100,0)==2);
    PartyClient old(net->Open(peers[0]),hostEp,77,2,100);
    REQUIRE(old.Send(PartyChannel::Interaction,{'x'},true)); old.Tick(0);
    REQUIRE(host.Remove(2)); REQUIRE(host.Admit(peers[0],200,0)==2);
    PartyClient replacement(net->Open(peers[0]),hostEp,77,2,200);
    REQUIRE(replacement.Send(PartyChannel::Interaction,{'y'},true)); replacement.Tick(0);
    for(double now=0;now<2;now+=1.0/60) { net->SetTime(now); host.Tick(now); replacement.Tick(now); }
    PartyDelivery frame; size_t count=0;
    while(host.Pop(frame)) { ++count; CHECK(frame.incarnation==200); CHECK(frame.payload==std::vector<uint8_t>({'y'})); }
    CHECK(count==1); CHECK(host.Rejected()>=1);
}
TEST(Party_RejectsGuestIdentityAndHostWorldForgery) {
    auto net=SimNetwork::Create(7); PartyHost host(net->Open(hostEp),77);
    REQUIRE(host.Admit(peers[0],100,0)==2);
    auto socket=net->Open(peers[0]); Connection attacker(hostEp);
    REQUIRE(attacker.SendReliable(MsgType::Chat,partywire::Encode({3,100,PartyChannel::Avatar,{'s'},true})));
    REQUIRE(attacker.SendReliable(MsgType::Chat,partywire::Encode({2,100,PartyChannel::HostWorld,{'w'},true})));
    std::vector<std::vector<uint8_t>> packets; attacker.Flush(0,packets);
    for(const auto& packet:packets) { auto bytes=partywire::Wrap(77,100,packet); socket->SendTo(hostEp,bytes.data(),bytes.size()); }
    net->SetTime(1); host.Tick(1); PartyDelivery frame;
    CHECK(!host.Pop(frame)); CHECK(host.Rejected()==2); CHECK(host.Guests()==1);
}
TEST(Party_RealUdpHostAndThreeGuests) {
    std::string error;
    auto hostSocket=UdpSocket::Open(0,error); REQUIRE(hostSocket);
    const Endpoint hostAddress {0x7f000001,hostSocket->LocalPort()};
    PartyHost host(std::move(hostSocket),910);
    std::array<std::unique_ptr<PartyClient>,3> clients;
    for(size_t i=0;i<3;++i) {
        auto socket=UdpSocket::Open(0,error); REQUIRE(socket);
        const Endpoint address {0x7f000001,socket->LocalPort()};
        REQUIRE(host.Admit(address,100+i,0)==i+2);
        clients[i]=std::make_unique<PartyClient>(std::move(socket),hostAddress,910,uint8_t(i+2),100+i);
        REQUIRE(clients[i]->Send(PartyChannel::Interaction,{uint8_t(i)},true));
    }
    for(double now=0;now<3;now+=1.0/120) { host.Tick(now); for(auto& client:clients) client->Tick(now); }
    PartyDelivery frame; size_t count=0; while(host.Pop(frame)) ++count; CHECK(count==3);
    for(auto& client:clients) { count=0; while(client->Pop(frame)) ++count; CHECK(count==2); }
}
TEST(Party_TimedOutGuestIsRemovedWithoutDroppingOtherGuests) {
    Party party; party.clients[0].reset();
    for(int second=0;second<18;++second) {
        for(size_t i=1;i<3;++i) REQUIRE(party.clients[i]->Send(PartyChannel::Avatar,{uint8_t(second)},false));
        party.Step(1);
        PartyDelivery frame; while(party.host.Pop(frame)) {}
        for(size_t i=1;i<3;++i) while(party.clients[i]->Pop(frame)) {}
    }
    CHECK(party.host.Guests()==2); CHECK(!party.host.Failed());
    PartyIdentity removed; REQUIRE(party.host.PopRemoved(removed)); CHECK(removed.id==2 && removed.incarnation==100);
    CHECK(!party.clients[1]->Failed()); CHECK(!party.clients[2]->Failed());
}
TEST(Party_InitialEmptyAckCannotAcknowledgeUndeliveredFirstAction) {
    // Hold the guest's first packet while the host sends an unrelated update.
    auto net=SimNetwork::Create(15); PartyHost host(net->Open(hostEp),77);
    REQUIRE(host.Admit(peers[0],100,0)==2);
    PartyClient client(net->Open(peers[0]),hostEp,77,2,100);
    net->config.cut=true;
    REQUIRE(client.Send(PartyChannel::Interaction,{'a'},true)); client.Tick(0);
    net->config.cut=false;
    REQUIRE(host.Broadcast(PartyChannel::HostWorld,{'w'},true));
    for(double now=0.01;now<3;now+=1.0/60) { net->SetTime(now); host.Tick(now); client.Tick(now); }
    PartyDelivery frame; REQUIRE(host.Pop(frame)); CHECK(frame.payload==std::vector<uint8_t>({'a'})); CHECK(!host.Pop(frame));
}
TEST(Party_UnconsumedQueueExhaustionIsExplicitFailure) {
    auto net=SimNetwork::Create(16); PartyHost host(net->Open(hostEp),77);
    REQUIRE(host.Admit(peers[0],100,0)==2);
    PartyClient client(net->Open(peers[0]),hostEp,77,2,100);
    for(size_t i=0;i<4300;++i) REQUIRE(client.Send(PartyChannel::Interaction,{'a'},true));
    for(double now=0;now<5 && !host.Failed();now+=1.0/60) { net->SetTime(now); host.Tick(now); client.Tick(now); }
    CHECK(host.Failed()); CHECK(!host.Broadcast(PartyChannel::HostWorld,{'w'},true));
}
