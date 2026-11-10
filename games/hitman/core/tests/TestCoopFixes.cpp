#include "TestFramework.h"
#include "coop/CoopFixes.h"
#include "coop/BundleSync.h"
#include "coop/LauncherProtocol.h"
#include "coop/LobbyRoster.h"
#include "coop/LauncherPolicy.h"

using namespace coop;

TEST(Clothes_FirstObservationAlreadyStrippedIsAnnounced) {
    BodyClothingState body;
    CHECK(body.Observe(false));
    CHECK(body.taken);
    CHECK(!body.Observe(false));
}

TEST(Clothes_RemoteDuplicateAndLateDressedStateCannotRestorePickup) {
    BodyClothingState body;
    CHECK(!body.Observe(true));
    body.AcceptPeer(true);
    CHECK(body.taken);
    body.AcceptPeer(false);
    CHECK(body.taken);
    CHECK(!body.Observe(false));
}

TEST(Clothes_LocalTransitionAndSceneReset) {
    BodyClothingState body;
    CHECK(!body.Observe(true));
    CHECK(body.Observe(false));
    CHECK(!body.Observe(true));
    CHECK(body.taken);
    body = {};
    CHECK(!body.taken);
    CHECK(!body.Observe(true));
}

TEST(TeamExit_RoundTripAndNearbyOnly) {
    TeamExitRequest sent { 4, "assembly:/scene.entity", "9ae01aaa-f19f-4c70-abc6-9fb4dc46edf0", {1,2,3} }, got;
    CHECK(TeamExitRequest::Decode(sent.Encode(), got));
    CHECK(got.Matches(4, sent.scene));
    CHECK(got.Near({1,2,3}));
    CHECK(!got.Near({20,2,3}));
    CHECK(!got.Matches(5, sent.scene));
    CHECK(!got.Matches(4, "assembly:/another.entity"));
}

TEST(TeamExit_RejectMalformedAndNonfiniteRequests) {
    TeamExitRequest value;
    CHECK(!TeamExitRequest::Decode("0 scene 9ae01aaa-f19f-4c70-abc6-9fb4dc46edf0 1 2 3", value));
    CHECK(!TeamExitRequest::Decode("1 scene invalid 1 2 3", value));
    CHECK(!TeamExitRequest::Decode("1 scene 9ae01aaa-f19f-4c70-abc6-9fb4dc46edf0 nan 2 3", value));
    CHECK(!TeamExitRequest::Decode("1 scene 9ae01aaa-f19f-4c70-abc6-9fb4dc46edf0 1 2 3 extra", value));
}

TEST(Clothes_NativeNudeBitOverridesClothAvailability) {
    BodyClothingState body;
    CHECK(!body.Observe(BodyHasClothes(true, false)));
    CHECK(body.Observe(BodyHasClothes(true, true)));
    CHECK(body.taken);
    CHECK(!BodyHasClothes(false, false));
    CHECK(!body.Observe(BodyHasClothes(true, false)));
}

TEST(Victim_SettledBodyStreamsWithoutAnimationBones) {
    CHECK(CanStreamVictim(true, 0));
    CHECK(CanStreamVictim(true, 1));
    CHECK(!CanStreamVictim(false, 0));
    CHECK(!CanStreamVictim(false, 1));
    CHECK(CanStreamVictim(false, 2));
}

TEST(Bundle_RoundTripIncludesActualOutfitAndScope) {
    BundleMessage sent;
    sent.epoch = 7;
    sent.scene = "assembly:/training/scene.entity";
    sent.id = (uint64_t(2) << 56) | 4;
    sent.outfit = "054f443b-824f-4913-8b29-64dfcd82b089";
    sent.charset = 2;
    sent.variation = 1;
    sent.modifiers = true;
    sent.xf.pos = {1.25f, -5.5f, 0.75f};
    BundleMessage got;
    REQUIRE(BundleMessage::Decode(sent.Encode(true), true, got));
    CHECK(got.id == sent.id && got.outfit == sent.outfit);
    CHECK(got.charset == 2 && got.variation == 1 && got.modifiers);
    CHECK(Distance(got.xf.pos, sent.xf.pos) < 0.00001f);
    CHECK(got.Matches(7, sent.scene));
    CHECK(!got.Matches(8, sent.scene));
    CHECK(!got.Matches(7, "another mission"));
    REQUIRE(BundleMessage::Decode(sent.Encode(false), false, got));
    CHECK(got.id == sent.id);
    CHECK(!BundleMessage::Decode(sent.Encode(true), false, got));
}

