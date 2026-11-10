# Private Lifetime-3 (OPS0.1.15)

Retain exact birth-keyed destructor entry/body-return receipts. A late return
from an older actor never completes a replacement at the same address.
Pre-clear snapshots include live actors and already-pending destructor bodies.
Opaque cuts bind a unique book instance and prune epoch; foreign/rebuilt books
and pruned evidence are refused. Capacity exhaustion fails closed.
Stop detaches both callbacks before client destruction without retaining a
client pointer or lock across the native body. Shared Stats carry size tags.
Scene logs count per-birth captured/entered/body-returned actors separately
from observed old jobs. No body/global totals enable native retirement,
allocation free, worker barrier, birth pruning or multiplayer readiness.
No new native hooks, NPC controls, ClearScene wrapper or public update.
