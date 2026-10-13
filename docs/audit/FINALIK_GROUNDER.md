# Installed FinalIK grounder

October 13, 2026. **Shipped-artifact-derived and implemented in a disconnected
C owner.** `pe_grounder` performs one `GrounderFBBIK.OnSolverUpdate` for the
female player: the root ray, both legs, the pelvis, and the pelvis, foot and
spine requests handed to [the full-body solver](FINALIK_FBBIK.md). No host
calls it. Nothing here has been compared with the original on uneven ground.

## Evidence

Like the solver, the grounder is compiled into the installed
`Assembly-CSharp.dll` and absent from the checked-in source. The saved REA
inventory `ev_13b79477fb6033537478536807957a9437d4a82d8da7d9d87b9ef1ef27ee7243`
already held it; no new inventory, assembly execution or game launch was
needed.

[The committed excerpt](../reference/finalik-grounder-il.json) keeps 77 method
bodies and 60 fields: `Grounder`, `GrounderFBBIK`, its `SpineEffector`,
`Grounding`, its nested `Leg` and `Pelvis`, plus `Interp.LerpValue` and
`V3Tools.LineToPlane`. `export_grounder.py` binds every body to the installed
bytes, pins the method, field and serialized-settings digests, and regenerates
[the settings](../../rebuild/generated/pe_grounder_source.json).

The female component is MonoBehaviour 11400014 on `FemaleModel/IK`, bound to
the solver component by its `ik` reference.

| Setting | Serialized female value |
|---|---|
| weight | 1 |
| quality / rotateSolver | Simple (three rays per foot) / off (world up) |
| maxStep / heightOffset | 0.4 / 0 |
| footSpeed / footRadius / prediction | 2.5 / 0.1 / 0.05 |
| footRotationWeight / Speed / maxFootRotationAngle | 1 / 10 / 26.7° |
| pelvisSpeed / pelvisDamper | 2 / 0.01 |
| lowerPelvisWeight / liftPelvisWeight | 0.2 / 0.8 |
| spineBend / spineSpeed | 4.5 / 2 |
| spine effectors | both shoulders, horizontal 0.8, vertical 0; seven other entries zero |
| layers | mask 620801 (bits 0, 8, 11–14, 16, 19); layer names not resolved |
| root / pelvis / feet | `Model` / `Bip01` / the two foot effector bones |

## Update

`GrounderFBBIK.Update` clamps the weight and arms one solve per frame.
`OnSolverUpdate` is the last `OnPreUpdate` subscriber (observed order: after
the Halt/hit effects and `InteractionSystem`). When enabled with positive
weight:

1. **Root ray.** Down from one `maxStep` above the root, up to eleven steps.
   The root is grounded when the hit is nearer than two steps. A miss counts
   as not grounded.
2. **Each leg.** Three rays down from one step above the foot, reaching three
   steps: at the foot, ahead by `footRadius` plus predicted motion, and to the
   side by half `footRadius`. Their hit points give a plane.
3. **Pelvis.** Its height moves toward `-(largest leg offset) * 0.2 -
   (smallest leg offset) * 0.8`.
4. **Requests.** The pelvis bone is moved directly. Each foot effector gets
   the offset from the moved foot to its target, and the foot bone is rotated
   directly. The shoulders get the spine lean.

Things the code does that a guess would get wrong:

- **Height is the root's height over the ground under each foot**, not the
  foot's. The foot's own height cancels out of `GetHeightFromGround`. On flat
  ground with the root at ground level the grounder therefore requests
  nothing, however the animation lifts the feet. This matches the observed
  tutorial session, where its share stayed under a millimetre.
- **The foot offset is interpolated twice a frame**: `Interp.LerpValue` (a
  constant 2.5 m/s) and then `Mathf.Lerp` toward the same target.
- **The leg-height clamp only limits lifting.** A foot can be raised by at
  most `maxStep` less its own height above the root; reaching down is limited
  only by `maxStep`.
- **A foot over a drop deeper than `maxStep` still reaches the full step** but
  stops counting as grounded. A ray that hits nothing leaves the foot at a
  default point two steps below it.
- **The pelvis is a transform write, not an effector.** Moving it also moves
  the feet, so the foot offset is measured from the foot after the move. In a
  steady state over ground below the root, the pelvis drops by the whole gap
  and the feet are requested back to where the animation put them.
- **The plane normal is limited per frame to 26.7° from vertical**, and the
  foot rotation then eases toward it at ten per second.
- **The spine lean scales with distance.** Each leg contributes its horizontal
  vector from the root, times its offset length, times a facing factor; the
  sum is multiplied by 4.5 and reaches the shoulders at 0.8.
- **Elapsed time starts wrong.** `OnEnable` stores `Time.deltaTime` as the
  last time, so the first update measures from near zero.
- **Initiate forces both leg mappings to maintain rotation 1**, the value the
  prefab already serializes.

## Implemented in C

[pe_grounder](../../rebuild/src/pe_grounder.h) takes time, the root position
and axes, the two foot positions and a ray callback, and returns the pelvis
offset, two foot offsets and rotations, and nine effector offsets. It refuses
any mode other than the serialized one at export time. Invalid or nonfinite
input, or a nonfinite result, fails without changing state or output. Nothing
is saved.

Adapters and omissions, none of which are original behavior:

- **Rays are a caller callback.** The original casts against Unity colliders
  on the serialized layers; what the rebuild's terrain returns is the caller's
  concern.
- `Quaternion.FromToRotation`, `Slerp`, `SlerpUnclamped` and
  `Vector3.RotateTowards` are engine natives; C uses conventional forms.
  `Quaternion.RotateTowards`, `Mathf.Lerp`, `LerpValue` and `LineToPlane` are
  managed and follow the compiled bodies.
- `Leg.r` starts as the default zero quaternion in the original; C starts it
  at identity. What the engine's `Slerp` does from a zero quaternion is
  unknown.
- The foot offset assumes the pelvis move translates the feet rigidly.
- Fastest and Best quality, `rotateSolver`, `heightOffset` and
  `OnPreGrounder`/`OnPostGrounder` subscribers are not implemented.

## Verification

The core suite checks the female settings; flat ground (no request, seven rays
a frame); the one-frame arithmetic and the steady state over lower ground; a
0.2 m step (pelvis −0.04, feet −0.16 and +0.04, shoulder lean at 0.8); half
weight; a drop deeper than a step; ground above the root; 10° and 40° slopes;
no ground; the pelvis damper; the first-update and repeated-time cases;
disabled and zero weight; and atomic failure on invalid input.

`test_grounder_export.py` holds a second transcription in the original's
object shape and double precision. Across 40 scenarios (2,240 updates over
rolling terrain, pits with no ground, hovering roots, varying frame times,
repeated times, weights and disabling) it agrees with C to 4e-7 m and 5e-8 in
quaternion dot. Both transcriptions share one author and the same native
adapters, so this guards transcription, not engine arithmetic or collision.

```sh
python3 rebuild/tools/run_tests.py -k grounder
make -C rebuild grounder-source-check
```

## Remaining boundary

- **No uneven-ground observation.** The one observed session was on flat
  ground. A supervised capture on a slope or steps, recording the grounder's
  rays and outputs, would qualify the adapters and the ray conventions.
- **Collision.** Which rebuild surfaces correspond to the serialized layer
  mask is undecided, and the layer names are unresolved.
- **Host composition** is not started, as for the solver.

Live reactions, stun/knockback, get-up, shield, pounce/aggression and Town
Defense gates stay closed. PECSv22 is untouched.
