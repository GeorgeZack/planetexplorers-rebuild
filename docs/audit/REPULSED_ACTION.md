# Admitted Repulsed lifecycle and movement requests

October 10–11, 2026. Source-derived and implemented in a disconnected C owner.
Neither gameplay host calls `pe_repulsed`; live hit reactions remain gated.

October 12 follow-through: [HumanPhyCtrl consumption](HUMAN_PHY_CTRL.md) is now
read from installed CIL and held by the disconnected `pe_human_phy`. The
controller sums requests, holds them to 10 m/s and applies them one frame
later; `pe_repulsed` still reports the unclamped request. The
[animation contract](REACTION_PRESENTATION.md) shows what its four animator
floats select: a separate stagger clip set in the locomotion tree.

## Active source contract

`Motion_Beat.Start` registers `Action_Repulsed` and assigns its optional
`Motion_Move_Motor`. On `View_Prefab_Build`, the female view supplies
`BeatParam11471154` and `HumanPhyCtrl`. The action reads that bound parameter's
curves, rather than its own serialized fields on `EntityPlayer11400038`.

The active female profile has five four-key curves: time/velocity scale,
force/velocity, force/move time, force/stop time and force/Wentfly time. The
active force-0 clamped samples are velocity 3.9569721, move time 0.2051925 and
stop time 0.09598491. The unused entity curves give approximately 0.000142918
for all three. These are curve endpoints, not evidence that force 0 admits a
reaction. At force 400, the active velocity is 9 and move time 0.35; force 750
gives 16 and 0.55. Preserve the narrow Wentfly curve transitions near 700
and 750 rather than replacing them with a guessed duration.

All five curves have finite tangents and serialized pre/post infinity mode 2.
The existing [Unity5 evaluator qualification](UNITY5_CURVE_EVALUATOR.md) establishes
that this raw mode clamps endpoints and supplies the finite-key polynomial
operation order. Its sword-curve arithmetic replay does not establish a new
bit-exact engine comparison for these reaction curves.

The female Wentfly multiplier is 0.800000011920929, with transform 400106,
`Bip01 Spine2`, in the active model hierarchy. EntityPlayer has
`Motion_Move_Human11400024` and no `Motion_Move_Motor`. Consequently this
player's admitted reaction requests movement through `HumanPhyCtrl`, while
the optional generic-motor force branch is absent. The C owner represents
both branches' requests; no physical motor is executed.

[The exporter](../../rebuild/tools/export_repulsed_action.py) guards the active
source methods, model/view/entity bindings, exact curves, bone ancestry and
ActionRelation6's ordinal masks and relation lists. It records logical source
paths and hashes in [generated metadata](../../rebuild/generated/pe_repulsed_source.json).

## Action ordering

The larger motion manager owns admission and pauses. Row6 has no `EndAction`
waits; it pauses actions13,15,18,19,22,24 and immediately ends its recovered
list, including SwordAttack14. Pausing HoldShield15 leaves it running.
Dependencies prohibit InAir, Jump and Fall among other source masks; they do
not require Ground or prohibit Whacked. A request for an already-running
Repulsed bypasses dependencies and calls inherited, empty `ResetAction`.
It changes neither force/direction nor elapsed time.

Once admitted and unpaused, DoAction:

1. Tests `Angle(-entity.forward, previous m_MoveDir) < 30` with a running
   HoldShield before assigning the new hit direction. A qualified block halves
   the already angle-scaled force. Selected equipment alone cannot supply it.
2. Reads the active `PEActionParamVVF.param` singleton, averages the current
   and hit positions, and projects/normalizes the new direction on the actor's
   up plane. It does not read the queued `para` argument.
3. Evaluates the four force curves, resets elapsed/stop state, and sets
   `SheildBlockF`. Only the optional generic motor receives an initial force;
   HumanPhyCtrl movement starts at Update.

Update checks the old elapsed time. A present force bone and strict
`elapsed > wentfly_time` request Wentfly with the actor itself as the target,
the bone name, another normalized direction and the recovered force multiplier.
Repulsed then ends even if the manager denies Wentfly. This check precedes
move-time completion. While `elapsed < move_time`, the time/velocity curve
scales the requested velocity; strict `elapsed > stop_time` requests Halt
once, and only then is deltaTime added. Animation parameter requests use a
clamped5×deltaTime interpolation fraction when both animator and HumanPhyCtrl
exist. The caller still owns inverse-rotation velocity and the actual floats.

EndImmediately clears the mask, BeatWeight/SheildBlockF, desired direction and
movement request, and supplies zero to an optional generic motor. It sets
elapsed to move time, resumes the exact row6 paused actions through the manager,
and retains the old direction for a later admitted reaction. Context loss uses
that cleanup. The C API leaves state and output unchanged on invalid input.

## Installed Unity angle evidence

REA inspected the installed UnityEngine assembly without executing it. The
MCP member inventory exceeded its transport frame limit; the existing REA 6.0.0
CLI produced the retained full inventory under
`rebuild/build/repulsed-UnityEngine-members.json`. A
[focused static evidence excerpt](../reference/repulsed-unity-vector-angle.json)
preserves EvidenceID
`ev_f1f0444bcfceecdc4767fd6a28a0733e4bc518f31d1427187dc47a02056067a3`,
artifact SHA256, MVID, raw IL, method hashes, calls and limitations.

For assembly SHA256
`f728161940fbb8f27659c979bfcabe3f544002858779abfaa1143065926c051b`,
`Vector3.Angle` token 0x060004cc normalizes both operands, takes their dot,
clamps it to [-1,1], calls Acos and multiplies by 57.295780181884766.
`Normalize` token 0x060004c3 returns zero below its 1e-5 magnitude boundary.
Thus the zero-direction angle is statically derived as 90 degrees. The first
reaction's default old direction does not pass the shield cone. The C owner
handles its zero direction itself, guarding against a caller's modern zero-angle
calculation; nonzero angles remain caller-supplied. The exporter
checks these method bytes against the pinned assembly. This is static engine
evidence; no managed numerical execution or original gameplay was observed.

## Validation and remaining gate

The registered core suite covers admission masks, active-model endpoints,
previous-direction shield behavior, ignored repeated hits, strict Halt and
Wentfly boundaries, overshoot ordering, optional components, cleanup and atomic
invalid-input failure. Seventeen exporter cases pass with UnityPy and no skips.
Two ordinary-interpreter arithmetic tests compare 840 force-profile values and
404 movement phases with an independent double Hermite basis. They use explicit
tolerances rather than claiming bit-exact engine equivalence.

Full checkpoint results are recorded in [PLAYTEST](../PLAYTEST.md). No game,
desktop input, normal save, new pose capture or installed asset modification
was needed. PECSv22 and live damage remain unchanged.

Before gameplay integration, bake the reaction poses named by the
[animation contract](REACTION_PRESENTATION.md); and compose reaction/body/attachments/defence and cancellation with the
actual host. Angle/projection and final Wentfly normalization remain caller
operations. MotionMgr scheduling, shared parameter lifetime, Halt/IK effects,
ragdoll and shield composition require their own owners or explicit adapters.
HumanPhyCtrl's request and force order is [qualified separately](HUMAN_PHY_CTRL.md);
PhysX integration and contact response remain unrecovered engine behavior.
This increment supplies no permission to enable live stun/knockback.
