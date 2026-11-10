#include "TestFramework.h"
#include "coop/PartyPresentationRoster.h"
#include "coop/PartyAvatarPlayback.h"
#include "coop/PartyPreviewRuntime.h"
#include "FakeWorld.h"
#include "SimNetwork.h"
#include "coop/LauncherProtocol.h"

using namespace coop;
namespace {
PartyRoomStatus Playing(uint8_t self = 1) {
    PartyRoomStatus status;
    status.state = self == 1 ? PartyRoomStatus::State::Hosting : PartyRoomStatus::State::Connected;
    status.host = self == 1;
    status.self = self;
    status.session = 700;
    status.revision = 10;
    status.epoch = 4;
    status.phase = LobbyRoster::Phase::Playing;
    status.mission = kPartyFreeformContract;
    for (uint8_t id = 1; id <= 4; ++id)
        status.members.push_back({id, uint64_t(100 + id), "Agent", true, true, true, false});
    return status;
}
}

TEST(PartyPresentation_ThreeRemoteIdentitiesForHostAndGuest) {
    for (uint8_t self = 1; self <= 4; ++self) {
        PartyPresentationRoster roster;
        auto status = Playing(self);
        REQUIRE(roster.Update(status));
        unsigned remote = 0;
        for (uint8_t id = 1; id <= 4; ++id) {
            CHECK((roster.Remote(id, 100 + id, 4) != nullptr) == (id != self));
            remote += roster.Remote(id, 100 + id, 4) != nullptr;
        }
        CHECK(remote == 3);
        CHECK(!roster.Remote(0, 100, 4));
        CHECK(!roster.Remote(5, 105, 4));
        CHECK(!roster.Remote(2, 999, 4));
        CHECK(!roster.Remote(2, 102, 3));
    }
}

TEST(PartyPresentation_RejectsMalformedSnapshotsWithoutReplacingCurrentScope) {
    PartyPresentationRoster roster;
    REQUIRE(roster.Update(Playing()));
    auto bad = Playing();
    bad.members[1].id = 1;
    CHECK(!roster.Update(bad));
    bad = Playing(); bad.members[1].incarnation = 0;
    CHECK(!roster.Update(bad));
    bad = Playing(); bad.members[1].id = 5;
    CHECK(!roster.Update(bad));
    bad = Playing(); bad.members.erase(bad.members.begin());
    CHECK(!roster.Update(bad));
    bad = Playing(3); bad.members.erase(bad.members.begin() + 2);
    CHECK(!roster.Update(bad));
    bad = Playing(); bad.members[1].loaded = false;
    CHECK(!roster.Update(bad));
    bad = Playing(); bad.epoch = 0;
    CHECK(!roster.Update(bad));
    bad = Playing(); bad.mission = "different-contract";
    CHECK(!roster.Update(bad));
    CHECK(roster.Remote(4, 104, 4) != nullptr);
    CHECK(roster.Status().revision == 10);
}

TEST(PartyPresentation_StaleRosterReplayAndRoomSwitchCannotDriveOldActors) {
    PartyPresentationRoster roster;
    REQUIRE(roster.Update(Playing()));
    auto update = Playing(); update.revision = 9;
    CHECK(!roster.Update(update));
    update = Playing(); update.epoch = 3;
    CHECK(!roster.Update(update));
    update = Playing(); update.session = 800;
    CHECK(!roster.Update(update));
    update = Playing(2);
    CHECK(!roster.Update(update));
    update = Playing(); update.revision = 11; update.epoch = 5;
    REQUIRE(roster.Update(update));
    CHECK(!roster.Remote(4, 104, 4));
    CHECK(roster.Remote(4, 104, 5) != nullptr);
    CHECK(!roster.Update(Playing()));
    roster.Reset();
    update = Playing(2); update.session = 800;
    REQUIRE(roster.Update(update));
    CHECK(roster.Remote(1, 101, 4) != nullptr);
    CHECK(!roster.Remote(2, 102, 4));
}

