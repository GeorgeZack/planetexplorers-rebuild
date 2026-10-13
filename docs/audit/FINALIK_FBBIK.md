# Installed FinalIK full-body biped solver

October 12, 2026. **Shipped-artifact-derived and implemented in a disconnected
C owner.** `pe_fbbik` performs one `IKSolverFullBody.OnUpdate` for the female
rig: pose read, chain solve and spine/limb mapping. No host calls it. It is the
solve that [Halt](HALT_STOP_EFFECT.md) and [hit IK](HIT_IK.md) offsets were
waiting for, but it supplies no visible reaction: the engine's quaternion
natives are conventional adapters. One
observed session agrees with the original solver to 0.29 mm.

## Evidence

FinalIK is not in the checked-in source; its classes are compiled into the
installed `Assembly-CSharp.dll`. The complete saved REA inventory
`ev_13b79477fb6033537478536807957a9437d4a82d8da7d9d87b9ef1ef27ee7243` already
held them, so no new inventory, assembly execution or game launch was needed.

[The committed excerpt](../reference/finalik-fbbik-il.json) keeps 229 method
bodies and 164 fields of the update path, selected by declaring-type token
because nested types (`Node`, `BoneMap`, `ChildConstraint`) have bare names in
the inventory. It covers `FullBodyBipedIK`, `IK`, `SolverManager`,
`IKSolver`, `IKSolverFullBody`, `IKSolverFullBodyBiped`, `FBIKChain`,
`IKEffector`, `IKConstraintBend`, the three mappings, `QuaTools`, `V3Tools`
and `IKSolverFABRIK.SolveJoint`. `export_fbbik.py` binds every body to the
installed bytes and pins the method and field digests.

The female component is readable by name in
`Assets/Resources/Prefab/PlayerPrefab/FemaleModel.prefab` (MonoBehaviour
11400016). Its script GUID has no checked-in MonoScript; the class identity
rests on GrounderFBBIK's `ik` reference to it and on the serialized field
layout matching the installed class. The exporter pins the whole serialized
solver and regenerates [the rig metadata](../../rebuild/generated/pe_fbbik_source.json)
and C rig.

| Setting | Serialized female value |
|---|---|
| iterations | 1 |
| Chains | body: Spine2; arms: UpperArm/Forearm/Hand; legs: Thigh/Calf/Foot |
| pull / pin / push / reach | 1 / 0 / 0 / 0.1 on every chain |
| Child constraints | L arm–R leg and R arm–L leg (pull elasticity 1), arm–arm and leg–leg (rigid) |
| Effectors | nine, every position, rotation and maintain weight 0; no targets |
| Spine mapping | Bip01, Spine2, Spine3; 3 iterations; twist weight 1 |
| Limb mappings | arms with clavicle parents, maintain rotation 0; legs without, maintain rotation 1 |
| Head mapping | maintain rotation 0 |
| spineStiffness / pullBodyVertical / pullBodyHorizontal | 0.5 / 0.5 / 0 |
| timeStep / fixTransforms | 0 / on |

All effector weights are zero, so in this rig the solver acts only on
`positionOffset` requests. The solve hierarchy is 25 transforms: 24 from
`FemaleModel` to the hands and feet, plus the solver root `IK`; serialized scales are one to within
1e-6 and the C rig treats them as one.

## Update order

`SolverManager.LateUpdate` calls the solver every frame when `timeStep` is 0;
`Update` first restores the stored default local pose because `fixTransforms`
is set. `IKSolver.Update` raises `OnPreUpdate`, where
`IKOffsetModifier` (the Halt and hit owner) writes its offsets, then:

1. `IKPositionWeight <= 0` zeroes every `positionOffset` and returns.
2. **ReadPose.** `PullBody` adds a body offset from out-of-reach weighted hand
   targets. Spine stiffness rewrites the push elasticity of the two diagonal
   constraints to `clamp01(1 - spineStiffness)`. Each limb's bend constraint
   runs `LimitBend` with clamp 0.505. Effectors convert offsets into node
   offsets; the body effector also offsets its two thigh child nodes. Chains
   read node positions and lengths; mappings record the animated plane frames.
