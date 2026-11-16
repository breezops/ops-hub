# Reproduce the private binding restart test

Start with the scene-clear diagnostic profile, then enable
`COOP_NPC_RESTART_BINDING_DIAGNOSTICS` and set native identity
`0.13.94-four-player.preview.4.npc-rebind.1`.
Keep `COOP_MINHOOK_ABSOLUTE_ENTRIES=ON` and the existing job, identity,
lifetime, destruction, and clear-return observers. Disable the solo hold,
pool, and fixed catalog control profiles.

Build against SDK 4.2.1 for game 3.290.1. Package with
`tools/build-ops-release.py`, the verified NPC discovery package as its
runtime base, this candidate's config/instructions/changelog, and the
verified OPS 0.1.3 rollback ZIP. Include the tested
`Restore-Tested.ps1` and `RESTORE-0.1.3.cmd` from Lifetime-3 as extras.

The observation callback reuses `ListNpcs` to select the complete active
mission catalog. It checks each instance against the current slot and native
action view. Every ticket receives local ownership before being watched.
No native hold operations occur.

For each solo load, expect one `npc-bind` summary and individual ID/birth
records. Each restart should produce a `npc-pool` release for the previous
scope followed by a new catalog summary with fresh construction stamps.
Compare the independent `Return-callback evidence` records with the next
frame's evidence. The observations cover the NPCs admitted to the watched
catalog and preserve the distinction between metadata release and complete
native simulation retirement.