TEST(PartyPresentation_ReusedSeatAndInterruptedMissionInvalidateOldStreams) {
    PartyPresentationRoster roster;
    REQUIRE(roster.Update(Playing()));
    auto status = Playing();
    status.revision = 11;
    status.members[3].incarnation = 900;
    REQUIRE(roster.Update(status));
    CHECK(!roster.Remote(4, 104, 4));
    CHECK(roster.Remote(4, 900, 4) != nullptr);
    CHECK(roster.Remote(2, 102, 4) != nullptr);
    status.revision = 12;
    status.phase = LobbyRoster::Phase::Interrupted;
    REQUIRE(roster.Update(status));
    CHECK(!roster.Remote(2, 102, 4));
    CHECK(!roster.Remote(4, 900, 4));
    status.revision = 13;
    status.phase = LobbyRoster::Phase::Lobby;
    REQUIRE(roster.Update(status));
    CHECK(!roster.Remote(4, 900, 4));
}

namespace {
struct AvatarSink : IPartyAvatarWorld {
    std::string fault;
    std::string PartyPreviewError() const override { return fault; }
    bool accept = true;
    std::array<Transform, 5> transforms;
    std::array<Pose, 5> poses;
    std::array<OutfitDesc, 5> outfits;
    std::array<std::vector<int16_t>, 5> skeletons;
    std::array<int, 5> shown {}, posed {}, hidden {};
    int hiddenAll = 0;
    bool ApplyPartyRoster(const PartyRoomStatus&) override { return accept; }
    bool ShowPartyAvatar(uint8_t id, uint64_t, uint32_t, const Transform& xf) override {
        transforms[id] = xf; ++shown[id]; return true;
    }
    bool ShowPartyPose(uint8_t id, uint64_t, uint32_t, const Pose& pose) override {
        poses[id] = pose; ++posed[id]; return true;
    }
    bool SetPartyOutfit(uint8_t id, uint64_t, uint32_t, const OutfitDesc& outfit) override { outfits[id] = outfit; return true; }
    bool SetPartySkeleton(uint8_t id, uint64_t, uint32_t, const std::vector<int16_t>& parents) override { skeletons[id] = parents; return true; }
    bool HidePartyAvatar(uint8_t id, uint64_t, uint32_t) override { ++hidden[id]; return true; }
    void HidePartyAvatars() override { ++hiddenAll; }
};
PartyGameEvent AvatarEvent(uint8_t id, uint32_t ms, float x) {
    MsgAvatarState message;
    message.epoch = 4; message.timeMs = ms; message.xf.pos.x = x;
    return {id, uint64_t(100 + id), 4, PartyChannel::Avatar, PartyAvatarPacket(message), false};
}
PartyGameEvent PoseEvent(uint8_t id, uint32_t ms, float x) {
    MsgPose message;
    message.epoch = 4; message.timeMs = ms; message.pose.rotations.resize(2);
    message.pose.rootPos.x = x;
    return {id, uint64_t(100 + id), 4, PartyChannel::Pose, PartyAvatarPacket(message), false};
}
}

TEST(PartyPlayback_ThreeIndependentTypedStreamsAndBoundedPackets) {
    AvatarSink sink;
    PartyAvatarPlayback playback(sink);
    REQUIRE(playback.UpdateRoster(Playing()));
    for (uint8_t id = 2; id <= 4; ++id) {
        REQUIRE(playback.Receive(AvatarEvent(id, 1000, 10.f * id), 1.));
        MsgOutfit outfit; outfit.outfit.repositoryId = "outfit-" + std::to_string(id);
        REQUIRE(playback.Receive({id, uint64_t(100 + id), 4, PartyChannel::Avatar, PartyAvatarPacket(outfit), true}, 1.));
        MsgSkeleton skeleton; skeleton.parents = {-1, 0};
        REQUIRE(playback.Receive({id, uint64_t(100 + id), 4, PartyChannel::Avatar, PartyAvatarPacket(skeleton), true}, 1.));
        REQUIRE(playback.Receive(PoseEvent(id, 1000, 2.f * id), 1.));
    }
    playback.Tick(1.1);
    for (uint8_t id = 2; id <= 4; ++id) {
        CHECK(sink.shown[id] == 1);
        CHECK(sink.posed[id] == 1);
        CHECK(sink.transforms[id].pos.x == 10.f * id);
        CHECK(sink.poses[id].rootPos.x == 2.f * id);
        CHECK(sink.outfits[id].repositoryId == "outfit-" + std::to_string(id));
        CHECK(sink.skeletons[id] == std::vector<int16_t>({-1, 0}));
    }
    MsgPose maxPose; maxPose.pose.rotations.resize(kMaxPoseBones);
    CHECK(!PartyAvatarPacket(maxPose).empty());
    CHECK(PartyAvatarPacket(maxPose).size() <= partywire::MaxPayload - 4);
}

