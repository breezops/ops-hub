# OPS candidate 0.1.4 — native identity check

Private read-only diagnostics for HITMAN 3.290.1. Not published to the updater.

- Stamp native actor construction through the SDK's existing constructor hook.
- Validate live actor-list identity, runtime slot and construction generation.
- Log a bounded identity audit in Freeform Training, including after restart.
- Keep shared-NPC control and the new hold/resume hooks disabled.
- Preserve the movement/disguise preview and use the existing tracked installer.
- Bundle the original, verified OPS 0.1.3 ZIP and a restoration shortcut.

The native build is `0.13.94-four-player.preview.4.identity.1`; the party envelope
remains protocol 2. This candidate is intended for a solo identity check. All PCs
would need the same native build for movement-preview networking.

Source validation: 266 automated tests, full SDK compilation and SDK ABI checks.
Live actor identity and constructor coverage have not yet been validated in-game.
No game files, saves or secrets are included. Existing upstream credits/licenses
remain included. Planned host mission following and all-players pause remain
future work.
