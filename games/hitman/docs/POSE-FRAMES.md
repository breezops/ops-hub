# Animation reference frames — Preview 4

The two-PC Preview 3 test passed outfit continuity and session lifecycle. Both
mouse-led turns and WASD circles while crouched showed lateral gliding with the
remote body facing forward. Transform rotation already travels through the
network and interpolation. Missing animation orientation is a candidate cause;
the real game's visual result remains unverified until the next playtest.

Read-only inspection of the installed HITMAN 3.290.1 binary identified
ZMorphemeEntity's IBoneAnimator interface (SDK offset 0x90), update vtable slot 1.
In that build the update routine is at image-relative 0xFDDF70.
The fourth argument is read as four 16-byte matrix rows by the native pose
postprocessor. The existing hook passed it through but captured only bones.
No game bytes, executable patches, or disassembly are distributed here.

The hook now captures the validated world rotation and bone array together
under its mutex. Source changes clear both. The pose message uses its count's
high bit for an optional packed quaternion after the normal bone data. Ordinary
messages retain their previous encoding. Exact native-build room negotiation
rejects old/new mixed builds before pose traffic. NPC/legacy poses have no frame
extension. Party envelope protocol stays 2.

The receiving worker reads its own native animation world frame. For Hamilton
quaternions, with S = source world, T = receiver world and R = source root:

    correctedRoot = inverse(T) * S * R
    T * correctedRoot = S * R

Only mapped root 0 copying source root 0 is rebased. Its parent-relative offset
uses the same frame difference; pelvis and other bones remain parent-relative.
Global position remains the interpolated player transform. The extra native
trajectory node is deliberately not integrated a second time. No movement
velocity or camera heading is substituted for body orientation.

World reads use VirtualQuery and ReadProcessMemory, refuse guard/unreadable
pages and reject non-affine, nonfinite, reflected, degenerate or sheared frames.
Invalid/missing frame metadata retains the prior presentation. Rig-count,
incarnation, masking and visibility protections remain active. Every ten seconds
per active avatar the log reports source metadata, rebased/fallback counts and
the latest correction angle; no screen or input capture is involved.

Automated checks cover DirectX row-matrix conventions and all quaternion
conversion branches, positive scales, wrap at 180 degrees, old/extended/truncated
messages, missing metadata, real Windows hook buffers with three independently
oriented remote rigs, root world invariance, untouched child bones, stale rig
counts, unreadable/guarded frames, source rebind and uninstall.

## Two-PC acceptance test

Update both closed games to OPS 0.1.3 / Preview 4. Load Freeform Training,
host/join, ready and start. Take turns observing:

1. Crouch and circle with WASD while holding the camera still.
2. Crouch and move in a circle while turning the camera with the mouse.
3. Strafe left/right, then backpedal, standing and crouched. The body should
   follow the actual local animation, including intentional sideways travel.
4. Turn at rest, walk, sprint and vault near the teammate.
5. Change outfits repeatedly; verify no blackout or twisted/frozen pose.
6. Stop, ready and start; repeat one circle in each control style.

Compare both directions. If turning is still wrong, save both logs after exit;
frame counters distinguish absent metadata from an applied conversion whose
engine-space convention needs further investigation. Two PCs test this adapter;
a later four-PC pass still needs to confirm simultaneous presentation/lifecycle.
