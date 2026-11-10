# Cooperative session candidate (unpublished)

This development source adds authenticated optional cooperative events for native
NPC conversation entry, text reveal/advance/end and limited interaction/sequence
entry. It also transmits the equipped backpack mesh transform for a decorative
remote mesh. It does not yet replicate inventory choices, all quest state,
checkpoints or every cinematic transition; no full-campaign synchronization
claim is made.

Pause code disables focus auto-pause during co-op, observes explicit native pause
input, combines the two pause votes and displays a partner-paused screen notice.
Loading screens are excluded from pause/menu recovery. Physical Escape/Alt-Tab
and notice rendering still require gameplay verification.

UE4SS 3.0.1 repeatedly crashed during earlier isolated runs. Singleton references
now resolve fresh wrappers and polling modules share one bounded game-thread
queue. This reduces competing asynchronous work but does not establish that the
upstream Lua concurrency issue is fixed.

Validation: 58 automated tests pass, including actual authenticated loopback
transport, malformed/stale cooperative payloads, shared pause behavior and the
single-worker scheduler. Native test profile coop-scheduler-smoke2 created remote
backpack actors on both peers without reported cooperative errors. Its Guardian
fixture did not enter dialogue on both peers. A later polling fallback detects
confirmed running dialogue when native entry bypasses reflected hooks; retest
required. No release manifest was changed and no launcher update was published.

Next gate: repeat Guardian entry and text advance initiated from each side,
verify equal dialogue completion and controllable gameplay, visually verify both
backpacks, and test Escape plus focus loss. Keep all tests in isolated UserDir
profiles and preserve normal user settings/saves.

Further native probe: coop-dialogue-native-start also failed the both-dialogues
running gate. Its development fixture could not bind the Guardian dialogue to
the local cat; public StartDialog is guarded by that binding and was not forced.
A normal player-initiated conversation is needed to separate fixture setup from
receiver lifecycle problems. The scheduler smoke ran beyond the previous
65–90 second crash interval without child exit, but is not a stability guarantee.

Pause-loop correction: remove repeated unsolicited-menu dismissal and unowned
native pause recovery. Focus loss is handled by AutoPauseEnabled=false only.
Only explicit native pause input and connected peer votes request shared pause.
Engine pause changes retry at most three times, 500 ms apart, per intent change.
Regression exercises 300 polls for untouched native menus and 300 failed pause
polls (exactly three calls). All 58 tests pass. Physical Escape/Alt-Tab behavior
remains unverified; this candidate is not published to the Stray updater.