TEST(Bundle_RejectsMalformedNativeCreationArguments) {
    BundleMessage sent, got;
    sent.epoch = 1;
    sent.scene = "training";
    sent.id = (uint64_t(1) << 56) | 1;
    sent.outfit = "054f443b-824f-4913-8b29-64dfcd82b089";
    CHECK(BundleMessage::Decode(sent.Encode(true), true, got));
    sent.charset = -1;
    CHECK(!BundleMessage::Decode(sent.Encode(true), true, got));
    sent.charset = 0; sent.variation = 256;
    CHECK(!BundleMessage::Decode(sent.Encode(true), true, got));
    sent.variation = 0; sent.xf.rot = {0,0,0,0};
    CHECK(!BundleMessage::Decode(sent.Encode(true), true, got));
    sent.xf.rot = {}; sent.xf.pos.x = 100001.f;
    CHECK(!BundleMessage::Decode(sent.Encode(true), true, got));
    sent.xf.pos.x = 0; sent.outfit = "not-a-guid";
    CHECK(!BundleMessage::Decode(sent.Encode(true), true, got));
    sent.outfit = "054f443b-824f-4913-8b29-64dfcd82b089"; sent.id = 77;
    CHECK(!BundleMessage::Decode(sent.Encode(true), true, got));
    sent.id = (uint64_t(2) << 56) | 1;
    CHECK(!BundleMessage::Decode(sent.Encode(true) + " trailing", true, got));
}

TEST(Bundle_ConsumptionAndDuplicateSpawnNeverResurrectBag) {
    BundleLedger ledger;
    CHECK(ledger.CanSpawn(17));
    ledger.Spawned(17);
    CHECK(!ledger.CanSpawn(17));
    ledger.Removed(17);
    CHECK(!ledger.CanSpawn(17));
    ledger.Removed(18); // remove delivered before spawn
    CHECK(!ledger.CanSpawn(18));
    CHECK(!ledger.CanSpawn(0));
    ledger = {}; // fresh mission owns fresh identities
    CHECK(ledger.CanSpawn(17));
}

TEST(Drag_TargetMotionRejectsTeleportsAndStaleIntervals) {
    CHECK(Distance(DragTargetVelocity({}, {0.1f,0,0}, 0.1), {1,0,0}) < 0.001f);
    CHECK(Length(DragTargetVelocity({}, {100,0,0}, 0.1)) == 0.f);
    CHECK(Length(DragTargetVelocity({}, {0.1f,0,0}, 0.5)) == 0.f);
    CHECK(Length(DragTargetVelocity({}, {0.1f,0,0}, 0)) == 0.f);
    CHECK(Length(DragTargetVelocity({}, {0,0,5}, 0.1)) == 0.f);
}

TEST(Drag_FeedForwardReducesSteadyMotionLagAndStopsWhenReleased) {
    float oldPosition = 0, newPosition = 0;
    for (int i = 0; i < 600; ++i) {
        const float target = float(i) / 60.f;
        oldPosition += DragWantedVelocity({target-oldPosition,0,0}, {}, 3.5f).x / 60.f;
        newPosition += DragWantedVelocity({target-newPosition,0,0}, {1,0,0}, 3.5f).x / 60.f;
    }
    CHECK(std::fabs(10.f-newPosition) < std::fabs(10.f-oldPosition) * 0.25f);
    CHECK(Length(DragWantedVelocity({100,100,0}, {4,4,0}, 3.5f)) <= 3.501f);
    CHECK(Length(DragWantedVelocity({}, {}, 3.5f)) == 0.f);
}

TEST(Launcher_OnlyBoundedLocalActionsAndAddressesAreAccepted) {
    LauncherRequest request;
    CHECK(LauncherRequest::Parse("status", request));
    CHECK(LauncherRequest::Parse("host", request));
    CHECK(LauncherRequest::Parse("join \"192.0.2.1:47047\"", request));
    CHECK(request.value == "192.0.2.1:47047");
    CHECK(LauncherRequest::Parse("replay", request));
    CHECK(!LauncherRequest::Parse("exec powershell", request));
    CHECK(!LauncherRequest::Parse("host extra", request));
    CHECK(!LauncherRequest::Parse("join", request));
    CHECK(!LauncherRequest::Parse("join \"\"", request));
    CHECK(!LauncherRequest::Parse("join \"a;b\"", request));
    CHECK(!LauncherRequest::Parse(std::string(600, 'a'), request));
    CHECK(!LauncherRequest::Parse(std::string("host\0extra", 10), request));
}

