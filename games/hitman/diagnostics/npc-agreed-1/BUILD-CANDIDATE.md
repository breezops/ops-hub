# Reproduce the agreed NPC catalog build

Use HITMAN 3.290.1 and ZHMModSDK 4.2.1. Set the native build identity
to `0.13.94-four-player.preview.4.npc-agreed.1`.
Configure a fresh build directory after finalizing the source. Preserve
the linker map with the build. Run a live launcher status request at
the main menu before publishing a multiplayer candidate.

```text
COOP_NPC_AGREED_CATALOG=ON
COOP_NPC_CATALOG=ON
COOP_NPC_POOL_DIAGNOSTICS=ON
COOP_NPC_HOLD_DIAGNOSTICS=ON
COOP_NPC_ACTION_DIAGNOSTICS=ON
COOP_NPC_IDENTITY_DIAGNOSTICS=ON
COOP_NPC_DESTRUCTION_DIAGNOSTICS=ON
COOP_NPC_LIFETIME_DIAGNOSTICS=ON
COOP_NPC_CLEAR_RETURN_DIAGNOSTICS=ON
COOP_PARTY_NATIVE_RESTART=ON
COOP_PARTY_INITIAL_LOAD=ON
COOP_PARTY_GUEST_START=ON
COOP_PARTY_NPC_DISCOVERY=ON
COOP_PARTY_CONTRACT_TRACE=ON
COOP_MINHOOK_ABSOLUTE_ENTRIES=ON
```

Package with `tools/build-ops-release.py`, the verified NPC discovery
package as runtime base, this directory's configuration and instructions,
and the preserved 0.1.3 rollback ZIP and Lifetime-3 restore scripts.

During the live test, compare the agreed roster and exact birth stamps
on each PC. Look for the guest's baseline confirmations, the host's
simulation confirmations, and the scoped retirement record after restart.
