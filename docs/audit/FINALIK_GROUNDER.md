# Installed FinalIK grounder

October 13, 2026. **Shipped-artifact-derived and implemented in a disconnected
C owner.** `pe_grounder` performs one `GrounderFBBIK.OnSolverUpdate` for the
female player: the root ray, both legs, the pelvis, and the pelvis, foot and
spine requests handed to [the full-body solver](FINALIK_FBBIK.md). No host
calls it. One observed session on natural terrain
[agrees with it](#observed-on-uneven-ground) to 0.2 mm and 0.23°, with two
settings the game rewrites while playing fitted rather than recorded.

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
| layers | mask 620801: Default, AIPlayer, SceneStatic, VFVoxelTerrain, Building, TreeStatic, Unwalkable, GIEProductLayer (bits 0, 8, 11–14, 16, 19) |
| root / pelvis / feet | `Model` / `Bip01` / the two foot effector bones |

The layer names are source-derived: `Pathea.Layer` in
`WhiteCat/PE-VC/Layer.cs` and the 32 names in `ProjectSettings/TagManager.asset`
agree. The mask leaves out Player (10) and Ragdoll (15), so the grounder's rays
pass through the character's own colliders and land on terrain, buildings,
static scenery and trees.

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

## Prepared uneven-ground capture

The flat tutorial session could not qualify the grounder, so a second
supervised session was prepared and then [run](#observed-on-uneven-ground).
Its tools:

- `ReferenceGrounderProbe.cs` attaches to the live player's `GrounderFBBIK`.
  It adds one subscriber each to the grounder's own `OnPreGrounder` and
  `OnPostGrounder` delegates, which the compiled `OnSolverUpdate` raises
  immediately before `Grounding.Update` and after the last spine request, and
  removes them at the end. It writes no grounder or solver field, transform,
  input or save.
- **Before each update** it records time, weight, the root position and axes,
  both foot positions and rotations, the pelvis bone, the nine effector
  offsets already written by Halt and other effects, and the private leg and
  pelvis state the update starts from.
- **It repeats the grounder's seven rays** as plain `Physics.Raycast` queries
  on the same layers, at the same instant, and records origin, hit point,
  normal, distance, layer and collider type. The original keeps only its root
  and heel hits; the analyzer checks the probe's repeats against those two.
- **After each update** it records the leg and pelvis state, the moved pelvis
  bone, the rotated feet and the effector offsets, so the grounder's share is
  the difference from the "before" values.
- `analyze_reference_grounder.py` replays the capture through `pe_grounder`
  twice: once restarting every frame from the original's recorded state (one
  update's arithmetic, no drift), and once free-running from the first frame.
  It reports errors for foot offset and target, height from ground, foot and
  leg rotation, pelvis height and bone move, foot and spine effector requests,
  and ray origins. It also reports live settings against the prefab, the live
  layer names, what was hit, and whether the capture left flat ground at all
  (a foot request of 3 cm or more).

What the session should settle, in the order it matters:

1. **Ray conventions**: origins, lengths and miss defaults.
2. **Foot offset, pelvis height and spine lean** on slopes and steps.
3. **The four engine-native adapters** (`FromToRotation`, `Slerp`,
   `SlerpUnclamped`, `RotateTowards`) and the zero starting quaternion of
   `Leg.r`, through the recorded foot rotations.
4. **The rigid-translation assumption** for the foot after the pelvis move.
5. **What the rays actually hit** in play, by layer and collider type.

Session, about two minutes of play, operator input only: load a place with
natural terrain (the Adventure start has slopes; the tutorial is flat), arm,
then stand still on a slope, walk and run up, down and across it, step on and
off a ledge or rock, and jump once. The launcher defaults to the isolated bake
profile; `--user-data` selects another, and the preserved sandbox reference
checkpoints must not be overwritten.

```sh
python3 rebuild/tools/reference_grounder_probe.py          # offline compile only
python3 rebuild/tools/reference_grounder_probe.py --launch # supervised session
python3 rebuild/tools/analyze_reference_grounder.py CAPTURE.jsonl
make -C rebuild grounder-probe-test
```

The analyzer's tests write a capture from the second transcription over
rolling ground, and check that both replays agree with it, that a flat capture
is reported as not qualifying, and that changed state, requests, rays,
settings and modes are visible or rejected. They establish that the tools
work, not what the original does.

## Observed on uneven ground

**Observed in the original, October 13, 2026.** One supervised Adventure
session with the female player on the isolated bake profile, launch-only shim,
single-threaded rendering. The operator armed after loading and supplied every
input on sloped terrain for 120 seconds; no input was injected. The capture
closed at its time limit with 1,453 frames (`pose-captures/female-grounder-02.jsonl`,
SHA-256 `5cdf64e150d1f1660cf7288ecc78d995e3857b8fbec98688bcde35f4e6c02ed4`,
kept in the local profile and not committed).

The first launch aborted by itself just after the world loaded, before
arming, and recorded only its header. It was a segfault in generated code
followed by Mono's abort, which resembles the
[earlier graphics faults](RUNNING_KNIFE_LIVE_OBSERVER.md#first-launch-pre-arm-graphics-crash);
the core was not opened, so the cause is not established. Its log is kept
beside the capture.

Coverage: foot requests up to the 0.4 m step limit (mean 8.5 cm), pelvis
moves up to 24.9 cm, foot tilts up to 37.5°, 288 frames with the root ray
missing and 115 frames ungrounded. Every frame cast all seven rays.

**Two settings are not constants.** The session showed it and the source
explains it:

- `Motion_Move_Human` writes `solver.maxFootRotationAngle` every update from
  its serialized `m_ForwardAngleFootRotate` curve evaluated at the physics
  controller's `forwardGroundAngle`. The prefab's 26.7° is only the value
  before the first write; 17.26° was live at attach.
- `IKCmpt` sets `spineBend` to its prefab value (4.5) while its spine mask is
  empty and to 0 otherwise. `Action_Tool` and `HeavyEquipmentCtrl` fill the
  mask. It was 0 at attach.

The probe recorded both only at attach, so the analyzer fits them one update
at a time: the spine bend as 0 or 4.5 from the spine requests, the rotation
limit from the leg rotations. The fit chose no bend on 1,121 frames and 4.5 on
332; the limit ranged up to 38° and was pinned to within 1° on 1,326 frames
(a frame pins it only while a foot is clamped). Replaying with the prefab
constants instead left foot rotations wrong by up to 21.7° and spine requests
by up to 7.5 cm. The probe now records both per frame, and the analyzer uses
recorded values when present.

| Measure over 1,453 frames, one update restarted from the original's state | Max |
|---|---|
| Ray origins against the original's | 0 |
| Probe's repeated root / heel hits against the hits the original kept | 0 / 0.011 mm |
| Foot offset, height from ground, pelvis height and damper | under 1e-7 |
| Foot target position | 0.16 mm |
| Pelvis bone move | 0.019 mm |
| Foot effector request | 0.015 mm |
| Spine offset and the other effector requests (fitted bend) | 0.003 mm |
| Leg and foot rotation (fitted limit) | 0.23° |
| Grounded-flag differences, failed frames, ray-count differences | 0 |

Left running from the first frame on recorded inputs, the figures are the
same: nothing drifts over the two minutes.

What this settles:

1. **Ray conventions**: origins, lengths, order and miss defaults.
2. **Foot offset, pelvis height and damping** on slopes, steps and in the air.
3. **The foot is a rigid translation of the pelvis move** to 0.02 mm.
4. **The native rotation adapters and the zero starting `Leg.r`**, to the
   resolution of the fitted limit. A capture with the limit recorded would
   tighten this.
5. **The rays hit only voxel terrain**: 8,938 hits, all layer 12
   `VFVoxelTerrain`, all `MeshCollider`. Nothing else was underfoot, so
   buildings, scenery and trees are still unobserved.
6. `OnPreGrounder` and `OnPostGrounder` had no other subscribers.

Not settled by this capture: the ground angle on each frame, which actions
held the spine mask, and `IKCmpt`'s companion `enableArmMap`, which zeroes the
solver's arm mapping weights under the same mask and so changes
[the solve](FINALIK_FBBIK.md) too.

## Settings written while playing

**Source-derived and implemented in C; the curve's evaluation is unobserved.**

| Setting | Writer | Value |
|---|---|---|
| `maxFootRotationAngle` | `Motion_Move_Human`, every update | `m_ForwardAngleFootRotate.Evaluate(forwardGroundAngle)`; angle 0 without a physics controller |
| `spineBend` | `IKCmpt.enableGrounderSpineEffect` | 4.5 while the spine mask is empty, else 0 |

The curve is serialized on `Motion_Move_Human` (component 11400024) of
`EntityPlayer.prefab`, five keys with clamped ends:

| Ground angle | −90° | −60° | 0° | 60° | 90° |
|---|---|---|---|---|---|
| Limit | 40° | 30° | 5° | 30° | 40° |

So feet may tilt only 5° on level ground and up to 40° on a wall-steep slope.
`export_grounder.py` pins the keys and tangents, the component's script GUID,
the single assignment in `Motion_Move_Human.cs` and the `IKCmpt.cs` setter,
and writes them to [the metadata](../../rebuild/generated/pe_grounder_source.json)
and header. `pe_grounder_foot_rotation_limit` evaluates the curve;
`pe_grounder_set_live` writes both settings into the grounder before an
update, as the original mutates its component.

- **The ground angle** is `HumanPhyCtrl.forwardGroundAngle`, already
  [recovered](HUMAN_PHY_CTRL.md) in the disconnected `pe_human_phy` from
  contact normals. No host produces contact normals yet.
- **The spine mask** is filled by `Action_Tool` and `HeavyEquipmentCtrl`
  through `SetSpineEffectDeactiveState`, and evaluated once when the view is
  built. Which player actions those are is not traced here.
- **`AnimationCurve.Evaluate` is native.** C uses the Hermite form over the
  keys, checked against an exact rational evaluation, not against the engine.
- **Script order** between `HumanPhyCtrl.Update` and `Motion_Move_Human`'s
  write is not established, so the limit may lag the angle by a frame.

Consistency with the observed session, which did not record the angle: the
limits fitted on the 1,326 pinned frames span 5° to 38°, inside the curve's 5°
to 40°, with the lower end exactly the level-ground value. The 17.26° live at
attach corresponds to a ground angle of about ±28.8°. This does not test the
curve's shape. The probe now records the ground angle and the arm mapping
weight per frame, and the analyzer then reports the curve's error directly.

## Remaining boundary

- **Inputs to the written settings.** [The curve and both writers](#settings-written-while-playing)
  are recovered, but a host must supply the ground angle (contact normals)
  and the spine mask, and the arm mapping weights follow the same mask. The
  curve's evaluation and the limit itself are unobserved: the session's limit
  is fitted.
- **Only terrain was underfoot** in the observed session.
- **Collision.** The layer names are resolved, but which rebuild surfaces
  stand in for them is undecided. Terrain, buildings and static scenery are
  the plain reading; whether player-built blocks and tree capsules should
  catch foot rays waits on what the capture shows the rays hitting.
- **Host composition** is not started, as for the solver.

Live reactions, stun/knockback, get-up, shield, pounce/aggression and Town
Defense gates stay closed. PECSv22 is untouched.