TEST(PartyPlayback_RejectsMalformedWrongChannelAndStaleIdentityBeforeNativeCalls) {
    AvatarSink sink;
    PartyAvatarPlayback playback(sink);
    REQUIRE(playback.UpdateRoster(Playing()));
    auto event = AvatarEvent(2, 1000, 10.f);
    event.incarnation = 900;
    CHECK(!playback.Receive(event, 1.));
    event = AvatarEvent(1, 1000, 10.f);
    CHECK(!playback.Receive(event, 1.));
    event = AvatarEvent(2, 1000, 10.f); event.epoch = 5;
    CHECK(!playback.Receive(event, 1.));
    event = AvatarEvent(2, 1000, 10.f); event.channel = PartyChannel::HostWorld;
    CHECK(!playback.Receive(event, 1.));
    event = AvatarEvent(2, 1000, 10.f); event.payload.push_back(0);
    CHECK(!playback.Receive(event, 1.));
    event = AvatarEvent(2, 1000, 10.f); event.payload.resize(2);
    CHECK(!playback.Receive(event, 1.));
    MsgAvatarState wrongEpoch; wrongEpoch.epoch = 3;
    event = {2, 102, 4, PartyChannel::Avatar, PartyAvatarPacket(wrongEpoch), false};
    CHECK(!playback.Receive(event, 1.));
    MsgOutfit outfit; outfit.outfit.repositoryId = "guard";
    event = {2, 102, 4, PartyChannel::Avatar, PartyAvatarPacket(outfit), false};
    CHECK(!playback.Receive(event, 1.));
    playback.Tick(1.1);
    CHECK(sink.shown[2] == 0);
    CHECK(sink.posed[2] == 0);
    CHECK(sink.outfits[2].repositoryId.empty());
}

TEST(PartyPlayback_DuplicateOrReorderedPacketsCannotKeepAFrozenAvatarAlive) {
    AvatarSink sink;
    PartyAvatarPlayback playback(sink);
    REQUIRE(playback.UpdateRoster(Playing()));
    REQUIRE(playback.Receive(AvatarEvent(2, 1000, 20.f), 1.));
    REQUIRE(playback.Receive(PoseEvent(2, 1000, 2.f), 1.));
    playback.Tick(1.1);
    CHECK(sink.shown[2] == 1);
    CHECK(!playback.Receive(AvatarEvent(2, 1000, 99.f), 2.9));
    CHECK(!playback.Receive(AvatarEvent(2, 999, 99.f), 2.9));
    CHECK(!playback.Receive(PoseEvent(2, 999, 3.f), 2.9));
    playback.Tick(3.1);
    CHECK(sink.shown[2] == 1);
    CHECK(sink.hidden[2] == 1);
    REQUIRE(playback.Receive(AvatarEvent(2, 2000, 30.f), 3.2));
    playback.Tick(3.3);
    CHECK(sink.shown[2] == 2);
    CHECK(sink.transforms[2].pos.x == 30.f);
}

