#include "TestFramework.h"
#include "coop/PartyNpcAuthority.h"
#include "coop/PartyNpcSync.h"
#include "SimNetwork.h"
#include "FakeWorld.h"
#include "coop/PartyPreviewRuntime.h"
#include <functional>
#include <limits>
#include <random>

using namespace coop;
namespace {
PartyNpcScope Scope() { return {700,4,{101,102,103,104}}; }
PartyIdentity Peer(uint8_t id) { return {id,uint64_t(100+id)}; }
PartyNpcOptions DelegatedOptions() {
    PartyNpcOptions options;
    options.policy=PartyNpcPolicy::NearestPlayer;
    return options;
}
struct World:IPartyNpcWorld {
    std::map<uint64_t,PartyNpcState> states;
    std::map<uint64_t,bool> running;
    bool freeze=true,apply=true,resume=true,capture=true,prepare=true;
    unsigned preparations=0;
    unsigned resumes=0;
    std::vector<uint64_t> retirements;
    std::set<uint64_t> retired;
    unsigned retirementChecks=0;
    void BeginNpcSceneRetirement(uint64_t token) override { retirements.push_back(token); }
    bool ConfirmNpcSceneRetired(uint64_t token) override { ++retirementChecks; return retired.contains(token); }
    std::function<void()> check;
    std::function<void(uint64_t)> onFreeze;
    bool PrepareNpcScene() override {
        ++preparations;
        if (!prepare) return false;
        for (const auto& [npc,state]:states) if (!FreezeNpc(npc)) return false;
        return true;
    }
    bool FreezeNpc(uint64_t npc) override {
        if(!freeze) return false;
        running[npc]=false; if(onFreeze) onFreeze(npc); if(check) check(); return true;
    }
    bool ApplyNpcBaseline(uint64_t npc,const PartyNpcState& state) override {
        CHECK(!running[npc]);
        if(!apply) return false;
        states[npc]=state; return true;
    }
    bool ResumeNpc(uint64_t npc) override {
        if(!resume) return false;
        CHECK(states[npc].life==PartyNpcLife::Alive);
        running[npc]=true; ++resumes; if(check) check(); return true;
    }
    bool CaptureNpc(uint64_t npc,PartyNpcState& state) override {
        if(!capture || !states.contains(npc)) return false;
        state=states[npc]; return true;
    }
};
struct Four {
    PartyNpcAuthority host{Scope(),DelegatedOptions()};
    std::array<World,4> worlds;
    std::array<std::unique_ptr<PartyNpcReplica>,4> peers;
    double now=0;
    Four() {
        for(uint8_t id=1;id<=4;++id) {
            worlds[id-1].check=[this] { Check(); };
            peers[id-1]=std::make_unique<PartyNpcReplica>(Scope(),Peer(id),worlds[id-1]);
        }
    }
    void Check() {
        std::map<uint64_t,unsigned> active;
        for(const auto& world:worlds) for(const auto& [npc,on]:world.running) active[npc]+=on;
        for(const auto& [npc,count]:active) CHECK(count<=1);
    }
    bool Apply(uint8_t target,const PartyNpcMessage& message) {
        PartyNpcMessage decoded;
        const auto bytes=EncodePartyNpc(message);
        CHECK(!bytes.empty()); CHECK(DecodePartyNpc(bytes,decoded));
        const bool accepted=peers[target-1]->Receive(Peer(1),PartyChannel::HostWorld,decoded);
        Check(); return accepted;
    }
    void Pump() {
        for(int round=0;round<100;++round) {
            bool work=false;
            PartyNpcDelivery command;
            while(host.Pop(command)) {
                work=true;
                for(uint8_t id=1;id<=4;++id) if(!command.target || command.target==id) Apply(id,command.message);
            }
            for(uint8_t id=1;id<=4;++id) {
                PartyNpcMessage response;
                while(peers[id-1]->Pop(response)) {
                    work=true; PartyNpcMessage decoded;
                    CHECK(DecodePartyNpc(EncodePartyNpc(response),decoded));
                    host.Receive(Peer(id),decoded,now); Check();
                }
            }
            if(!work) return;
        }
        CHECK_MSG(false,"NPC message loop did not settle");
    }
    bool Add(uint64_t npc=55) {
        const bool ok=host.Register(npc,PartyNpcState{},now); Pump(); return ok;
    }
    bool Transfer(uint8_t id,uint64_t npc=55) {
        now+=3.; const bool ok=host.Propose(npc,Peer(id),now); Pump(); return ok;
    }
};
}

TEST(PartyNpc_FourReplicasUseReleasePrepareCommitWithNoOverlappingSimulation) {
    Four f; REQUIRE(f.Add());
    CHECK(f.worlds[0].running[55]);
    REQUIRE(f.host.Propose(55,Peer(4),3.));
    PartyNpcDelivery command;
    REQUIRE(f.host.Pop(command)); CHECK(command.message.kind==PartyNpcKind::Quiesce); CHECK(command.target==1);
    CHECK(!f.worlds[3].running[55]);
    f.worlds[0].states[55].xf.pos.x=7.f;
    REQUIRE(f.Apply(1,command.message));
    CHECK(!f.worlds[0].running[55]); CHECK(!f.worlds[3].running[55]);
    PartyNpcMessage response;
    REQUIRE(f.peers[0]->Pop(response)); CHECK(response.kind==PartyNpcKind::Released);
    REQUIRE(f.host.Receive(Peer(1),response,3.1));
    REQUIRE(f.host.Pop(command)); CHECK(command.message.kind==PartyNpcKind::Prepare); CHECK(command.target==4);
    REQUIRE(f.Apply(4,command.message)); CHECK(!f.worlds[3].running[55]);
    REQUIRE(f.peers[3]->Pop(response)); CHECK(response.kind==PartyNpcKind::Prepared);
    REQUIRE(f.host.Receive(Peer(4),response,3.2));
    REQUIRE(f.host.Pop(command)); CHECK(command.message.kind==PartyNpcKind::Commit); CHECK(command.target==0);
    for(uint8_t id=1;id<=4;++id) REQUIRE(f.Apply(id,command.message));
    CHECK(f.worlds[3].running[55]); CHECK(!f.worlds[0].running[55]);
    for(const auto& world:f.worlds) CHECK(world.states.at(55).xf.pos.x==7.f);
    CHECK(f.host.Find(55)->owner.id==4); CHECK(f.host.Find(55)->generation==2);
}

