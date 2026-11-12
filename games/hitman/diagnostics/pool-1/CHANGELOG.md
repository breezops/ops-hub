# Private Pool-1 (OPS0.1.16)

Reuse retained action bridge, native SetHeld, identity/slot/job readback and
mutation gate for a solo two-NPC hold/drain/resume/restart-job cycle.
Add immutable fixed-scope ticket watches to the retained native hold observer;
track ownership/interference independently per actor, preserve game calls,
refuse stale lease/birth, retired/reused address, scope changes and capacity
exhaustion. Legacy single-actor mode remains covered by its existing tests.
Native destructor entry invalidates matching pool watches. Owner permissions
are required before resuming; observed interference refuses further controller
calls. No mutex spans yielding native control. Does not establish every hold
writer/cross-thread ordering, full producer closure or NPC lifetime pinning.
This is not whole-scene preparation, retirement proof or shared NPC gameplay.