TEST(PartyPlayback_ReconnectAndReplayClearBufferedTransformsAndPoses) {
    AvatarSink sink;
    PartyAvatarPlayback playback(sink);
    REQUIRE(playback.UpdateRoster(Playing()));
    REQUIRE(playback.Receive(AvatarEvent(2, 1000, 20.f), 1.));
    REQUIRE(playback.Receive(PoseEvent(2, 1000, 2.f), 1.));
    playback.Tick(1.1);
    auto status = Playing(); status.revision = 11; status.members[1].incarnation = 900;
    REQUIRE(playback.UpdateRoster(status));
    playback.Tick(1.2);
    CHECK(sink.shown[2] == 1);
    CHECK(sink.posed[2] == 1);
    CHECK(!playback.Receive(AvatarEvent(2, 1100, 99.f), 1.2));
    auto fresh = AvatarEvent(2, 0, 40.f); fresh.incarnation = 900;
    REQUIRE(playback.Receive(fresh, 1.3));
    playback.Tick(1.4);
    CHECK(sink.transforms[2].pos.x == 40.f);
    status.epoch = 5; status.revision = 12;
    REQUIRE(playback.UpdateRoster(status));
    playback.Tick(1.5);
    CHECK(sink.shown[2] == 2);
    CHECK(!playback.Receive(fresh, 1.5));
}

TEST(PartyPlayback_ChangedSkeletonClearsOldPoseWithoutClearingOtherPlayers) {
    AvatarSink sink;
    PartyAvatarPlayback playback(sink);
    REQUIRE(playback.UpdateRoster(Playing()));
    for (uint8_t id = 2; id <= 3; ++id) {
        REQUIRE(playback.Receive(AvatarEvent(id, 1000, 20.f), 1.));
        REQUIRE(playback.Receive(PoseEvent(id, 1000, float(id)), 1.));
    }
    playback.Tick(1.1);
    MsgSkeleton skeleton; skeleton.parents = {-1, 0};
    REQUIRE(playback.Receive({2, 102, 4, PartyChannel::Avatar, PartyAvatarPacket(skeleton), true}, 1.2));
    playback.Tick(1.3);
    CHECK(sink.posed[2] == 1);
    CHECK(sink.posed[3] == 2);
    CHECK(!playback.Receive(PoseEvent(2, 999, 3.f), 1.3));
    REQUIRE(playback.Receive(PoseEvent(2, 1100, 6.f), 1.4));
    playback.Tick(1.5);
    CHECK(sink.posed[2] == 2);
    CHECK(sink.poses[2].rootPos.x == 6.f);
}

TEST(PartyPlayback_NativeSceneRefusalAndRoomClosureDoNotLeaveAnActiveScope) {
    AvatarSink sink;
    PartyAvatarPlayback playback(sink);
    sink.accept = false;
    CHECK(!playback.UpdateRoster(Playing()));
    CHECK(!playback.Receive(AvatarEvent(2, 1000, 20.f), 1.));
    sink.accept = true;
    REQUIRE(playback.UpdateRoster(Playing()));
    REQUIRE(playback.Receive(AvatarEvent(2, 1000, 20.f), 1.));
    auto status = Playing(); status.state = PartyRoomStatus::State::Closed;
    CHECK(!playback.UpdateRoster(status));
    CHECK(sink.hiddenAll == 1);
    CHECK(!playback.Receive(AvatarEvent(2, 1100, 99.f), 1.1));
    playback.Tick(1.2);
    CHECK(sink.shown[2] == 0);
}