TEST(PartyNpc_GuestToGuestHandoffsAndIndependentNpcsKeepSingleWriters) {
    Four f; REQUIRE(f.Add(55)); REQUIRE(f.Add(77));
    REQUIRE(f.Transfer(2,55)); REQUIRE(f.Transfer(3,77)); REQUIRE(f.Transfer(4,55));
    CHECK(f.worlds[3].running[55]); CHECK(f.worlds[2].running[77]);
    CHECK(!f.peers[1]->Report(55)); CHECK(!f.peers[0]->Report(77));
    f.worlds[3].states[55].xf.pos.x=30.f;
    REQUIRE(f.peers[3]->Report(55)); f.Pump();
    for(const auto& world:f.worlds) CHECK(world.states.at(55).xf.pos.x==30.f);
    CHECK(f.host.Find(77)->owner.id==3);
}

TEST(PartyNpc_CanonicalKnockoutDeathAndClothingDoNotResurrectFromOldReports) {
    Four f; REQUIRE(f.Add()); REQUIRE(f.Transfer(2));
    auto& state=f.worlds[1].states[55]; state.life=PartyNpcLife::Unconscious; state.stripped=true;
    REQUIRE(f.peers[1]->Report(55)); f.Pump();
    for(const auto& world:f.worlds) { CHECK(world.states.at(55).life==PartyNpcLife::Unconscious); CHECK(world.states.at(55).stripped); }
    CHECK(!f.peers[1]->Running(55)); CHECK(!f.host.Propose(55,Peer(3),10.));
    state.life=PartyNpcLife::Dead;
    REQUIRE(f.peers[1]->Report(55)); f.Pump();
    for(const auto& world:f.worlds) CHECK(world.states.at(55).life==PartyNpcLife::Dead);
    state.life=PartyNpcLife::Alive;
    CHECK(!f.peers[1]->Report(55));
    PartyNpcMessage forged;
    forged.kind=PartyNpcKind::Report; forged.session=700; forged.epoch=4; forged.npc=55;
    forged.generation=f.host.Find(55)->generation; forged.revision=1; forged.sequence=100; forged.owner=Peer(2);
    CHECK(!f.host.Receive(Peer(2),forged,10.));
    CHECK(f.host.Find(55)->state.life==PartyNpcLife::Dead);
}

TEST(PartyNpc_EngagementThatStartsDuringReleaseKeepsItsOriginalOwner) {
    Four f; REQUIRE(f.Add());
    f.worlds[0].states[55].engaged=true;
    REQUIRE(f.Transfer(3));
    CHECK(f.host.Find(55)->owner.id==1); CHECK(f.host.Find(55)->generation==1);
    CHECK(f.host.Find(55)->issuedGeneration==2);
    CHECK(f.worlds[0].running[55]); CHECK(f.worlds[2].resumes==0);
    CHECK(!f.host.Propose(55,Peer(2),9.));
}

TEST(PartyNpc_MissingFreezeAcknowledgementNeverGrantsAnotherOwner) {
    Four f; REQUIRE(f.Add());
    f.worlds[0].freeze=false;
    REQUIRE(f.host.Propose(55,Peer(4),3.)); f.Pump();
    CHECK(f.host.Find(55)->phase==PartyNpcAuthority::Phase::Releasing);
    CHECK(f.worlds[0].running[55]); CHECK(f.worlds[3].resumes==0);
    f.host.Tick(7.); f.Pump();
    CHECK(f.host.Find(55)->phase==PartyNpcAuthority::Phase::Suspended);
    CHECK(f.worlds[3].resumes==0); CHECK(!f.peers[0]->Report(55));
    CHECK(!f.host.Propose(55,Peer(2),8.));
    f.worlds[0].freeze=true; f.peers[0]->RetryPending();
    CHECK(!f.worlds[0].running[55]);
}

TEST(PartyNpc_PooledActorWaitsForItsBaselineBeforeAcknowledgingOwnership) {
    Four f; f.worlds[3].freeze=false;
    REQUIRE(f.Add()); REQUIRE(f.host.Propose(55,Peer(4),3.)); f.Pump();
    CHECK(f.host.Find(55)->phase==PartyNpcAuthority::Phase::Preparing);
    CHECK(!f.worlds[0].running[55]); CHECK(!f.worlds[3].running[55]);
    f.worlds[3].freeze=true; f.peers[3]->RetryPending(); f.now=3.2; f.Pump();
    CHECK(f.host.Find(55)->owner.id==4); CHECK(f.worlds[3].running[55]);
}

TEST(PartyNpc_LatePooledReplicaAppliesLatestDeathInsteadOfOriginalAliveCatalog) {
    Four f; f.worlds[2].freeze=false; REQUIRE(f.Add());
    f.worlds[0].states[55].life=PartyNpcLife::Dead;
    REQUIRE(f.peers[0]->Report(55)); f.Pump();
    CHECK(!f.worlds[2].states.contains(55));
    f.worlds[2].freeze=true; f.peers[2]->RetryPending();
    CHECK(f.worlds[2].states[55].life==PartyNpcLife::Dead); CHECK(!f.worlds[2].running[55]);
}

