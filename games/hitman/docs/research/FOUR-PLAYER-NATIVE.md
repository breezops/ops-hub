# Four-player native presentation migration

## Latest: Native runtime and preview launcher integrated

The native plugin frame loop and dedicated preview launcher now drive PartyPreviewRuntime/PartyRoom/PartyAvatarPlayback. Party protocol 2 adds reliable native SceneLost interruption. The first distributable is movement/disguise only, with manual Freeform loading and no four-player world interactions. All 161 automated tests, native build/import checks, named-pipe launcher checks and disposable installer/rollback/removal checks pass. Native real-PC playtesting is still required. See FOUR-PLAYER-PREVIEW-1.md; earlier milestones below are historical.


## Current milestone: per-player native adapter and playback

Current build: development-only four-player.dev.1. Installed Foundation 4.1 is unchanged.

Twelve native methods now take an explicit RemoteAvatarState: spawning, AI modifiers, outfit fallback/application, spawn waits, showing/hiding transforms, pose input, skeleton input, and rig resets/matching. Legacy wrappers retain the existing two-player entry points. PartyAvatars.cpp implements IPartyAvatarWorld using a persistent four-ID array (three remote IDs for any local player), the accepted room roster, connection incarnation and mission epoch. Source IDs, stale incarnations/epochs and unsupported native scenes are rejected before accessing an actor. Native presentation is limited to the exact Freeform Training scene. NPC listing, outfit observers and relevant selection paths exclude all owned stand-ins rather than only the legacy actor.

Each party actor uses its own PoseHook and CrashGuards entries. Native body/animator/skeleton changes trigger only that actor's rig rematch. Hidden actors finish their individual AI warmup and remain guarded. IDs reuse their scene-owned actor rather than deleting live NPCs on disconnect. Scene clearing unhooks all party targets, detaches unseen modifiers from their actors, clears the guard slots and forgets per-player scene references while retaining resource-backed puppet factories. The old walking-AI fallback is disabled for party actors; the first prototype requires pose streams.

A shared outfit-kit safety check now runs before temporary hero/actor variation edits, preventing the early-return path from leaving a shared kit modified. Rig diagnostics also bound mapping indices when the engine's native bone count changes.

PartyPresentationRoster validates roster identity/role/member bounds, phase, mission, loaded state and stale revision/epoch before exposing remote identities. It scopes one native scene to one room/local ID. PartyAvatarPlayback supplies typed avatar/pose/outfit/skeleton packets, strict channel/reliability checks, existing wire decoders and payload bounds, and separate interpolation buffers for each remote player. Old sender timestamps do not refresh stream freshness. Transform streams hide after two seconds without a fresh update and resume from fresh data rather than interpolating across the stalled gap. Changed skeletons clear only that player's old pose buffer. Reconnect/replay/closure clears the applicable buffered presentation.

Validation: 146 core/network/presentation tests, six direct animation-hook tests and four generated speech/guard tests pass (156 total). Four roster tests, six playback tests and an actual four-peer PartyRoom roster/event/disconnect integration test are new in this milestone. The playback tests use a fake native sink; they do not verify HITMAN spawning. The full isolated native project builds, and its DLL passes all 55 SDK import and ABI 1 checks. See checks/four-player-isolated-core-tests.txt, checks/four-player-native-build.txt, checks/four-player-native-dll-checks.txt, checks/four-player-pose-tests.txt and checks/four-player-guard-tests.txt.

Still required before installing a friend test: connect PartyRoom and PartyAvatarPlayback to the plugin's frame loop, capture/send local transforms/poses and reliable outfit/skeleton metadata, report actual native mission transitions, and expose an explicit preview mode through the launcher that excludes the legacy Session. The existing launcher still controls two-player Session. No four-player native playtest or gameplay installer has been issued. Global four-player NPC/interaction authority and native shared extraction remain unfinished. First runtime validation should check the new path on two real PCs, then the four-avatar movement/disguise/disconnect/restart test.

The earlier storage-only equivalence check below records the previous milestone. SdkWorld.cpp now has intentional parameterization and routing changes, so it is no longer a purely mechanical storage refactor.

## Verified baseline preserved

Foundation 4.1 / gameplay protocol 19 is preserved at:

- preserved/Foundation-4.1-Verified-2026-11-08
- R:/Hitman-Coop-Backups/Foundation-4.1-Verified-2026-11-08

Both copies include the exact working installer ZIP, its archived source ZIP, original build notes, payload hashes, manifest and restore instructions. Both archives are read-only. Every manifest hash was verified in both locations. The installer ZIP SHA256 is 988e9c38819e01a98297fffff34c7df7026aaaa132e2946659e926accf40c24f. These are two local copies; no off-machine backup is claimed. Game executables and saves are not part of these archives; restoring remains dependent on compatible HITMAN/SDK versions.

