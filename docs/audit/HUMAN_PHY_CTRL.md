# HumanPhyCtrl request consumption and force ordering

October 12, 2026. Source-derived from installed CIL and implemented in a
disconnected C owner. Neither gameplay host calls `pe_human_phy`; live hit
reactions, stun and knockback remain gated.

## Why the installed assembly

`HumanPhyCtrl` has no C# file in the reference project. Every use in the
checked-in scripts is a call into it. The class exists only in the installed
`Assembly-CSharp.dll`, so its rules are read from that file's CIL without
loading or running it.

REA 6.0.0 `inspect-managed-members` produced complete inventories. The MCP form
closed its connection on the 8.4 MB assembly; the same command through the CLI
completed. The 367 MB result stays ignored under `rebuild/build`. A
[focused excerpt](../reference/human-phy-ctrl-il.json) keeps the raw CIL,
offsets and hashes of all 46 `HumanPhyCtrl` methods, the shipped
`Action_Repulsed.Update`/`EndImmediately`, the first-pass
`Util.ProjectOntoPlane` and the fifteen `Vector3`/`Mathf` helpers they call.

| Assembly | SHA-256 | MVID | REA EvidenceID |
|---|---|---|---|
| Assembly-CSharp.dll | `56d90949…a093f4d7d3` | `7ddcc464-5353-4750-b969-fe628ee474da` | `ev_13b79477…f27ee7243` |
| Assembly-CSharp-firstpass.dll | `9fb0f052…f3dbe399df` | `7b178142-9afb-4aa1-ad2b-b1f8d0d56dee` | `ev_3ac0cdbd…e534fc29a6` |
| UnityEngine.dll | `f7281619…5926c051b` | `5cf16eb2-32c9-483e-8fb7-9072104d5826` | `ev_f1f0444b…2056067a3` |

The excerpt and exporter carry the unabbreviated values.

[cil_disasm.py](../../rebuild/tools/cil_disasm.py) decodes method bodies from
file bytes. Its instruction count equals REA's for every body in the three
assemblies (44,059, 2,685 and 5,103 methods). The decoder is a reading aid;
[the exporter](../../rebuild/tools/export_human_phy.py) pins artifact hashes,
MVIDs, a digest over every excerpted body and the individual bodies the C owner
depends on, then compares the excerpt with the installed bytes.

The shipped `Action_Repulsed.Update` matches the checked-in C#: same strict
Wentfly and Halt comparisons, the same single `ApplyMoveRequest(m_MoveDir *
m_FinalVelocity)`. The shipped assembly also contains callers that the
checked-in C# lacks, including `MainPlayerCmpt.UpdateMoveState`
(`ApplyMoveRequest` with `waterUpSpeed * up`, and `ApplyImpact`),
`Action_Rotate` and `Action_Step`. The excerpt lists every static caller.

## Active female binding

`FemaleModel.prefab` object 100170 (layer 10) carries the one enabled
`HumanPhyCtrl` 11400002, script GUID `67b4d01020fbe104eb36ed0d3c6900ad`, with
Rigidbody 5400000 and CapsuleCollider 13600000.

| Value | Constructor | Female prefab |
|---|---|---|
| `MaxSubSpeed` | 10 | not serialized: 10 applies |
| `m_MoveSpeed` | 5 | not serialized |
| `m_GroundDrag` / `m_WaterDrag` / `m_AirDrag` | 0.5 / 0.16 / 0.006 | 1.5 / 1.3 / 0.006 |
| `m_StopSpeed` | 0.1 | 0.05 |
| `m_LeaveGroundTime` | 0.05 | 0.5 |
| `m_SpineHeight` | 1.3 | 1.5 |
| `m_CheckGroundRadius` | 0.3 | 0.47 |
| `m_IsContrler`, `netMoveSpeedScale`, `maxSpeed` | 1, 1, 50 | 1, 1, 50 |
| `m_GroundLayer` / `m_WaterLayer` bits | — | 2751232 / 16 |
| `m_SubAcc`, `m_WaterAcc` | — | zero |

The Rigidbody has mass 200, drag 0, `useGravity` false, not kinematic,
constraints 112 (rotation frozen), discrete collision and no interpolation.
`Awake` also forces `freezeRotation` and clears `useGravity`. The capsule has
radius 0.37, height 1.75, centre y 0.875 and no physic material. Project
settings give fixed timestep 0.02 s, maximum allowed timestep 1/3 s and gravity
y −9.81. `Awake` copies that gravity into the controller.

## Request consumption