TEST(Lobby_FourSlotsStableIdentityAndNoFifthPlayer) {
    LobbyRoster lobby("host");
    CHECK(lobby.Join("a") == 2); CHECK(lobby.Join("b") == 3); CHECK(lobby.Join("c") == 4);
    CHECK(lobby.Join("b") == 3); CHECK(lobby.Join("d") == 0); CHECK(lobby.Count() == 4);
    CHECK(lobby.Leave(3)); CHECK(lobby.Join("d") == 3);
}
TEST(Lobby_AllFourMustBeReadyAndLoadedBeforePlaying) {
    LobbyRoster lobby("host"); lobby.Join("a"); lobby.Join("b"); lobby.Join("c");
    REQUIRE(lobby.Select("freeform"));
    for (uint8_t i=1;i<=3;++i) CHECK(lobby.Ready(i,lobby.epoch,true,true));
    CHECK(!lobby.BeginLoad()); CHECK(!lobby.Ready(4,lobby.epoch-1,true,true));
    CHECK(lobby.Ready(4,lobby.epoch,false,true)); CHECK(!lobby.BeginLoad());
    CHECK(lobby.Ready(4,lobby.epoch,true,true)); REQUIRE(lobby.BeginLoad());
    CHECK(lobby.Join("late") == 0);
    for(uint8_t i=1;i<=3;++i) CHECK(lobby.Loaded(i,lobby.epoch));
    CHECK(lobby.phase == LobbyRoster::Phase::Loading);
    CHECK(lobby.Loaded(4,lobby.epoch)); CHECK(lobby.phase == LobbyRoster::Phase::Playing);
}
TEST(Lobby_FourNativeCompletionAcknowledgementsAndReplayEpoch) {
    LobbyRoster lobby("host"); lobby.Join("a"); lobby.Join("b"); lobby.Join("c");
    REQUIRE(lobby.Select("freeform")); const auto old = lobby.epoch;
    for(uint8_t i=1;i<=4;++i) lobby.Ready(i,old,true,true);
    REQUIRE(lobby.BeginLoad()); for(uint8_t i=1;i<=4;++i) lobby.Loaded(i,old);
    for(uint8_t i=1;i<=3;++i) CHECK(lobby.Completed(i,old));
    CHECK(lobby.phase == LobbyRoster::Phase::Completing); CHECK(!lobby.ReturnToLobby());
    CHECK(lobby.Completed(4,old)); REQUIRE(lobby.ReturnToLobby()); REQUIRE(lobby.Select("freeform"));
    CHECK(!lobby.Ready(2,old,true,true)); CHECK(!lobby.Completed(2,old)); CHECK(!lobby.BeginLoad());
}
TEST(Lobby_DisconnectCannotProduceFalseSharedVictory) {
    LobbyRoster lobby("host"); lobby.Join("a"); REQUIRE(lobby.Select("freeform"));
    lobby.Ready(1,lobby.epoch,true,true); lobby.Ready(2,lobby.epoch,true,true); REQUIRE(lobby.BeginLoad());
    lobby.Loaded(1,lobby.epoch); lobby.Loaded(2,lobby.epoch); lobby.Completed(1,lobby.epoch);
    CHECK(lobby.Leave(2)); CHECK(lobby.phase == LobbyRoster::Phase::Interrupted);
    CHECK(!lobby.Completed(1,lobby.epoch)); CHECK(lobby.ReturnToLobby());
}

TEST(Launcher_FreeformSceneRecognitionIgnoresGamePathCase) {
    CHECK(IsFreeformTrainingScene("assembly:/_PRO/Scenes/Missions/TheFacility/_Scene_Mission_Polarbear_Module_002_B.entity"));
    CHECK(IsFreeformTrainingScene("assembly:/_pro/scenes/missions/thefacility/_scene_mission_polarbear_module_002_b.entity"));
    CHECK(!IsFreeformTrainingScene("assembly:/_pro/scenes/missions/thefacility/_scene_mission_polarbear_module_001.entity"));
    CHECK(!IsFreeformTrainingScene("assembly:/_pro/scenes/missions/thefacility/_scene_mission_polarbear_module_002_b.entity.other"));
}
TEST(Launcher_ReadyRequestReplyAndExpiryAreDistinct) {
    CHECK(std::string(LauncherReadyState(true,100,90,-100,-100,-100)) == "waiting");
    CHECK(std::string(LauncherReadyState(true,100,-100,90,-100,-100)) == "incoming");
    CHECK(std::string(LauncherReadyState(true,145,-100,90,-100,-100)) == "incoming");
    CHECK(std::string(LauncherReadyState(true,151,90,-100,-100,-100)) == "expired");
    CHECK(std::string(LauncherReadyState(true,100,-100,-100,95,-100)) == "acknowledged");
    CHECK(std::string(LauncherReadyState(true,100,-100,-100,-100,95)) == "replied");
    CHECK(std::string(LauncherReadyState(false,100,90,90,95,95)) == "unavailable");
}
TEST(Launcher_FocusLossPausePolicyPreservesFocusedEscAndOtherMenus) {
    CHECK(BlockLauncherFocusPause(false,true,true));
    CHECK(!BlockLauncherFocusPause(true,true,true));
    CHECK(!BlockLauncherFocusPause(false,false,true));
    CHECK(!BlockLauncherFocusPause(false,true,false));
}