TEST(PartyNpc_ReleasedGenerationFencesDelayedCommitsEvenWithNewerRevision) {
    Four f; REQUIRE(f.host.Register(55,PartyNpcState{},0.));
    PartyNpcDelivery initial; REQUIRE(f.host.Pop(initial));
    for(uint8_t id=1;id<=4;++id) REQUIRE(f.Apply(id,initial.message));
    REQUIRE(f.host.Propose(55,Peer(2),3.));
    PartyNpcDelivery release; REQUIRE(f.host.Pop(release)); REQUIRE(f.Apply(1,release.message));
    auto stale=initial.message; stale.revision=1000;
    CHECK(!f.Apply(1,stale)); CHECK(!f.worlds[0].running[55]);
    f.now=3.1; f.Pump(); CHECK(f.worlds[1].running[55]);
    f.host.Stop(); f.Pump();
    auto delayed=initial.message; delayed.owner=Peer(2); delayed.generation=3; delayed.revision=2000;
    CHECK(!f.Apply(2,delayed)); CHECK(!f.worlds[1].running[55]);
}

TEST(PartyNpc_EnvelopeIdentityEpochChannelAndUnregisteredIdsAreChecked) {
    Four f; REQUIRE(f.Add()); REQUIRE(f.peers[0]->Report(55));
    PartyNpcMessage report; REQUIRE(f.peers[0]->Pop(report));
    auto bad=report; bad.npc=999; CHECK(!f.host.Receive(Peer(1),bad,1.));
    bad=report; bad.epoch=5; CHECK(!f.host.Receive(Peer(1),bad,1.));
    bad=report; bad.session=701; CHECK(!f.host.Receive(Peer(1),bad,1.));
    bad=report; bad.owner=Peer(2); CHECK(!f.host.Receive(Peer(2),bad,1.));
    CHECK(!f.host.Receive({1,999},report,1.));
    REQUIRE(f.host.Receive(Peer(1),report,1.));
    CHECK(!f.host.Receive(Peer(1),report,1.));
    auto commit=report; commit.kind=PartyNpcKind::Commit;
    CHECK(!f.peers[1]->Receive(Peer(2),PartyChannel::HostWorld,commit));
    CHECK(!f.peers[1]->Receive(Peer(1),PartyChannel::Interaction,commit));
    CHECK(!f.peers[1]->Receive({1,999},PartyChannel::HostWorld,commit));
}

TEST(PartyNpc_NearestOfFourUsesFreshPositionsHoldTimeAndHysteresis) {
    Four f; REQUIRE(f.Add());
    std::vector<PartyNpcLocation> locations{{Peer(1),{30,0,0},3.},{Peer(2),{10,0,0},3.},{Peer(3),{3,0,0},3.},{Peer(4),{1,0,0},0.}};
    CHECK(!f.host.ConsiderNearest(55,locations,1.));
    REQUIRE(f.host.ConsiderNearest(55,locations,3.)); f.now=3.; f.Pump();
    CHECK(f.host.Find(55)->owner.id==3);
    locations[0].position={2,0,0};
    for(auto& location:locations) location.observed=6.;
    CHECK(!f.host.ConsiderNearest(55,locations,6.)); // Only one metre closer than current owner.
    locations[0].position={30,0,0}; locations[1].position={0,0,0}; locations[2].position={20,0,0}; locations[3].position={40,0,0};
    REQUIRE(f.host.ConsiderNearest(55,locations,6.)); f.now=6.; f.Pump();
    CHECK(f.host.Find(55)->owner.id==2);
}

TEST(PartyNpc_CodecRejectsInvalidStateTruncationTrailingDataAndFuzz) {
    Four f; REQUIRE(f.host.Register(55,PartyNpcState{},0.));
    PartyNpcDelivery command; REQUIRE(f.host.Pop(command));
    const auto bytes=EncodePartyNpc(command.message); REQUIRE(!bytes.empty());
    PartyNpcMessage decoded;
    REQUIRE(DecodePartyNpc(bytes,decoded)); CHECK(decoded.state==command.message.state);
    for(size_t size=0;size<bytes.size();++size) CHECK(!DecodePartyNpc({bytes.begin(),bytes.begin()+size},decoded));
    auto trailing=bytes; trailing.push_back(0); CHECK(!DecodePartyNpc(trailing,decoded));
    command.message.state.xf.pos.x=std::numeric_limits<float>::quiet_NaN(); CHECK(EncodePartyNpc(command.message).empty());
    command.message.state={}; command.message.state.life=PartyNpcLife(99); CHECK(EncodePartyNpc(command.message).empty());
    std::mt19937 random(551);
    for(int i=0;i<10000;++i) {
        std::vector<uint8_t> junk(random()%256); for(auto& value:junk) value=uint8_t(random());
        CHECK(!DecodePartyNpc(junk,decoded));
    }
}

TEST(PartyNpc_CapacityAndQueueOverflowFailExplicitlyWithoutNewWriters) {
    PartyNpcOptions options; options.capacity=2; options.queueLimit=1;
    PartyNpcAuthority host(Scope(),options);
    REQUIRE(host.Register(1,PartyNpcState{},0.));
    CHECK(!host.Register(1,PartyNpcState{},0.));
    CHECK(!host.Register(2,PartyNpcState{},0.));
    CHECK(host.Failed()); CHECK(!host.Propose(1,Peer(2),3.));
    PartyNpcDelivery delivery; CHECK(!host.Pop(delivery));
    PartyNpcAuthority invalid(PartyNpcScope{}); CHECK(invalid.Failed());
}

