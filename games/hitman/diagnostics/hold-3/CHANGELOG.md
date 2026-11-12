# OPS 0.1.12 / hold.3 - verified absolute entry backend

- Address the confirmed Hold-2 attachment refusal: its running process had no
  suitably aligned near buffer location even within two GiB of any verified
  entry. No VirtualAlloc or hook-table allocation was attempted by that search.
- Add a private opt-in x64 backend that allocates trampoline pages anywhere and
  patches entries with a fourteen-byte register-preserving absolute jump.
- Reuse MinHook's thread suspension/IP mapping, bookkeeping and instruction
  decoder. Copy whole instructions, back up/restore fourteen overwritten bytes,
  and refuse short/hot-patch functions or relative/RIP-dependent stolen code.
- Admit only the verified job/action/actor-control entry prefixes. The original
  fourteen vendor files stay unchanged; a documented deterministic script
  generates the modified build-local C sources and their hash audit.
- Test a trampoline more than two GiB away with the entire nearby four-GiB
  arena reserved, verify original execution and restoration, and reject an
  unapproved entry without modifying it. Run existing control/refusal tests in
  both backend modes; standard mode remains the default.
- Preserve Hold-2 NPC selection, native ownership/completion checks, the solo
  foreground F8 cycle and exact public 0.1.3 rollback. No live hold success yet.