3. **Solve**, once per iteration: end effectors, `Push`, `Reach`, other
   effectors, a trigonometric pass, `Stage1`, other effectors, `Stage2` with
   `clamp(iterations, 2, 4)` constraint passes. Then end effectors again, and
   a trigonometric pass using the bend constraints.
4. **WritePose.** Spine mapping, head mapping, then per limb: clavicle swing,
   upper bone pinned to its node, the two plane rotations, maintained end
   rotation and effector rotation.
5. Every `positionOffset` is zeroed.

Things the code does that a guess would get wrong:

- **End effectors hold their animated place.** Hands and feet are reset to
  their animated position plus their own offset before each iteration and
  again before the bend pass. A body offset therefore bends the knees over
  planted feet rather than translating the character.
- **A body offset alone lands only partly.** The shoulders carry no offset
  and the arm chains pull at full weight. In the test pose a requested 8 cm
  drop moves the body about 4.7 cm. Offsetting the shoulders and hands as well
  lands the full 8 cm. This is why the Halt effect drives seven effectors.
- **Limbs never straighten fully.** The knee or elbow is placed for 99.9% of
  the limb length even when the target is out of reach.
- **Only three spine bones are written.** Bip01 is positioned and oriented
  from the thigh plane, Spine2 swings and twists, Spine3 is placed from the
  shoulder plane. Pelvis, Spine and Spine1 follow rigidly.
- **Feet keep their animated rotation; hands follow the forearm.**

## Implemented in C

[pe_fbbik](../../rebuild/src/pe_fbbik.h) takes the animated local pose, the
solver weight and nine effector inputs, and returns the new local pose. It
models the transform hierarchy itself. `pe_fbbik_init` takes the pose at
`Initiate`, which fixes each bend constraint's default directions and each
clavicle's swing axis. Invalid or nonfinite input, or a nonfinite result,
fails without changing the output. Nothing is saved.

Adapters and omissions, none of which are original behavior:

- `LookRotation`, `FromToRotation`, `Lerp`, `Inverse`, `Vector3.Slerp` and
  `OrthoNormalize` are engine natives absent from managed code. C uses
  conventional float forms. The managed thresholds (vector equality,
  `normalized`, `Project`, quaternion equality, `Angle`) were read from the
  installed UnityEngine inventory.
- Written rotations are renormalized, because a conjugate stands in for the
  inverse.
- `PullBody` measures arm length from the current pose; the original reads the
  lengths left by the previous update. Bones are rigid, so these agree.
- In the `iterations == 0` path the original keeps a stale bend rotation
  offset; C uses identity. The female rig never takes that path.
- Bend goals, effector targets, `FixTransforms` and the solver delegates are
  caller concerns.

## Verification

The core suite checks the female bindings; that zero offsets reproduce a bent
pose to 0.1 mm; the zero-weight early return; partial and full body offsets
with planted feet; half solver weight; hand offsets; position and rotation
pins; `PullBody`; the 99.9% limb limit; the offset-only path; rigid segment
lengths after every solve; a head hit from `pe_hit_ik` fed through the solver;
and atomic failure on invalid input or a malformed rig.

`test_fbbik_export.py` holds a second transcription of the compiled update,
written in the original's object shape and in double precision. Across 84
random cases (jittered poses, offsets, position/rotation/maintain weights,
bend weights, one and two iterations, the offset-only path, non-rest
initiation poses) it agrees with C to 0.2 mm and 2e-6 in quaternion dot on
every bone. It found two slips in the first C draft: unnormalized written
rotations, and the stale offset in the offset-only path. Both transcriptions
share one author and the same native adapters, so this guards transcription,
not engine arithmetic.

```sh
python3 rebuild/tools/run_tests.py -k fbbik
make -C rebuild fbbik-source-check
```

Full results are in [PLAYTEST](../PLAYTEST.md).

## Engine comparison

**Observed in the original, October 12, 2026.** One supervised tutorial
session with the female player, on the isolated bake profile with the
launch-only shim. The operator stood, walked, ran and stopped sharply for 120
seconds; no input was injected.