namespace {
struct NetworkNpc {
    std::shared_ptr<SimNetwork> net;
    std::array<std::unique_ptr<PartyRoom>,4> rooms;
    std::array<World,4> worlds;
    std::array<std::unique_ptr<PartyNpcSync>,4> sync;
    size_t count;
    double now=0;
    explicit NetworkNpc(size_t players=4,uint32_t seed=881,PartyNpcOptions options=DelegatedOptions()):net(SimNetwork::Create(seed)),count(players),options(options) {
        const Endpoint endpoint{0x0a000001,47047};
        for(size_t i=0;i<count;++i) {
            PartyRoomConfig config; config.name="NPC-test-"+std::to_string(i);
            rooms[i]=i==0?PartyRoom::Host(net->Open(endpoint),config):
                PartyRoom::Join(net->Open({uint32_t(0x0a000001+i),uint16_t(50001+i)}),endpoint,config,now);
            worlds[i].check=[this] { Check(); };
        }
    }
    PartyNpcOptions options;
    void Check() {
        std::map<uint64_t,unsigned> active;
        for(size_t i=0;i<count;++i) for(const auto& [npc,on]:worlds[i].running) active[npc]+=on;
        for(const auto& [npc,total]:active) CHECK(total<=1);
    }
    PartyIdentity Identity(size_t index) const {
        const auto& status=rooms[index]->Status();
        for(const auto& member:status.members) if(member.id==status.self) return {member.id,member.incarnation};
        return {};
    }
    void Step(double seconds) {
        const double end=now+seconds;
        while(now<end) {
            now+=1./60.; net->SetTime(now);
            for(size_t i=0;i<count;++i) rooms[i]->Tick(now);
            for(size_t i=0;i<count;++i) if(sync[i]) {
                PartyGameEvent event;
                while(rooms[i]->PopGame(event)) sync[i]->Receive(event,now);
                sync[i]->Tick(now);
            }
            Check();
        }
    }
    bool Begin() {
        Step(2.);
        if(rooms[0]->Status().members.size()!=count || !rooms[0]->SelectMission(kPartyFreeformContract)) return false;
        Step(0.5);
        for(size_t i=0;i<count;++i) if(!rooms[i]->SetReady(true,true)) return false;
        Step(0.5); if(!rooms[0]->BeginLoad()) return false;
        Step(0.5); for(size_t i=0;i<count;++i) if(!rooms[i]->ReportLoaded()) return false;
        Step(0.5);
        for(size_t i=0;i<count;++i) {
            sync[i]=std::make_unique<PartyNpcSync>(*rooms[i],worlds[i],options);
            if(!sync[i]->Start()) return false;
        }
        return true;
    }
    bool Add(uint64_t npc=55) {
        const bool result=sync[0]->Register(npc,PartyNpcState{},now);
        Step(0.5); return result;
    }
};
}

TEST(PartyNpcSync_TwoAndFourPeersRunTheSameOwnershipAndDeathPipeline) {
    for(size_t count:{size_t(2),size_t(4)}) {
        NetworkNpc n(count); REQUIRE(n.Begin()); REQUIRE(n.Add());
        const size_t selected=count-1;
        REQUIRE(n.sync[0]->Propose(55,n.Identity(selected),n.now));
        n.Step(1.5);
        CHECK(n.sync[0]->Authority()->Find(55)->owner.id==n.Identity(selected).id);
        CHECK(n.worlds[selected].running[55]);
        n.worlds[selected].states[55].xf.pos.x=9.f;
        n.worlds[selected].states[55].life=PartyNpcLife::Unconscious;
        n.Step(0.5);
        for(size_t i=0;i<count;++i) {
            CHECK(n.worlds[i].states[55].xf.pos.x==9.f);
            CHECK(n.worlds[i].states[55].life==PartyNpcLife::Unconscious);
            CHECK(!n.worlds[i].running[55]);
        }
        n.worlds[selected].states[55].life=PartyNpcLife::Dead;
        n.Step(0.5);
        for(size_t i=0;i<count;++i) CHECK(n.worlds[i].states[55].life==PartyNpcLife::Dead);
    }
}

TEST(PartyNpc_HostOnlyIsDefaultAndNeverIssuesGuestLeaseAttempts) {
    PartyNpcOptions defaults;
    CHECK(defaults.policy==PartyNpcPolicy::HostOnly);
    PartyNpcAuthority host(Scope());
    REQUIRE(host.Register(55,PartyNpcState{},0));
    CHECK(!host.Propose(55,Peer(2),3));
    CHECK(!host.ConsiderNearest(55,{{Peer(1),{100,0,0},3},{Peer(4),{0,0,0},3}},3));
    REQUIRE(host.Find(55));
    CHECK(host.Find(55)->owner.id==1);
    CHECK(host.Find(55)->generation==1);
    CHECK(host.Find(55)->issuedGeneration==1);
    defaults.policy=PartyNpcPolicy(255);
    PartyNpcAuthority invalid(Scope(),defaults);
    CHECK(invalid.Failed());
}

TEST(PartyNpcSync_HostOnlyTwoAndFourPeersShareMovementKnockoutDeathAndStripping) {
    for(size_t count:{size_t(2),size_t(4)}) {
        NetworkNpc n(count,491,PartyNpcOptions{});
        REQUIRE(n.Begin()); REQUIRE(n.Add());
        for(size_t i=1;i<count;++i) {
            CHECK(!n.worlds[i].running[55]);
            CHECK(n.worlds[i].resumes==0);
            CHECK(!n.sync[0]->Propose(55,n.Identity(i),n.now));
        }
        n.net->config.loss=.18; n.net->config.duplicate=.25;
        n.net->config.minLatency=.001; n.net->config.maxLatency=.05;
        auto& host=n.worlds[0].states[55];
        host.xf.pos={8,2,3}; host.engaged=true;
        n.Step(3);
        for(size_t i=0;i<count;++i) {
            CHECK(n.worlds[i].states[55].xf.pos.x==8);
            CHECK(n.worlds[i].states[55].engaged);
        }
        host.life=PartyNpcLife::Unconscious; host.stripped=true;
        n.Step(3);
        for(size_t i=0;i<count;++i) {
            CHECK(n.worlds[i].states[55].life==PartyNpcLife::Unconscious);
            CHECK(n.worlds[i].states[55].stripped);
            CHECK(!n.worlds[i].running[55]);
        }
        host.life=PartyNpcLife::Dead;
        n.Step(3);
        for(size_t i=0;i<count;++i) CHECK(n.worlds[i].states[55].life==PartyNpcLife::Dead);
        CHECK(n.sync[0]->Authority()->Find(55)->owner.id==1);
        CHECK(n.sync[0]->Authority()->Find(55)->issuedGeneration==1);
    }
}

