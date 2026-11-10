#include "TestFramework.h"
#include "FakeWorld.h"
#include "SimNetwork.h"
#include "coop/PartyReplayClient.h"
#include "coop/PartyPreviewRuntime.h"

using namespace coop;
namespace {
struct Sink:IPartyAvatarWorld {
    std::map<uint8_t,Transform> transforms;
    std::map<uint8_t,Pose> poses;
    std::map<uint8_t,OutfitDesc> outfits;
    std::map<uint8_t,std::vector<int16_t>> skeletons;
    unsigned shown=0;
    bool ApplyPartyRoster(const PartyRoomStatus&) override { return true; }
    bool ShowPartyAvatar(uint8_t id,uint64_t,uint32_t,const Transform& value) override { transforms[id]=value; ++shown; return true; }
    bool ShowPartyPose(uint8_t id,uint64_t,uint32_t,const Pose& value) override { poses[id]=value; return true; }
    bool SetPartyOutfit(uint8_t id,uint64_t,uint32_t,const OutfitDesc& value) override { outfits[id]=value; return true; }
    bool SetPartySkeleton(uint8_t id,uint64_t,uint32_t,const std::vector<int16_t>& value) override { skeletons[id]=value; return true; }
    bool HidePartyAvatar(uint8_t id,uint64_t,uint32_t) override { transforms.erase(id); return true; }
    void HidePartyAvatars() override { transforms.clear(); }
};
struct Lab {
    std::shared_ptr<SimNetwork> net=SimNetwork::Create(316);
    std::array<FakeWorld,2> worlds {FakeWorld("Real host"),FakeWorld("Real guest")};
    std::array<Sink,2> sinks;
    std::array<std::unique_ptr<PartyPreviewRuntime>,2> games;
    std::array<std::unique_ptr<PartyReplayClient>,2> bots;
    double now=0;
    Lab() {
        Endpoint host{0x0a000001,47047};
        for(size_t i=0;i<2;++i) {
            worlds[i].inScene=true;
            worlds[i].scene.scene="assembly:/_PRO/Scenes/Missions/TheFacility/_Scene_Mission_Polarbear_Module_002_B.entity";
            worlds[i].player.pos={float(10+10*i),10.f,0.f};
            worlds[i].pose.rotations.resize(2); worlds[i].pose.rootPos.x=float(1+2*i);
            worlds[i].pose.hasWorldRotation=true;
            worlds[i].pose.worldRotation=Normalize({.2f,.3f,.4f,.5f});
            worlds[i].skeleton={-1,0}; worlds[i].outfit.repositoryId=i==0?"host-disguise":"guest-disguise";
            PartyRoomConfig config; config.name=i==0?"Real host":"Real guest";
            auto room=i==0?PartyRoom::Host(net->Open(host),config):PartyRoom::Join(net->Open({0x0a000002,50002}),host,config,now);
            games[i]=std::make_unique<PartyPreviewRuntime>(std::move(room),worlds[i],sinks[i]);
            config.name="[LAB BOT "+std::to_string(i+1)+"]";
            auto synthetic=PartyRoom::Join(net->Open({uint32_t(0x0a000003+i),uint16_t(50003+i)}),host,config,now);
            bots[i]=std::make_unique<PartyReplayClient>(std::move(synthetic),Vec3{i==0?1.2f:-1.2f,0,0});
        }
    }
    void Step(double seconds) {
        const double end=now+seconds;
        while(now<end) {
            now+=1./60.; net->SetTime(now);
            for(auto& game:games) game->Tick(now);
            for(auto& bot:bots) bot->Tick(now);
        }
    }
    bool Start() {
        Step(2.);
        for(auto& game:games) if(!game->Ready()) return false;
        Step(1.);
        if(!games[0]->Start()) return false;
        Step(2.);
        return games[0]->Status().phase==LobbyRoster::Phase::Playing;
    }
};
}

TEST(PartyReplay_TwoRealSourcesAndTwoLabeledSyntheticPeersFillFourSeats) {
    Lab lab; REQUIRE(lab.Start());
    CHECK(lab.games[0]->Status().members.size()==4);
    size_t bots=0;
    for(const auto& member:lab.games[0]->Status().members) bots+=member.name.find("[LAB BOT ")==0;
    CHECK(bots==2);
    lab.Step(1.);
    for(size_t i=0;i<2;++i) {
        const auto id=lab.bots[i]->Status().self;
        CHECK(lab.bots[i]->Forwarded()>0);
        for(size_t real=0;real<2;++real) {
            REQUIRE(lab.sinks[real].transforms.contains(id));
            CHECK(std::fabs(lab.sinks[real].transforms[id].pos.x-(10.f+(i==0?1.2f:-1.2f)))<0.001f);
            CHECK(lab.sinks[real].poses[id].rootPos.x==1.f);
            CHECK(lab.sinks[real].poses[id].hasWorldRotation);
            CHECK(AngleBetween(lab.sinks[real].poses[id].worldRotation,lab.worlds[0].pose.worldRotation)<.005f);
            CHECK(lab.sinks[real].outfits[id].repositoryId=="host-disguise");
            CHECK(lab.sinks[real].skeletons[id]==std::vector<int16_t>({-1,0}));
        }
    }
}

TEST(PartyReplay_WaitsForRealHostReadinessRatherThanClaimingANativeGameExists) {
    Lab lab;
    lab.worlds[0].inScene=false; lab.worlds[1].inScene=false;
    lab.Step(3.);
    CHECK(!lab.games[0]->Ready());
    for(const auto& bot:lab.bots) {
        bool ready=false; for(const auto& member:bot->Status().members) if(member.id==bot->Status().self) ready=member.ready;
        CHECK(!ready); CHECK(bot->Forwarded()==0);
    }
    CHECK(!lab.games[0]->Start());
}

TEST(PartyReplay_OnlyMirrorsItsChosenSourceAndFollowsDisguiseChanges) {
    Lab lab; REQUIRE(lab.Start());
    lab.worlds[1].player.pos.x=70.f; lab.worlds[1].outfit.repositoryId="unrelated-guest-outfit";
    lab.worlds[0].player.pos.x=12.f; lab.worlds[0].outfit.repositoryId="new-host-outfit";
    lab.Step(2.);
    for(size_t i=0;i<2;++i) {
        const auto id=lab.bots[i]->Status().self;
        CHECK(std::fabs(lab.sinks[1].transforms[id].pos.x-(12.f+(i==0?1.2f:-1.2f)))<0.001f);
        CHECK(lab.sinks[1].outfits[id].repositoryId=="new-host-outfit");
    }
}

TEST(PartyReplay_StopsOutsidePlayAndResumesOnlyInTheNewEpoch) {
    Lab lab; REQUIRE(lab.Start());
    const auto previous=lab.games[0]->Status().epoch;
    REQUIRE(lab.games[0]->Lobby()); lab.Step(0.5);
    const auto shown=lab.sinks[1].shown; lab.Step(0.5); CHECK(lab.sinks[1].shown==shown);
    CHECK(lab.games[0]->Status().epoch>previous);
    for(auto& game:lab.games) REQUIRE(game->Ready());
    lab.Step(1.); REQUIRE(lab.games[0]->Start()); lab.Step(2.);
    CHECK(lab.sinks[1].shown>shown);
    lab.bots[0]->Leave(lab.now); lab.Step(1.);
    CHECK(lab.games[0]->Status().phase==LobbyRoster::Phase::Interrupted);
}
