# Private shared NPC movement test (OPS 0.1.30)

The native adapter prepares the active NPC roster agreed by the room.
It verifies each actor's construction stamp and native job, holds the
actor, and confirms that job has drained before applying the host's
transform. The host waits for each guest's baseline receipt before
resuming that NPC.

Host-led restart releases old bindings after the matching scene-clear
return, observed job drain, and watched destructor returns. New NPC
instances are validated against the new room agreement.

This candidate covers living NPC transforms in Freeform Training.
Animation replication, damage, disguises taken from NPCs, and mission
objectives still need their own multiplayer validation.