TEST(PartyNpcSync_HostOnlyLateNativeActorGetsLatestDeathBaseline) {
    NetworkNpc n(4,492,PartyNpcOptions{});
    REQUIRE(n.Begin());
    n.worlds[3].freeze=false;
    REQUIRE(n.Add());
    n.worlds[0].states[55].life=PartyNpcLife::Dead;
    n.worlds[0].states[55].stripped=true;
    n.Step(2);
    CHECK(n.worlds[3].states.find(55)==n.worlds[3].states.end());
    n.worlds[3].freeze=true;
    n.Step(1);
    CHECK(n.worlds[3].states[55].life==PartyNpcLife::Dead);
    CHECK(n.worlds[3].states[55].stripped);
    CHECK(n.worlds[3].resumes==0);
}

namespace {
struct RuntimeNpcSink:IPartyAvatarWorld {
    size_t shown=0;
    bool ApplyPartyRoster(const PartyRoomStatus&) override { return true; }
    bool ShowPartyAvatar(uint8_t,uint64_t,uint32_t,const Transform&) override { ++shown; return true; }
    bool ShowPartyPose(uint8_t,uint64_t,uint32_t,const Pose&) override { return true; }
    bool SetPartyOutfit(uint8_t,uint64_t,uint32_t,const OutfitDesc&) override { return true; }
    bool SetPartySkeleton(uint8_t,uint64_t,uint32_t,const std::vector<int16_t>&) override { return true; }
    bool HidePartyAvatar(uint8_t,uint64_t,uint32_t) override { return true; }
    void HidePartyAvatars() override {}
};
struct RuntimeNpcSource:FakeWorld {
    RuntimeNpcSource():FakeWorld("NPC runtime source") {}
    bool duplicate=false;
    void ListNpcs(std::vector<NpcState>& out) const override {
        FakeWorld::ListNpcs(out);
        if (duplicate && !out.empty()) out.push_back(out.front());
    }
};
struct RuntimeNpcLab {
    std::shared_ptr<SimNetwork> net=SimNetwork::Create(721);
    std::array<RuntimeNpcSource,4> sources;
    std::array<World,4> natives;
    std::array<RuntimeNpcSink,4> sinks;
    std::array<std::unique_ptr<PartyPreviewRuntime>,4> games;
    size_t count;
    double now=0;
    explicit RuntimeNpcLab(size_t players=4,bool enable=true,PartyNpcOptions options={}):count(players) {
        for(size_t i=0;i<count;++i) {
            sources[i].inScene=true;
            sources[i].scene.scene="assembly:/_PRO/Scenes/Missions/TheFacility/_Scene_Mission_Polarbear_Module_002_B.entity";
            sources[i].pose.rotations.resize(2);
            sources[i].skeleton={-1,0};
            NpcState npc; npc.id=55; npc.flags=NpcFlag_Alive;
            sources[i].npcs[55]=npc;
            natives[i].states[55]={};
            natives[i].check=[this] {
                size_t running=0;
                for(size_t p=0;p<count;++p) running+=natives[p].running[55];
                CHECK(running<=1);
            };
            PartyRoomConfig config;
            Endpoint host{0x0a000001,47047};
            auto room=i==0?PartyRoom::Host(net->Open(host),config):
                PartyRoom::Join(net->Open({uint32_t(0x0a000001+i),uint16_t(50001+i)}),host,config,now);
            games[i]=std::make_unique<PartyPreviewRuntime>(std::move(room),sources[i],sinks[i],enable?&natives[i]:nullptr,options);
        }
    }
    void Step(double seconds) {
        const auto end=now+seconds;
        while(now<end) {
            now+=1./60.; net->SetTime(now);
            for(size_t i=0;i<count;++i) games[i]->Tick(now);
        }
    }
    bool Begin() {
        Step(2);
        for(size_t i=0;i<count;++i) if(!games[i]->Ready()) return false;
        Step(.5);
        if(!games[0]->Start()) return false;
        Step(2);
        return games[0]->Status().phase==LobbyRoster::Phase::Playing;
    }
};
}

TEST(PartyNpcRuntime_OptionalAdapterKeepsMovementAndNpcPacketsOnOneLifecycle) {
    for(size_t count:{size_t(2),size_t(4)}) {
        RuntimeNpcLab lab(count);
        REQUIRE(lab.Begin());
        for(size_t i=0;i<count;++i) {
            CHECK(lab.games[i]->SharedNpcsActive());
            CHECK(lab.sinks[i].shown>0);
        }
        CHECK(lab.natives[0].running[55]);
        auto& host=lab.natives[0].states[55];
        host.xf.pos.x=12; host.life=PartyNpcLife::Unconscious;
        lab.Step(2);
        for(size_t i=0;i<count;++i) {
            CHECK(lab.natives[i].states[55].xf.pos.x==12);
            CHECK(lab.natives[i].states[55].life==PartyNpcLife::Unconscious);
            CHECK(!lab.natives[i].running[55]);
        }
        const auto epoch=lab.games[0]->Status().epoch;
        REQUIRE(lab.games[0]->Lobby()); lab.Step(.5);
        for(size_t i=0;i<count;++i) CHECK(!lab.games[i]->SharedNpcsActive());
        REQUIRE(lab.Begin());
        CHECK(lab.games[0]->Status().epoch>epoch);
        host.life=PartyNpcLife::Dead; lab.Step(1);
        for(size_t i=0;i<count;++i) CHECK(lab.natives[i].states[55].life==PartyNpcLife::Dead);
    }
}

