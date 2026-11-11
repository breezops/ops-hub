# Two-player Freeform test on 0.1.31

Both PCs ran `npc-agreed.2` on HITMAN 3.290.1. The host and guest bound
the same 83 NPC IDs and confirmed all 83 initial baselines before and
after the host restarted. The host confirmed simulation resumed for
every NPC in both mission loads. Both PCs released the old binding scope
and rebound every ID to a newer construction stamp.

The tester saw corresponding NPC positions and noticed stiff movement
on a distant guest NPC. This build applies host positions and rotations
on the guest while the guest NPC AI is held. NPC pose streaming is off
at all distances. The log verifies the ownership and restart sequence.
Visual animation and smoothness need their own test.
