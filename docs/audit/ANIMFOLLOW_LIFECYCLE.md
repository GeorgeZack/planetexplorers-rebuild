# Installed AnimFollow lifecycle and scalar requests

October 12, 2026. Recovered offline from shipped `Assembly-CSharp.dll` and the
installed female prefab; implemented in disconnected `pe_animfollow`. No game
or target assembly was executed. No host calls this owner. It supplies state
and requests, not a ragdoll, PD solve, bone pose or integration acceptance.

## Evidence and inspection boundary

The reverse-engineer-anything skill's managed-artifact workflow was used.
Connected REA identity inspection is evidence
`ev_28ec0aeead49bbd1e5dd9005ab19e7643b544db3ea89a67a447b65468c40b10d`:
Assembly-CSharp.dll SHA-256
`56d90949e732331be7644fc9185dc088c78ad4888a100f60d96b7aa093f4d7d3`,
MVID `7ddcc464-5353-4750-b969-fe628ee474da`, complete static metadata coverage.
An earlier firstpass inventory contains neither base class; the matching
complete saved Assembly-CSharp member inventory was reused rather than
re-inspected. That inventory's source Evidence ID is retained per assembly in
[the bounded CIL excerpt](../reference/animfollow-lifecycle-il.json).

The excerpt binds all RagdollControl_AF, AnimFollow_AF, HashIDs and wrapper
methods, the view getter and SimpleFootIK's drive callers to raw installed
bytes. Tokens are build-local, not portable IDs. Principal anchors:

| Method | Token | Relevant CIL |
|---|---|---|
| AnimFollow_AF.FixedUpdate | 0x06000020 | calls DoAnimFollow |
| AnimFollow_AF.DoAnimFollow | 0x06000017 | 0x000c–0x0022: controller before active check; 0x0023 onwards: error reset and drive |
| RagdollControl_AF.DoRagdollControl | 0x0600002b | 0x00f3–0x0332: fall entry; 0x0348–0x07a7: get-up; 0x07a7–0x0aee: falling; 0x0aee onwards: contact control |
| RagdollControl_AF.get_active | 0x0600002d | forwards to AnimFollow_AF.get_active |
| AnimFollow_AF.get_active | 0x0600001e | returns mActive |
| AnimFollow_AF.set_active | 0x0600001d | changed-value admission, SyncTransforms, renderer/body policy |
| HashIDs static initializer | 0x0600000b | exact state, transition and signal strings |

These are observations of bytes and static control flow, not observations of
running Unity, PhysX or Animator behavior. The C reconstruction is derived
from that control flow; it is not bit-exact Mono qualification.

## Installed serialized settings

`mainData`'s resource `prefab/playerprefab/femalemodel` points to
`resources.assets` GameObject 47841. Its descendants include AnimFollow_AF
324352 and PERagdollController 324629. The installed stripped MonoBehaviour
trees expose only their 32-byte common header. The exporter reads the remaining
fields in pinned metadata order, including SerializeField attributes and
excluding static, readonly and nonserialized fields. It validates both whole
payload hashes and complete consumption: 2,496 and 564 bytes respectively.
Script names, enabled state, resource binding and all participating asset
hashes are checked. The serialized layout and pointer identities are retained
in generated JSON, with logical installed-source labels.

Constructor defaults alone would give the wrong female behavior:

| Setting | Constructor | Installed female |
|---|---:|---:|
| get-up angular drag / drag | 50 / 25 | 35 / 17 |
| residual torque / joint torque | 0 / 120 | 0.04 / 80 |
| master fall Animator factor | 0.4 | 0.25 |
| early / late get-up Animator factors | 0.25 / 0.65 | 0.35 / 0.75 |
| contact torque / force / joint torque | 1 / 1 / 3 | 4 / 3 / 5 |
| wake-up strength | 0.2 | 0.5 |
| maximum matching error | 0.1 | 0.3 m |