TEST(PartyNpcRuntime_DefaultMovementModeNeverCallsNativeNpcOperations) {
    RuntimeNpcLab lab(4,false);
    REQUIRE(lab.Begin());
    lab.Step(2);
    for(size_t i=0;i<4;++i) {
        CHECK(!lab.games[i]->SharedNpcsActive());
        CHECK(lab.natives[i].running.empty());
        CHECK(lab.natives[i].resumes==0);
        CHECK(lab.sinks[i].shown>0);
    }
}

TEST(PartyNpcRuntime_AmbiguousCatalogIdsAreExcludedAndPooledCaptureRetries) {
    RuntimeNpcLab lab(2);
    lab.sources[0].duplicate=true;
    REQUIRE(lab.Begin());
    CHECK(!lab.natives[0].running[55]);
    lab.sources[0].duplicate=false;
    lab.natives[0].capture=false;
    lab.Step(1.5);
    CHECK(!lab.natives[0].running[55]);
    lab.natives[0].capture=true;
    lab.Step(1.5);
    CHECK(lab.natives[0].running[55]);
    CHECK(!lab.natives[1].running[55]);
}

TEST(PartyNpcRuntime_UnconfirmedStopRequiresSceneReloadBeforeReady) {
    RuntimeNpcLab lab(2);
    REQUIRE(lab.Begin());
    lab.natives[0].freeze=false;
    REQUIRE(lab.games[0]->Lobby()); lab.Step(.5);
    CHECK(!lab.games[0]->NativeReady());
    CHECK(!lab.games[0]->Ready());
    CHECK(lab.games[0]->Message().find("did not confirm")!=std::string::npos);
    lab.natives[0].freeze=true;
    lab.Step(1);
    CHECK(!lab.games[0]->NativeReady()); // A later retry cannot clear the scene latch.
    lab.games[0]->SceneCleared();
    CHECK(!lab.games[0]->NativeReady()); // Callback precedes native destruction.
    lab.Step(.5);
    CHECK(!lab.games[0]->NativeReady());
    REQUIRE(lab.natives[0].retirements.size()==1);
    lab.natives[0].retired.insert(lab.natives[0].retirements.back());
    lab.Step(.1);
    CHECK(lab.games[0]->NativeReady());
}

TEST(PartyNpcRuntime_EarlySceneClearDoesNotAllowPreparationOrOldRetirementReceipts) {
    RuntimeNpcLab lab(2); REQUIRE(lab.Begin());
    const auto preparations=lab.natives[0].preparations;
    const auto resumes=lab.natives[0].resumes;
    lab.games[0]->SceneCleared();
    REQUIRE(lab.natives[0].retirements.size()==1);
    const auto old=lab.natives[0].retirements.back();
    lab.games[0]->SceneCleared();
    REQUIRE(lab.natives[0].retirements.size()==2);
    const auto current=lab.natives[0].retirements.back();
    CHECK(current>old);
    lab.natives[0].retired.insert(old);
    lab.Step(1);
    CHECK(!lab.games[0]->NativeReady()); CHECK(!lab.games[0]->Ready());
    CHECK(lab.natives[0].preparations==preparations);
    CHECK(lab.natives[0].resumes==resumes);
    CHECK(!lab.games[0]->SharedNpcsActive());
    lab.natives[0].retired.insert(current);
    lab.Step(.1);
    CHECK(lab.games[0]->NativeReady());
}

TEST(PartyNpcRuntime_SceneRetirementTokensCannotBeReusedAcrossRuntimeInstances) {
    RuntimeNpcLab first(2); REQUIRE(first.Begin());
    first.games[0]->SceneCleared(); first.games[1]->SceneCleared();
    const auto host=first.natives[0].retirements.back();
    const auto guest=first.natives[1].retirements.back();
    CHECK(host!=guest);
    first.natives[1].retired.insert(host); first.Step(.1);
    CHECK(!first.games[1]->NativeReady());
    RuntimeNpcLab next(2); REQUIRE(next.Begin());
    next.games[0]->SceneCleared();
    const auto replacement=next.natives[0].retirements.back();
    CHECK(replacement!=host); CHECK(replacement!=guest);
    next.natives[0].retired.insert(host); next.Step(.1);
    CHECK(!next.games[0]->NativeReady());
    next.natives[0].retired.insert(replacement); next.Step(.1);
    CHECK(next.games[0]->NativeReady());
}

TEST(PartyNpcRuntime_MovementOnlyClearNeverRequestsNpcRetirement) {
    RuntimeNpcLab lab(2,false); REQUIRE(lab.Begin());
    lab.games[0]->SceneCleared(); lab.Step(.1);
    CHECK(lab.games[0]->NativeReady());
    CHECK(lab.natives[0].retirements.empty());
    CHECK(lab.natives[0].retirementChecks==0);
}

TEST(PartyNpcRuntime_PendingRetirementDoesNotTouchThePreviousScenesActorsAgain) {
    RuntimeNpcLab lab(2); REQUIRE(lab.Begin());
    lab.games[0]->SceneCleared();
    // Scene-clearing callbacks have returned; the native engine may now destroy
    // actors or reuse template IDs. Polling may only inspect retirement proof.
    unsigned freezes=0;
    lab.natives[0].onFreeze=[&](uint64_t) { ++freezes; };
    lab.sources[0].inScene=false; lab.Step(.5);
    lab.sources[0].inScene=true; lab.Step(.5);
    CHECK(freezes==0); CHECK(!lab.games[0]->NativeReady());
    const auto checks=lab.natives[0].retirementChecks;
    lab.natives[0].retired.insert(lab.natives[0].retirements.back());
    lab.Step(.1); REQUIRE(lab.games[0]->NativeReady());
    lab.Step(.5);
    CHECK(lab.natives[0].retirementChecks==checks+1);
    CHECK(freezes==0);
}

