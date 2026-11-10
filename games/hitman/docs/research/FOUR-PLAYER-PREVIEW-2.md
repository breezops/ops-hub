# Four-player movement preview 2

Build: 0.13.94-four-player.preview.2. Party protocol 2. HITMAN 3.290.1 / ZHMModSDK 4.2.1 ABI 1. All players need this exact build. This is the movement/disguise preview, with four slots; shared NPCs, bodies, items, objectives and extraction are still absent.

## Report and changes

The three-player Preview 1 report describes an accidental takedown while vaulting beside a teammate. The victim remained a moving/flickering corpse on one PC. Stop/Start hid and reused that native actor. The three saved logs show actor reuse and subsequent skeleton-read fallback; they do not independently identify the exact initiating interaction.

Preview 2 adds a pattern-resolved native combat-picker guard for registered remote actors only. It registers protection and sets native block-close-combat/block-drag bits before activation, and refreshes those bits while stopped/hidden. Normal NPC candidates replay the original instructions, including flags. Other guards and player capacity remain unchanged. If this guard cannot install, the preview refuses to host/join or ready, with an explicit launcher error.

If an activated stand-in is dead or pacified after its initial two-second setup, the native adapter latches a scene fault, logs the player ID/incarnation and flags, hides the avatars and stops pose presentation. The runtime sends a reliable scene-loss interruption. Ready and Start remain blocked on that PC across Stop/Start and reconnect until a native scene clear resets the fault. The launcher displays the mission-restart instruction on its periodic status poll. No raw resurrection, actor deletion or growing replacement-actor pool is used.

Recovery: use HITMAN Restart Mission on each affected PC. The host stops the interrupted preview, then everyone readies and starts again. A healthy Stop/Start continues to reuse healthy stand-ins normally.

## Validation

152 core/network/runtime tests, 6 generated PoseHook tests and 5 generated guard tests pass (163 total). The interaction regression executes the production x64 stub against synthetic actors, covering three sparse player slots, legacy actor protection, ordinary NPC fallback, preservation of native comparison flags, stale removal and clearing. Runtime tests cover native faults on host and guest, reliable party interruption, refusal to reuse the broken scene after Stop/Start, and recovery after scene reset. Existing localhost transport tests pass.

Full native build succeeds. All 55 SDK imports resolve, ABI 1 matches and no Visual C++ redistributable is required. Read-only signature/disassembly checks find a unique matching picker site in the installed 3.290.1 executable and verify both rejection branches agree. These automated checks do not prove the original in-game vault/takedown case is fixed; that is the next real-PC test.

Evidence: checks/preview-2-{build,core-tests,pose-tests,guard-tests,dll-checks,bridge-tests,installer-tests}.txt, preview-2-picker-pattern.json and preview-2-picker-disassembly.txt. Original test logs are preserved under checks/playtest-07-three-player-preview-1. Build/package work does not change the installed game or the frozen Preview 1/Foundation 4.1 archives.

## Player test

Close HITMAN and launchers on all PCs. Extract the entire Preview 2 ZIP into a fresh folder; START-HERE.cmd choice 1 installs/updates; choice 4 opens the current launcher. Use two or three players first. Load Freeform Training on every PC, host/join, ready, start. Walk/crouch/vault beside and toward each other from both directions. Check no teammate Subdue/Eliminate prompt appears. Confirm normal NPC interactions remain available. Stop/ready/start twice. Test a guest leaving, reloading Freeform, rejoining, readying and starting again. Save each PC's log if a remote actor falls, flickers, or the game crashes. All users keep the same build.

The installer preserves names, pins quiet/auto-update settings, includes tracked removal and exact Foundation 4.1 rollback. Original co-op by Jordan Rodrigues (GPL-3.0); ZHMModSDK by OrfeasZ and contributors. Corresponding modified source and original licenses are included.
