# OPS 0.1.10 / hold.1 - private one-guard native hold/resume

- Reuse NpcActionPhase's checked control operation from the existing retained
  action entry. No stacked custom loop hook. Release observer/session mutexes
  before cancellation; atomically retain the client context until control ends.
- Preserve default observation behavior. Optional hold controls require a
  separately enabled private SDK flag, solo play and an explicit foreground F8.
- Select one nearby conscious guard with fresh constructor identity after a
  native restart, scheduled slot validation and one observed active native job.
- Confirm agreed native held flags, completed job return and cleared task/action/
  job pointers before an eight-second hold; resume only the controller's hold
  and verify that a new native job starts. Log each transition and refusal.
- Watch the verified native hold setter with a pinned pass-through observer.
  Distinguish our single marked entry from external/recursive script calls;
  refuse further controller changes after observed game/script intervention.
- Add retained fiber cancellation/resume, wrong-stack/identity, shutdown,
  external hold, recursive hold and integrated arbitration fixtures.
- Keep report/configuration layout linker tags, clean build procedure, exact
  tested 0.1.3 rollback and unchanged public downloads.

This is an untested-in-game local diagnostic. It does not acknowledge full
native retirement, all hold sources/cross-thread ordering, guest readiness or
shared NPC authority. Startup attachment must be confirmed before pressing F8.
