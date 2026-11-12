# OPS 0.1.5 / action.1 — private solo diagnostic

- Observe the verified native action loop with a pinned MinHook 1.3.4 bridge.
- Validate the actual scheduled runtime slots, actor-list lock, stable IDs,
  constructor births, native actor pointers, flags and action/task/job pointers.
- Detach client callbacks before the mod world is destroyed. Keep native
  execution and trampoline memory alive until process exit.
- Retain identity audits and verified original OPS 0.1.3 restoration package.

This candidate has not been validated live. It does not enable shared NPC
control, NPC freezes, unanimous pause or the intended persistent mission loop.
Read-only observations are not worker-drain or physical-retirement receipts.
