# Reproduce the NPC discovery test

Use the Loop-6 build and packaging instructions with this profile. Add
`-DCOOP_PARTY_NPC_DISCOVERY=ON` and set the native build override to
`0.13.94-four-player.preview.4.discovery.1`.

Each PC scans the activated native actor list during the party's playing
phase. The scan uses the original beta's template entity IDs and reuses
the native slot and entity checks used by Catalog-3.1. The scan records
the current local construction stamp and slot only when a unique enabled
actor passes those checks. Targets use the same discovery path as other
NPCs. The records distinguish ambiguous, inactive, stale and untracked
actors. They do not constitute permission to control native actors.

Look for `npc-scan` in each `HitmanCoop.log`. Each snapshot starts with
the room, mission epoch, seat and record count. Use the last complete
snapshot per seat in each epoch and compare the set of matched IDs.
Construction stamps and native slots are local to each PC. Compare
those within a PC across the restart to identify reused or replaced
instances. A prior snapshot is never valid for a later mission epoch.
For the first Freeform run, check both the size of the common set and
the specific dispositions of escort and target IDs.

## Two-PC result

Both PCs recorded the same 99 template IDs and states before and after a
same-room restart. On each PC and in each epoch, 83 IDs had a unique
active actor with a verified slot and construction record, including the
mission target. The other 16 IDs were inactive. Neither scan reported
missing, ambiguous, stale or untracked IDs.

All 83 matched IDs kept their local actor slots, and every construction
stamp changed on each PC after restart. Retire previous bindings by
mission epoch and verify the new construction stamp before reusing a
slot. These logs establish matching and replacement for Freeform.