TEST(PartyNpcRuntime_CatalogOverflowInterruptsInsteadOfSilentlyDroppingNpcs) {
    PartyNpcOptions options; options.capacity=1;
    RuntimeNpcLab lab(2,true,options);
    auto extra=lab.sources[0].npcs[55]; extra.id=56;
    lab.sources[0].npcs[56]=extra;
    lab.natives[0].states[56]={};
    CHECK(!lab.Begin());
    lab.Step(1);
    for(size_t i=0;i<2;++i) {
        CHECK(!lab.games[i]->SharedNpcsActive());
        CHECK(!lab.natives[i].running[55]);
        CHECK(lab.games[i]->Status().phase==LobbyRoster::Phase::Interrupted);
    }
}

TEST(PartyNpcRuntime_AllNativeScenesMustBePreparedBeforeAnyLeaseRuns) {
    RuntimeNpcLab lab(4);
    lab.natives[3].prepare=false;
    CHECK(!lab.Begin());
    lab.Step(1);
    for(size_t i=0;i<4;++i) {
        CHECK(lab.games[i]->Status().phase==LobbyRoster::Phase::Loading);
        CHECK(!lab.games[i]->SharedNpcsActive());
        CHECK(lab.natives[i].resumes==0);
        CHECK(lab.natives[i].preparations>0);
    }
    lab.natives[3].prepare=true;
    lab.Step(2);
    for(size_t i=0;i<4;++i) CHECK(lab.games[i]->SharedNpcsActive());
    CHECK(lab.natives[0].running[55]);
}

TEST(PartyNpcRuntime_GuestLeaveAndNativeSceneLossStopEveryReplica) {
    for(bool sceneLoss:{false,true}) {
        RuntimeNpcLab lab(4);
        REQUIRE(lab.Begin());
        if (sceneLoss) {
            lab.sources[2].inScene=false;
            lab.games[2]->SceneCleared();
        } else {
            lab.games[2]->Leave(lab.now);
        }
        lab.Step(2);
        for(size_t i=0;i<4;++i) {
            CHECK(!lab.games[i]->SharedNpcsActive());
            CHECK(!lab.natives[i].running[55]);
        }
        CHECK(lab.games[0]->Status().phase==LobbyRoster::Phase::Interrupted);
    }
}

TEST(PartyNpcRuntime_RegisteredIdBecomingAmbiguousInterruptsItsScope) {
    RuntimeNpcLab lab(2);
    REQUIRE(lab.Begin());
    lab.sources[0].duplicate=true;
    lab.Step(2);
    CHECK(lab.games[0]->Status().phase==LobbyRoster::Phase::Interrupted);
    for(size_t i=0;i<2;++i) {
        CHECK(!lab.games[i]->SharedNpcsActive());
        CHECK(!lab.natives[i].running[55]);
    }
}

TEST(PartyNpcSync_GuestHandoffsConvergeUnderLossDuplicationAndReordering) {
    for(uint32_t seed=1;seed<=8;++seed) {
        NetworkNpc n(4,seed); REQUIRE(n.Begin()); REQUIRE(n.Add());
        n.net->config.loss=0.22; n.net->config.duplicate=0.3;
        n.net->config.minLatency=0.001; n.net->config.maxLatency=0.07;
        REQUIRE(n.sync[0]->Propose(55,n.Identity(1),n.now)); n.Step(2.5);
        REQUIRE(n.sync[0]->Authority()->Find(55)->owner.id==n.Identity(1).id);
        REQUIRE(n.sync[0]->Propose(55,n.Identity(3),n.now)); n.Step(2.5);
        REQUIRE(n.sync[0]->Authority()->Find(55)->owner.id==n.Identity(3).id);
        n.worlds[3].states[55].xf.pos.x=42.f;
        n.Step(4.);
        for(size_t i=0;i<4;++i) {
            CHECK(n.sync[i]->Active()); CHECK(!n.sync[i]->Failed());
            CHECK(n.worlds[i].states[55].xf.pos.x==42.f);
        }
        CHECK(n.net->dropped>0);
    }
}

TEST(PartyNpcSync_PartitionFreezesAllPeersWithoutGivingATimedOutOwnerAReplacement) {
    NetworkNpc n; REQUIRE(n.Begin()); REQUIRE(n.Add());
    REQUIRE(n.sync[0]->Propose(55,n.Identity(2),n.now)); n.Step(1.5);
    REQUIRE(n.worlds[2].running[55]);
    n.net->config.cut=true;
    n.Step(18.);
    for(size_t i=0;i<4;++i) { CHECK(!n.sync[i]->Active()); CHECK(!n.worlds[i].running[55]); }
    CHECK(n.rooms[0]->Status().phase==LobbyRoster::Phase::Interrupted);
    CHECK(!n.sync[0]->Start());
}

TEST(PartyNpcSync_NonownerAndWrongChannelClaimsDoNotReachOtherReplicas) {
    NetworkNpc n; REQUIRE(n.Begin()); REQUIRE(n.Add());
    PartyNpcScope scope; REQUIRE(PartyNpcScope::FromRoom(n.rooms[1]->Status(),scope));
    PartyNpcMessage claim;
    claim.kind=PartyNpcKind::Report; claim.session=scope.session; claim.epoch=scope.epoch;
    claim.npc=55; claim.generation=1; claim.revision=1; claim.sequence=99; claim.owner=n.Identity(1);
    claim.state.life=PartyNpcLife::Dead;
    REQUIRE(n.rooms[1]->SendGame(PartyChannel::Interaction,EncodePartyNpc(claim),true));
    CHECK(!n.rooms[1]->SendGame(PartyChannel::HostWorld,EncodePartyNpc(claim),true));
    claim.kind=PartyNpcKind::Commit;
    REQUIRE(n.rooms[1]->SendGame(PartyChannel::Interaction,EncodePartyNpc(claim),true));
    n.Step(1.);
    for(size_t i=0;i<4;++i) CHECK(n.worlds[i].states[55].life==PartyNpcLife::Alive);
    CHECK(n.sync[0]->Authority()->Find(55)->owner.id==1);
    CHECK(!n.sync[1]->Register(777,PartyNpcState{},n.now));
}