namespace {
struct PreviewRoom {
    std::shared_ptr<SimNetwork> net=SimNetwork::Create(451);
    std::array<FakeWorld,4> sources {FakeWorld("Host"),FakeWorld("GuestA"),FakeWorld("GuestB"),FakeWorld("GuestC")};
    std::array<AvatarSink,4> sinks;
    std::array<std::unique_ptr<PartyPreviewRuntime>,4> runtime;
    double now=0;
    PreviewRoom() {
        const Endpoint host {0x0a000001,47047};
        for (size_t i=0;i<4;++i) {
            sources[i].inScene=true;
            sources[i].scene.scene="assembly:/_PRO/Scenes/Missions/TheFacility/_Scene_Mission_Polarbear_Module_002_B.entity";
            sources[i].player.pos.x=float(i)*10.f;
            sources[i].pose.rotations.resize(2);
            sources[i].pose.rootPos.x=float(i);
            sources[i].skeleton={-1,0};
            sources[i].outfit.repositoryId="outfit-"+std::to_string(i);
            PartyRoomConfig config; config.name="Agent"+std::to_string(i);
            auto room=i==0 ? PartyRoom::Host(net->Open(host),config) :
                PartyRoom::Join(net->Open({uint32_t(0x0a000001+i),uint16_t(50001+i)}),host,config,now);
            runtime[i]=std::make_unique<PartyPreviewRuntime>(std::move(room),sources[i],sinks[i]);
        }
    }
    void Step(double seconds) {
        const double end=now+seconds;
        while(now<end) {
            now+=1./60.; net->SetTime(now);
            for(auto& peer:runtime) peer->Tick(now);
        }
    }
    bool Start() {
        Step(2.);
        for(auto& peer:runtime) if(!peer->Ready()) return false;
        Step(0.5);
        if(!runtime[0]->Start()) return false;
        Step(1.);
        for(auto& peer:runtime) if(peer->Status().phase!=LobbyRoster::Phase::Playing) return false;
        return true;
    }
};
}

TEST(PartyPreview_FourRuntimePeersCaptureAndDeliverThreeDistinctAvatars) {
    PreviewRoom room;
    REQUIRE(room.Start());
    room.Step(1.5);
    for(size_t receiver=0;receiver<4;++receiver) {
        CHECK(room.runtime[receiver]->Status().members.size()==4);
        CHECK(room.runtime[receiver]->Accepted()>0);
        for(size_t sender=0;sender<4;++sender) {
            const uint8_t id=room.runtime[sender]->Status().self;
            if(receiver==sender) { CHECK(room.sinks[receiver].shown[id]==0); continue; }
            CHECK(room.sinks[receiver].shown[id]>0);
            CHECK(room.sinks[receiver].posed[id]>0);
            CHECK(room.sinks[receiver].transforms[id].pos.x==room.sources[sender].player.pos.x);
            CHECK(room.sinks[receiver].poses[id].rootPos.x==room.sources[sender].pose.rootPos.x);
            CHECK(room.sinks[receiver].outfits[id]==room.sources[sender].outfit);
            CHECK(room.sinks[receiver].skeletons[id]==room.sources[sender].skeleton);
        }
    }
    room.sources[1].outfit.repositoryId="changed-disguise";
    room.Step(1.5);
    const auto id=room.runtime[1]->Status().self;
    CHECK(room.sinks[0].outfits[id].repositoryId=="changed-disguise");
    CHECK(room.sinks[2].outfits[id].repositoryId=="changed-disguise");
    CHECK(room.sinks[3].outfits[id].repositoryId=="changed-disguise");
}

TEST(PartyPreview_OnlyActualFreeformReadinessAllowsTheNativeLoadedBarrier) {
    PreviewRoom room;
    room.sources[2].inScene=false;
    room.Step(2.);
    CHECK(!room.runtime[2]->Ready());
    for(size_t i:{size_t(0),size_t(1),size_t(3)}) REQUIRE(room.runtime[i]->Ready());
    room.Step(0.5);
    CHECK(!room.runtime[0]->Start());
    room.sources[2].inScene=true;
    room.sources[2].scene.scene="assembly:/_pro/scenes/missions/paris/scene.entity";
    CHECK(!room.runtime[2]->Ready());
    room.sources[2].scene.scene=room.sources[0].scene.scene;
    REQUIRE(room.runtime[2]->Ready());
    room.Step(0.5);
    // Native scene disappearing after ready must never be acknowledged as loaded.
    room.sources[2].inScene=false;
    REQUIRE(room.runtime[0]->Start());
    room.Step(1.);
    CHECK(room.runtime[0]->Status().phase!=LobbyRoster::Phase::Playing);
    CHECK(room.sinks[0].shown[room.runtime[2]->Status().self]==0);
}