Four-player development now lives in dev/HitmanCoop-four-player with separate build outputs and a distinct four-player.dev.1 label. The sibling dev/HitmanCoop directory was left intact. Dependencies are referenced from its _deps folder. Installed Foundation 4.1 remains unchanged.

## Implemented adapter

PoseHook now supports room IDs 1..4 with at most three remote targets on a PC. Each target owns its native animator pointer, connection incarnation, pose, native-pose capture, rig mapping, bone mask and translation policy. Pose and rig updates commit together under the worker mutex. Rebinding resets all presentation state; repeated identical binding preserves it. Old-incarnation pose writes and removals fail. Animator lookup uses atomic rejection filters followed by a locked identity recheck. Cleanup detaches all player targets; uninstall clears them. Player targets cannot double-write an animator already controlled by the legacy target or an extra NPC/victim pose.

The existing two-player default target remains available during migration. The new API is not wired into SdkWorld yet. Only the validated current room roster may bind targets; packets do not directly bind native animator addresses. Scene/worker shutdown synchronization is still required before native entities are freed.

## Validation

Six adapter tests run the actual Windows vtable wrapper against synthetic animator output buffers. They cover three independent mappings/masks/translation policies, local source capture and native diagnostics, quaternion hemisphere handling, independent legacy behavior, remote capacity and duplicate-writer rejection, reconnect/stale-incarnation rejection, uninstall/reinstall cleanup, and concurrent worker callbacks during 2,000 bind/update/clear cycles. See checks/four-player-pose-tests.txt. These test hook behavior, not actual HITMAN avatar spawning or engine entity lifetime.

The isolated core/network copy also passes all 135 tests, including four real localhost UDP sockets. See checks/four-player-isolated-core-tests.txt.

## Remaining before a friend gameplay test

SdkWorld now groups 56 avatar-specific fields into RemoteAvatarState, including native actor references, outfit/rig/pose, movement, held-item slots, and the remote victim. The existing native methods still use a single legacy record. CrashGuards now has per-player/incarnation speech, knowledge and AI-unit adapters; the room/native adapter still needs to call them for actual spawned avatars. Restart guards continue retaining old actors for delayed engine teardown. These adapters are not a completed three-avatar runtime. NPC authority must be coordinated for the whole party. Then room admission/roster streams can replace the legacy session, and launcher controls can reflect actual four-player game state.

No four-player gameplay installer has been released or installed. Mission loading/shared native extraction remain separate unfinished work. The first four-player native test should validate three distinct avatars, independent movement/disguises, disconnect/reconnect and repeated mission restarts before broader interaction testing.

The complete isolated RelWithDebInfo development project builds successfully. DLL checks pass for SDK 4.2.1 / ABI 1, all 55 SDK imports, and Windows-only dependencies with no Visual C++ redistributable requirement. See checks/four-player-native-dll-checks.txt.

## Native guard and storage milestone (2026-11-08)

CrashGuards now registers up to three remote actors using IDs 1..4 and connection incarnations, alongside a separate legacy slot. Each player has independent knowledge and reasoning warmup/skip state. Updates/removals with a stale incarnation fail. Idempotent accepted roster updates keep knowledge intact. Disconnection clears live AI/speech entries but retains the actor in the existing restart history. The speech stub checks every fixed slot, including after empty slots; relative branch offsets are emitted with fixups. Ordinary voiced NPCs retain their original path, while ordinary no-voice NPCs take the silent path without counting as muted players. The legacy entry cannot take an actor already bound to a room player.

Four direct guard tests pass, including execution of the production-generated x64 speech stub against synthetic actor memory, all three simultaneous stand-ins, sparse slots after removal, ordinary NPC behavior, independent native AI warmup, puppet-list coexistence, reconnect/stale update rejection, and restart-history retention. No HITMAN process or executable is patched by these tests. Test-only seams are compiled only in coop_guard_tests, not in the mod DLL. See checks/four-player-guard-tests.txt.

RemoteAvatarState is noncopyable and nonmovable because it owns native references and resource-backed movement helpers. Scene factories, local-player capture, scene NPC ownership and shared world-item state remain on SdkWorld. The existing SdkWorld.cpp is identical to the previous implementation after undoing mechanical field qualification; all 56 field types/defaults were independently compared against the prior header. See checks/four-player-avatar-storage-check.txt. Native runtime methods still address m_LegacyAvatar; no three-avatar spawning claim is made for this storage refactor.

The complete isolated build succeeds (checks/four-player-native-build.txt), all 135 core/network tests plus six pose tests plus four guard tests pass (145 total), and the built DLL passes SDK 4.2.1 / ABI 1 and all 55 import checks. Foundation 4.1 remains installed and its preserved release is unchanged.

Next: parameterize the native spawn/outfit/rig/presentation methods by RemoteAvatarState, bind them to accepted room identities, and integrate roster-aware cleanup without deleting live NPC stand-ins. Then wire party streams and launcher state for an initial four-player movement/reconnect/restart test. Global NPC ownership and shared native extraction remain unfinished.