TEST(PartyNpcSync_HostProximityPolicySelectsAmongAllFourValidatedIdentities) {
    NetworkNpc n; REQUIRE(n.Begin()); REQUIRE(n.Add());
    n.Step(2.);
    REQUIRE(n.sync[0]->ObservePlayer(n.Identity(0),{40,0,0},n.now));
    REQUIRE(n.sync[0]->ObservePlayer(n.Identity(1),{20,0,0},n.now));
    REQUIRE(n.sync[0]->ObservePlayer(n.Identity(2),{1,0,0},n.now));
    REQUIRE(n.sync[0]->ObservePlayer(n.Identity(3),{30,0,0},n.now));
    n.Step(1.);
    CHECK(n.sync[0]->Authority()->Find(55)->owner.id==n.Identity(2).id);
    CHECK(n.worlds[2].running[55]);
}

TEST(PartyNpc_StopRequiresNativeConfirmationAndCannotRestartTheSameScope) {
    NetworkNpc n; REQUIRE(n.Begin()); REQUIRE(n.Add());
    n.sync[0]->Stop();
    CHECK(!n.sync[0]->Start());
    CHECK(!n.worlds[0].running[55]);
    Four f; REQUIRE(f.Add()); f.worlds[0].freeze=false;
    CHECK(!f.peers[0]->Stop()); CHECK(!f.peers[0]->StopConfirmed());
    CHECK(f.peers[0]->Running(55)); CHECK(!f.peers[0]->Report(55));
    f.worlds[0].freeze=true; CHECK(f.peers[0]->Stop()); CHECK(!f.peers[0]->Running(55));
}

TEST(PartyNpcSync_StoppingTheHostInterruptsAndFreezesAGuestOwnedNpc) {
    NetworkNpc n; REQUIRE(n.Begin()); REQUIRE(n.Add());
    REQUIRE(n.sync[0]->Propose(55,n.Identity(3),n.now)); n.Step(1.5);
    REQUIRE(n.worlds[3].running[55]);
    n.sync[0]->Stop(); n.Step(0.5);
    for(size_t i=0;i<4;++i) {
        CHECK(!n.sync[i]->Active());
        CHECK(!n.worlds[i].running[55]);
        CHECK(n.rooms[i]->Status().phase==LobbyRoster::Phase::Interrupted);
    }
}

TEST(PartyNpc_BusyDeclinesDoNotFreezeInteractionsOrReuseOldHandoffTickets) {
    Four f; REQUIRE(f.Add());
    f.worlds[0].states[55].interacting=true;
    REQUIRE(f.Transfer(2));
    CHECK(f.worlds[0].running[55]); CHECK(f.worlds[0].resumes==1);
    CHECK(f.host.Find(55)->owner.id==1); CHECK(f.host.Find(55)->generation==1);
    f.worlds[0].states[55].interacting=false;
    REQUIRE(f.peers[0]->Report(55)); f.Pump();
    REQUIRE(f.Transfer(2));
    CHECK(f.host.Find(55)->generation==3); CHECK(f.worlds[1].running[55]);
}

TEST(PartyNpc_EngagementRaceAfterNativeFreezeReturnsTheLeaseToItsOriginalOwner) {
    Four f; REQUIRE(f.Add());
    f.worlds[0].onFreeze=[&](uint64_t npc) { f.worlds[0].states[npc].engaged=true; };
    REQUIRE(f.Transfer(3));
    CHECK(f.host.Find(55)->generation==2); CHECK(f.host.Find(55)->owner.id==1);
    CHECK(f.worlds[0].running[55]); CHECK(f.worlds[2].resumes==0);
}

TEST(PartyNpc_SameLeaseRefreshPreservesReportSequenceAndDoesNotRestartAi) {
    Four f; REQUIRE(f.host.Register(55,PartyNpcState{},0.));
    PartyNpcDelivery baseline; REQUIRE(f.host.Pop(baseline));
    for(uint8_t id=1;id<=4;++id) REQUIRE(f.Apply(id,baseline.message));
    REQUIRE(f.peers[0]->Report(55));
    PartyNpcMessage first; REQUIRE(f.peers[0]->Pop(first)); CHECK(first.sequence==1);
    auto refresh=baseline.message; refresh.revision=10;
    REQUIRE(f.Apply(1,refresh)); CHECK(f.worlds[0].resumes==1);
    f.worlds[0].states[55].xf.pos.x=2.f;
    REQUIRE(f.peers[0]->Report(55));
    PartyNpcMessage second; REQUIRE(f.peers[0]->Pop(second)); CHECK(second.sequence==2);
}

TEST(PartyNpc_NewOwnerDoesNotRunUntilNativeResumeActuallySucceeds) {
    Four f; REQUIRE(f.Add()); f.worlds[3].resume=false;
    REQUIRE(f.Transfer(4));
    CHECK(f.host.Find(55)->owner.id==4);
    CHECK(!f.worlds[3].running[55]); CHECK(!f.peers[3]->Running(55));
    CHECK(!f.peers[3]->Report(55));
    f.worlds[3].resume=true; f.peers[3]->RetryPending();
    CHECK(f.worlds[3].running[55]); CHECK(f.worlds[3].resumes==1);
    REQUIRE(f.peers[3]->Report(55)); f.Pump();
    CHECK(f.host.Find(55)->confirmed);
}