Both use settledSpeed 0.2 m/s, fallLerp 1.5, getupLerp1 0.15, getupLerp2 2,
and contact/noncontact lerps 70/1. The female's original follow drag/angular
drag are 0.5/1 and secondaryUpdate is 2. Awake snapshots those follow values;
it does not use the controller's serialized localDrag/localAngularDrag fields.
Initial drive caps are torque/force/joint torque 100/100/10000.

## Active and body policy

The view's IsRagdoll reads **mActive** through the base getter. It does not read
the wrapper's m_RagdollActive, which OnFallBegin/OnGetupFinished manage.
Activation can make the view active before the first fall callback.

The active setter runs only on a changed value. It updates BoneCollector
ragdoll admission if present, synchronizes transforms, notifies the wrapper
renderer handler if present, then updates every slave Rigidbody/collider.
Normal bodies are kinematic when inactive. RagdollLock-tagged bodies remain
kinematic even when active. All their colliders are triggers when inactive,
enabled when active, and disabled when inactive. The separate ignoreCollidersWithTag constructor entry is IgnoreMe; it is
not this active-setter tag. SyncTransforms, optional
component effects and the physical bodies remain caller-owned; the C helper
reports policy requests without applying them.

The wrapper Activate always sets shotByBullet and get-up readiness, even when
the setter admits no change. Non-immediate Deactivate only makes get-up ready.
Immediate Deactivate clears falling/gettingUp, sets active false and invokes
OnGetupFinished even if already inactive. C retains both active and wrapper
callback state so they are not conflated.

## Controller call and fall entry

DoAnimFollow calls DoRagdollControl **before** testing active, then, if active,
resets total errors and runs the drive. Thus controller input totalForceError
belongs to the preceding drive, not the current one, and controller calls can
happen while inactive. Ordinary FixedUpdate calls DoAnimFollow; SimpleFootIK's
DoSimpleFootIK can also call it. Their actual enabled-component scheduling is
not established by static call sites and is not replaced by a native 60Hz clock.

The third controller call caches `inverse(masterRoot.rotation) * master.rotation`
as rootboneToForward. C emits a capture request; it does not invent a quaternion
or fit one from a still image. With an Animator and controller, the current
get-up-state/entry-transition classification is refreshed; otherwise its
prior values are retained. Previous-transition state is always retained first.

Fall entry occurs for shotByBullet or for totalForceError magnitude strictly
above 15 while orientated. On the first falling tick, non-Ground-Move/non-get-up
states with an Animator select the residual caps; other cases multiply them by
residualIdleFactor (female 0). It sets joint limits, secondaryUpdate 100,
restores friction and original drags and invokes OnFallBegin. Every admitted
entry clears the shot flag, sets falling, clears gettingUp/orientated and the
delayed-get-up latch, restores Animator speed and records collision speed >8
with a positive collision count. A shot during an existing fall does not
reinitialize the residual caps or repeat OnFallBegin. An error-triggered fall
does not itself set active true.

## Settling, water and get-up selection

While falling but not getting up, caps lerp toward zero by
`clamp01(fallLerp * fixedDeltaTime)`, with a joint-torque request at that point.
With a master entity and its Animator component, water checks test root position
minus 0.5 world-up and a downward ray from the root of length maxHeight+0.5,
mask 6144. Only under water with no ray hit sets ragdollInWater true. Master
position follows the root, or root minus maxHeight world-up in that water case.
C takes the query results and actual height/position as inputs; it does not
substitute native cosmetic water or movement collision.

Root speed must be strictly below the female's 0.2 m/s. On every settled tick
OnFallFinished runs, **including while get-up readiness is false**. With
readiness it sets gettingUp/orientate, zeros drive caps, requests Animator speed
0.25, and chooses a signal from
`(ragdollRoot.rotation * cachedAlignment * forward) dot down`:

- Nonnegative selects GetUpFront; if the current state is exactly the ground
  GetupFront state, it selects GetUpFrontMirror instead.
- Negative selects GetUpBack; if the current state is exactly ground
  GetupBack, it selects GetUpBackMirror instead.
- Water states do not satisfy those ground-state equality checks.

The compiled call is **SetBool(hash, true)**, not SetTrigger. The C output
retains that distinction. StringToHash, the input quaternion product and the
engine's admission/evaluation of this request are not qualified here.