TEST(PartyPreview_SceneClearInterruptsEveryPeerAndHostCanOpenANewEpoch) {
    PreviewRoom room;
    REQUIRE(room.Start());
    const auto oldEpoch=room.runtime[0]->Status().epoch;
    room.sources[2].inScene=false;
    room.runtime[2]->SceneCleared();
    room.Step(0.5);
    for(auto& peer:room.runtime) CHECK(peer->Status().phase==LobbyRoster::Phase::Interrupted);
    CHECK(!room.runtime[1]->Lobby());
    REQUIRE(room.runtime[0]->Lobby());
    room.Step(0.5);
    CHECK(room.runtime[0]->Status().epoch>oldEpoch);
    CHECK(!room.runtime[0]->Start());
    room.sources[2].inScene=true;
    for(auto& peer:room.runtime) REQUIRE(peer->Ready());
    room.Step(0.5);
    REQUIRE(room.runtime[0]->Start());
    room.Step(1.);
    for(auto& peer:room.runtime) CHECK(peer->Status().phase==LobbyRoster::Phase::Playing);
}

TEST(PartyPreview_HostClosureHidesGuestsWithoutFabricatingCompletion) {
    PreviewRoom room;
    REQUIRE(room.Start());
    room.runtime[0]->Leave(room.now);
    room.Step(17.);
    for(size_t i=1;i<4;++i) {
        CHECK(room.runtime[i]->Status().state==PartyRoomStatus::State::Failed);
        CHECK(room.sinks[i].hiddenAll>0);
        CHECK(room.runtime[i]->Status().phase!=LobbyRoster::Phase::Complete);
        CHECK(!room.runtime[i]->Ready());
    }
}

TEST(PartyPreview_LauncherCommandsRemainStrictAndEndpointBounded) {
    LauncherRequest request;
    for(const char* action:{"preview_status","preview_host","preview_leave","preview_ready","preview_start","preview_lobby","preview_reconnect"})
        CHECK(LauncherRequest::Parse(action,request));
    CHECK(LauncherRequest::Parse("preview_join \"example.test:47047\"",request));
    CHECK(!LauncherRequest::Parse("preview_join",request));
    CHECK(!LauncherRequest::Parse("preview_join \"host; calc\"",request));
    CHECK(!LauncherRequest::Parse("preview_host extra",request));
    CHECK(!LauncherRequest::Parse("preview_execute \"anything\"",request));
}


TEST(PartyPreview_DamagedStandInInterruptsAndStopStartCannotReuseBrokenScene) {
    for(size_t damaged:{size_t(0),size_t(2)}) {
        PreviewRoom room;
        REQUIRE(room.Start());
        room.sinks[damaged].fault="Remote stand-in damaged; restart Freeform Training.";
        room.Step(0.75);
        for(auto& peer:room.runtime) CHECK(peer->Status().phase==LobbyRoster::Phase::Interrupted);
        CHECK(!room.runtime[damaged]->NativeReady());
        CHECK(room.runtime[damaged]->Message()==room.sinks[damaged].fault);
        REQUIRE(room.runtime[0]->Lobby());
        room.Step(0.5);
        CHECK(!room.runtime[damaged]->Ready());
        CHECK(!room.runtime[damaged]->Start());
        CHECK(room.runtime[damaged]->Message()==room.sinks[damaged].fault);
        for(size_t i=0;i<4;++i) if(i!=damaged) REQUIRE(room.runtime[i]->Ready());
        room.Step(0.5);
        CHECK(!room.runtime[0]->Start());
        // Stop, scene callbacks and lobby changes cannot clear the sink's fault.
        room.runtime[damaged]->SceneCleared();
        CHECK(!room.runtime[damaged]->NativeReady());
        room.sinks[damaged].fault.clear(); // engine rebuilt the actor scene
        REQUIRE(room.runtime[damaged]->Ready());
        room.Step(0.5);
        REQUIRE(room.runtime[0]->Start());
        room.Step(1.);
        for(auto& peer:room.runtime) CHECK(peer->Status().phase==LobbyRoster::Phase::Playing);
    }
}