`ReferenceFbbikProbe.cs` attached to the live player's `FullBodyBipedIK`. It
added two delegate subscribers and removed them at the end; it wrote no solver
field, transform, input or save. Each frame it recorded, at `OnPreRead` (after
every `OnPreUpdate` offset writer, before the pose read), the solver weight,
nine effectors and the local bone transforms; and at `OnPostUpdate` the same
transforms plus the 13 node positions. `analyze_reference_fbbik.py` replayed
every frame through `pe_fbbik` and compared with the original's own output.

| Measure over 1,399 frames | Value |
|---|---|
| Solver failures | 0 |
| Bone position error | max 0.29 mm, mean 0.003 mm |
| Bone rotation error | max 0.18°, mean 0.002° |
| Node position error | max 0.29 mm |
| Original IK displacement being reproduced | max 0.67 m, mean 2.1 cm |

So the conventional native adapters reproduce the engine's solve to well under
a millimetre on real grounder and Halt inputs. This is agreement on observed
frames, not bit-for-bit engine arithmetic.

What the session also settled:

- **Topology and settings match the prefab**: chains, constraints, effector
  bones, spine, limbs, head, iterations 1, limb mapping weights 1.
- **Offset writer order**: `IKAnimEffectCtrl.ModifyOffset`, then
  `InteractionSystem.OnPreFBBIK`, then `GrounderFBBIK.OnSolverUpdate`.
- **The solver root is the component's own object**, `FemaleModel/IK`, a
  childless sibling of `FemaleModel/Model`. `IK.InitiateSolver` passes its own
  transform to `IKSolver.Initiate`, which overwrites the serialized `Model`
  reference; the capture exposed this and the compiled body confirms it. The
  exporter first used the serialized reference and now derives the owner
  transform. Only `PullBody` reads the root, and it was inactive (see below),
  so the measured errors are the same before and after.
- **Solver weight was 1 and every effector weight and bend weight 0** on every
  frame. Position/rotation pins, `PullBody` and bend goals were therefore not
  exercised by the original and remain checked only against the second
  transcription.
- **The idle solve is small but not zero.** In the 1,082 frames whose summed
  offsets stayed under 2 mm, the original's solve still moved bones by up to
  3.3 mm (mean 0.4 mm).
- **Initiation pose is undetermined but immaterial here.** Replays with the
  observed bend defaults and with prefab-rest initiation gave identical
  results.
- **The large inputs are the Halt effect.** Shoulder offsets reached 11.6 m
  for a few frames while the solved displacement stayed under 0.67 m.
  [`pe_halt` reproduces them](HALT_STOP_EFFECT.md#observed-comparison) to
  2 mm across thirteen stops, so offsets and solve are now both observed for
  a stop on flat ground.

The first launch recorded nothing: the probe searched under the model
controller's object and the solver is on its sibling. The capture above is the
second launch. Raw captures stay in the local profile
(`pose-captures/female-fbbik-02.jsonl`) and are not committed.

```sh
python3 rebuild/tools/reference_fbbik_probe.py          # offline compile only
python3 rebuild/tools/analyze_reference_fbbik.py CAPTURE.jsonl
```

The analyzer's own tests feed it a reordered synthetic capture; they establish
that the tool works, not what the original does.

## Remaining boundary

- **Unexercised solver paths.** Effector position/rotation weights, `PullBody`
  and bend goals were zero throughout the observed session.
- **GrounderFBBIK** (component 11400014, enabled, weight 1) writes foot, pelvis
  and spine offsets into the same solver from raycasts each update. It is now
  [recovered separately](FINALIK_GROUNDER.md) into `pe_grounder`, unobserved on
  uneven ground. It subscribes after `IKAnimEffectCtrl` and
  `InteractionSystem` (observed).
- **Host composition.** [Animated solver poses](PLAYER_IK_POSE.md) for the 31
  staged player clips now exist and solve cleanly, with per-bone motions from
  `pe_ik_skin`. Driving each rendered part's skin joints, defence capsules
  and attachments from them, and choosing the pose to read during a blend,
  are not started. Two AimIK components on the
  same model run separately.

Live reactions, stun/knockback, get-up, shield, pounce/aggression and Town
Defense gates stay closed. The Halt Move/Jump blocking decision is unchanged.
PECSv22 is untouched.