## Orientation, ramp and completion

GettingUp does not orient immediately. It waits for a recognized AnyState-to-
get-up entry transition to change from present to absent. Then it clears
falling, requests the source master-to-ragdoll rotation alignment followed by
horizontal forward LookRotation, marks orientated, zeros friction, sets
get-up drags 35/17 and calls OnGetupBegin. C reports the alignment request;
the root pose and quaternion solve are not supplied.

Once orientated, maxTorque <0.5 takes the early ramp: Animator speed 0.35,
caps toward 4/3/5 by 0.15×fixedDeltaTime and secondaryUpdate 20. At or above
0.5, a recognized get-up state/transition takes the later ramp: Animator speed
0.75, caps toward the same target by 2×fixedDeltaTime, secondaryUpdate 4 and
joint limits disabled.

When neither get-up state nor entry transition remains, it restores original
drags, Animator speed, secondaryUpdate and friction. Completion requires limb
error strictly below 0.3 m **or** a recognized idle state (Ground Move, Water
Move or Monster Locomotion). Completion clears gettingUp/delayed state, sets
active false and calls OnGetupFinished. Otherwise delayedGetupDone is set,
allowing the subsequent contact-control pass to keep driving matching.
If the first error predicate fails with no Animator, source IsIdle dereferences
null; C reports that partial-state source fault instead of invented completion.
There is no substituted clip-length or recovery timer.

The final contact pass resets collisionSpeed and updates contact/noncontact
clocks. Outside falling/gettingUp, or with delayedGetupDone, it lerps caps
back to 100/100/10000 when not contacting, or toward 4/3/5 when contacting.
This pass also runs after completion, so final scalar caps can differ from the
value passed to an earlier SetJointTorque in that same call. C preserves that
earlier request separately. No physical contact or PD error is synthesized.

## Implemented in C and verification

[pe_animfollow](../../rebuild/src/pe_animfollow.h) holds the disconnected female
flags and scalar caps, active admission/body policy, classifier/signal names,
wrapper requests and controller decisions. It emits explicit alignment,
friction, joint, Animator, position and callback requests. Caller owns PD
errors, velocities, actual Animator observations, geometry queries, optional
components, transform synchronization, joint/force consumption and cadence.

[export_animfollow.py](../../rebuild/tools/export_animfollow.py) checks the
committed excerpt against installed assembly bytes and binds the stripped
prefab layouts/settings. Generated outputs must be regenerated, never patched.
`make -C rebuild animfollow-source-check` verifies outputs and exporter tests
with UnityPy. Tests need the installed qualified artifacts, not the ignored
one-off inventory scripts from this investigation.

Focused core tests cover exact names, active/callback distinction, repeat
activation/deactivation, body policy, third-call alignment, settling and error
boundaries, repeated callbacks, all four signal cases, water queries, both
ramps, transition exit, limb-error/idle completion, delayed matching, retained
registration inputs, contact arithmetic, missing-assignment skips, source null
fault and atomic invalid input. Six exporter cases cover evidence/field-layout
mutation, truncated/trailing component input, whole-body payload mutation,
serialized overrides and regeneration. Full outcomes are in
[PLAYTEST](../PLAYTEST.md).

Still unqualified: the PD force/torque and ConfigurableJoint solve, actual
bodies/collisions and friction, quaternion synchronization/orientation,
Animator transitions/layer composition and every resulting pose, Mono float
width/bit equivalence, and enabled scene scheduling. No live hit reaction,
get-up, shield, pounce or defense gate is opened. Next offline target is the
already inventoried AnimFollow_AF.PDControl/drive and body/joint contracts, or
FinalIK's offset solve; isolated reaction/get-up bakes remain a separate engine
need. Passing this reducer is not physical or visual acceptance.

Subsequent source follow-through: [the drive and installed rig](ANIMFOLLOW_DRIVE.md)
now have a disconnected C request owner. PD math, indexed profiles and joint
requests are implemented; actual body/pose/physics consumption remains gated.