- `ApplyMoveRequest(v)` adds `v` to `m_RequestSubVelocity`. If the sum's
  magnitude is strictly greater than `MaxSubSpeed`, it becomes
  `MaxSubSpeed * normalized`. Several callers in one frame therefore add before
  the clamp.
- `LateUpdate` stores `Rigidbody.velocity` as the inertia velocity, copies the
  pending request into `m_ApplySubVelocity`, and zeroes the pending request.
- `CancelMoveRequest` zeroes both at once. It does not touch
  `desiredMovementDirection`; `Action_Repulsed.EndImmediately` clears that
  separately.

Consequences for Repulsed:

1. `MotionMgrCmpt.OnUpdate` runs from `CmptMgr.Update`. A request made there is
   latched by the same frame's `LateUpdate` and first consumed by the
   **following** frame's `FixedUpdate` calls.
2. A request that is not renewed lasts exactly one frame's `FixedUpdate` calls.
3. The active female velocity curve exceeds 10 m/s from about force 444
   (9 at force 400, 16 at force 750). The controller holds such requests to 10.
   `pe_repulsed` still reports the unclamped request; the clamp belongs to this
   owner.
4. The request is a target velocity, not an impulse. A frame with no
   `FixedUpdate` consumes nothing and nothing accumulates.

## Update: ground, water and drag

`Update` zeroes `_cntFixedUpdate`, then calls `UpdateGroundState`,
`UpdateWaterState` and `UpdateDrag` in that order.

**Ground.** `OnCollisionEnter` and `OnCollisionStay` append the normal of every
contact whose other object is on a ground layer. While
`Time.time - m_ImpactTime < m_LeaveGroundTime` the controller is "impacting":
`grounded` and `phyGround` are false. Otherwise the controller's `CheckGround`
is true when any normal was collected or the Rigidbody is kinematic; a
non-controller uses `OverlapSphere` instead. `grounded` stays true until
`m_LeaveGroundTime` has passed since the last physical ground. `fallGround` is
the frame grounded turns true. `m_ImpactTime` starts at 0, so the first half
second of game time counts as impacting.

With normals and `spineInWater` false (the value from the previous frame),
normals within `m_MaxGroundBalanceAngle` 60° of world up are summed and
normalized into `m_GravityUpDir`; a zero sum gives world up. Normals within 90°
feed the forward ground angle. A kinematic Rigidbody skips the loop. Without
normals, or in water, the direction is world up and the angle decays by
`Lerp(angle, 0, 3 * deltaTime)`. The list is cleared either way.

**Water.** The three flags reset each frame. `PE.PointInWater` is sampled at
the root, or 0.5 above it when grounded. Above 0.5, a 2 m downward ray from
root + 1.75 on the water layer gives the surface height; a miss assumes
root + 2.25. The clamped depth sets feet (> 1.0), spine (> 1.5) and head
(≥ 1.75 less one float epsilon). `m_WaterAcc` is `-gravity * up` when the spine
is in water, else zero.

**Drag.** Ground drag when grounded, else water drag when the spine is in
water, else air drag.

## FixedUpdate: force order

1. Return if `freezeUpdate`.
2. `desired = (desiredVelocity + m_ApplySubVelocity) * netMoveSpeedScale`,
   where `desiredVelocity` is `desiredMovementDirection * m_MoveSpeed`, or zero
   for a zero direction.
3. Kinematic Rigidbody: a non-controller calls `MovePosition(position +
   desired * fixedDeltaTime)`; then `Rigidbody.velocity = 0`; return. The
   counter is not advanced.
4. `if (_cntFixedUpdate++ >= 2) return`. Only the first two calls after each
   `Update` do anything. Later calls in a long frame apply no request, gravity
   or drag.
5. Water-surface hold: not grounded, feet in water, `m_SubAcc.y <= 0`,
   `desired.y > 0`, and depth between spine − 0.05 and 1.75. `desired.y`
   becomes 0, and the root is set to surface − 1.55 unless it is already a
   spine height above that.
6. If `desired != 0`: when `phyGround`, `spineInWater` or `useRopeGun`,
   `force = desired - velocity`. Otherwise both are first projected onto the
   plane of `transform.up`, so vertical velocity is left alone in the air.
7. If `velocity.sqrMagnitude > 10000`, `force = -velocity`.
8. `AddForce(force, VelocityChange)`.
9. `AddForce(gravity * m_GravityUpDir, Acceleration)`, then `m_WaterAcc`, then
   `m_SubAcc`, each as `Acceleration`.
10. If `desired == 0`: `AddForce(-m_Drag * sqrMagnitude * normalized,
    Acceleration)`.
11. If `desiredVelocity == 0`, `m_SubAcc != 0` and speed is below
    `m_StopSpeed`: `set_velocity(zero)`, which is `AddForce(-velocity,
    VelocityChange)`. The female prefab's zero `m_SubAcc` never satisfies this.
12. If `position.y < -10000`: move to y −10 and `AddForce(-velocity,
    VelocityChange)`.
13. If speed exceeds `maxSpeed` 50: `AddForce(-normalized * (speed - 50),
    VelocityChange)`.

Mass does not enter: every request is `VelocityChange` or `Acceleration`.
Vector equality is Unity 5's squared distance below about 1e-10, `Normalize`
returns zero at or below 1e-5, and `ProjectOntoPlane` is `v - Project(v, n)`.

For a grounded Repulsed this means: one frame after the first request, each
running `FixedUpdate` asks the solver to replace the body's whole velocity with
`direction * min(curve velocity, 10)`, plus gravity along the averaged ground
normal. When requests stop, quadratic ground drag `1.5 * speed²` decelerates
the body; there is no snap to rest.

## Collision

`HumanPhyCtrl` contains no collision response of its own. Its only collision
code collects ground-layer contact normals and, for non-controllers, runs
`OverlapSphere`. Sliding, depenetration, step handling and contact generation
belong to PhysX acting on the capsule. Those are engine behavior and are not
recovered by this increment. The rebuild's existing swept player capsule is a
separate native adapter.

## C owner

[pe_human_phy](../../rebuild/src/pe_human_phy.h) holds the request latch,
ground/water/drag state and one `FixedUpdate`'s requests. It emits the summed
`VelocityChange` and `Acceleration` vectors, an optional position write and the
kinematic outputs. It does not integrate, collide or move anything. The caller
supplies contact normals, water probes, transform and Rigidbody velocity, and
must call it in the original order: `FixedUpdate` × n, physics, `Update`,
`LateUpdate`. Invalid input leaves state and output unchanged. All state is
transient and unsaved.

Constants come from [generated metadata](../../rebuild/generated/pe_human_phy_source.json)
and its header. `pe_repulsed` is unchanged.

## Validation and remaining gate

The registered core suite covers request summing, the strict 10 m/s clamp,
latch and one-frame lifetime, cancel, the two-call counter, freeze and
kinematic branches, grounded and airborne replacement, drag, the stop snap,
overspeed and the 50 m/s limiter, the below-world rescue, ground timing and
impact, gravity direction, forward angle, water flags, the surface hold,
Repulsed requests at force 400 and 750, and atomic invalid-input failure.
Sixteen exporter cases cover the decoder, evidence mutations, installed bytes,
prefab and project settings. Two ordinary-interpreter tests compare 404 request
sequences and 900 force cases with an independent double oracle, using
tolerances rather than a bit-exact claim. Full results are in
[PLAYTEST](../PLAYTEST.md).

No game launch, desktop input, save, pose capture or asset installation was
needed. PECSv22 and live damage are unchanged.

Still unqualified:

- **PhysX.** Whether `Rigidbody.velocity` read inside `FixedUpdate` excludes
  forces queued earlier in the same call is assumed, not observed. The C owner
  uses one velocity snapshot per call. Integration, contact response and solver
  order are not recovered.
- **Mono arithmetic.** The CIL fixes operation order; JIT float width and
  `Mathf.Epsilon`'s flush-to-zero choice are not qualified. The epsilon only
  matters for non-unit, near-zero plane normals.
- **Frame pacing.** The native 60 Hz gameplay tick and the original's 0.02 s
  fixed step with a two-call limit differ. A host needs an explicit scheduling
  adapter.
- **Other inputs.** `Motion_Move_Human`, `Action_Move`/`Sprint`/`Jump`/`Fall`
  drive `desiredMovementDirection`, `ResetSpeed` and gravity. They are listed
  as callers and not ported here. `Motion_Beat.OnMsg` also writes `grounded`
  directly.
- **Existing native adapters.** `Action_SwordAttack` root motion reaches the
  same `ApplyMoveRequest`, so the original holds it to 10 m/s and applies it
  one frame late. The rebuild's running-attack path keeps its documented
  adapter; this increment does not change it.

Before gameplay integration: recover Whacked/Repulsed poses and animation
events, then compose reaction, body, attachments, defence and cancellation with
the actual host. `Action_Halt`, `Action_Wentfly` and `Action_Knocked` are also
compiled callers of this controller and need their own owners. This increment
supplies no permission to enable live stun or knockback.
