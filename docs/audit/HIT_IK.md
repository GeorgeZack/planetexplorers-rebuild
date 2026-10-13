# Female hit IK offset requests

October 12, 2026. Source-derived from checked-in C# and the female prefab;
implemented in a disconnected C owner. Neither game was launched for this
increment. There is no recovered FinalIK solve or reaction pose.

October 12 follow-through: [the installed solver](FINALIK_FBBIK.md) is now
read statically into the disconnected `pe_fbbik`, and its suite feeds a head
hit's offsets through it. `IKOffsetModifier` subscribes to the solver's
`OnPreUpdate`, so these offsets precede the pose read.

## Active call chain

`SkAliveEntity.ApplyRepelEff` reaches `Motion_Beat.Beat` and
`ApplyMoveEffect`, whose angle-scaled force selects the reaction branch.
Only Null and Repulsed call `ApplyHitEffect`. Whacked uses its animation action;
Wentfly and Knocked request Wentfly instead. This is not an IK effect on every
committed HP hit. The preceding force permission and reaction decision remain
caller-owned; see [hit reaction](HIT_REACTION.md).

`ApplyHitEffect` draws independently from
`Random.Range(-BeatParam.m_RandomScale, BeatParam.m_RandomScale)`, multiplies the
already angle-scaled force by `1 + draw`, and evaluates the force-to-hit-weight
and force-to-hit-time curves. The female random scale is 0.1. Preserve the
sharp weight drop near force 400: the source weight is about 1.4 at 398.03656,
then 0.5 at 400; duration at 400 is about 0.85 seconds. It is not a smooth
monotonic force response.

`IKAnimEffectCtrl.OnHit` needs a FullBodyBipedIK reference and delegates to
`IKHitReaction.OnHit`. It starts **every** part whose transform list contains
the exact hit transform. Matching does not use names, a nearest limb or a
single first result. Each matching effect replaces its direction, weight,
root starting position and remaining time, with fade time 0.1 seconds.
Unmatched parts retain their existing effects. Direction is not normalized.

## Prefab contracts

The female component 11400010 has 16 hit parts, each with 6–9 authored
links to the nine FinalIK effectors. The exporter records transform IDs and
bone names, link order, both curves and world-position pin flags. In particular:

- Transform 400150 (left foot) belongs to both LFoot and the final Foot part.
  Transform 400236 belongs to the final Foot part alone. Preserve those
  memberships even though the latter part uses right-side links.
- LUpperArm includes two links to effector 2; LCalf and LFoot each include two
  links to effector 3, and RCalf and Foot each include two to effector 4.
  Their contributions add; they are not duplicate records to remove.
- Some body, shoulder, hand and foot links pin in world position. Their
  contribution subtracts the world root displacement since that part's most
  recent hit, before applying the weight.
- Chest's left-hand force curve has looping pre-infinity mode 1, but its first
  key is below zero. Supported normalized elapsed time is nonnegative, so that
  loop is unreachable. All reachable boundaries clamp. No general looping
  evaluator is claimed.

The per-part contribution at the solver's pre-update is:

```
elapsed = duration - remaining
fade = clamp01(elapsed / 0.1)
deltaRoot = currentRoot - rootAtHit
offset += (force(elapsed/duration) * direction + up(elapsed/duration) * worldUp
           - (pinned ? deltaRoot : zero))
          * (masterWeight * currentWeightScale * fade * weightAtHit)
remaining -= deltaTime
```

`weightAtHit` already includes the weightScale at the hit. Consequently a
constant scale of 0.5 makes the contribution one quarter, not one half. The
first solver frame contributes zero; it still consumes time. Disabled
modifier, absent controller/solver, nonpositive master weight or zero delta
time skip the pass without consuming time. Master weight above 1 clamps to 1.
An expired effect contributes nothing on the next pass. Offsets from separate
parts add in serialized part/link order, after the stop-effect pass in the
original controller.

## Implemented in C

[pe_hit_ik](../../rebuild/src/pe_hit_ik.h) owns the force profile and 16 transient
part clocks, exact transform routing, restart and nine summed effector
requests. The caller supplies the independent random draw and current root
position. No gameplay host or save owns this state. Invalid inputs or
nonfinite arithmetic fail without changing state or output.

[export_hit_ik.py](../../rebuild/tools/export_hit_ik.py) guards seven active
source bodies and the full serialized hit-part/profile payloads, preserving
source hashes and logical paths. Generated files must be regenerated, never
edited. `make -C rebuild hit-ik-source-check` compares outputs and runs the
UnityPy-backed mutation checks.

## Verification and remaining work

The core suite checks curve knots and the sharp force-weight boundary,
injected draws, all 16 transform bindings, overlapping left-foot effects,
per-part restart, nonunit direction, root displacement subtraction, twice
applied weightScale, concurrent summation, modifier skips, expiry and atomic
invalid-input/overflow failure. Six exporter tests include source and prefab
mutations and deterministic regeneration. Full results are recorded in
[PLAYTEST](../PLAYTEST.md).

Still provisional: Mono arithmetic equivalence, exact solver scheduling and
the resulting bone/defence pose. The polynomial follows the previously
[qualified Unity5 order](UNITY5_CURVE_EVALUATOR.md); it is not a new bit-exact
oracle proof. FinalIK is an installed third-party solver, not recovered by
these offsets. Live reactions, stun/knockback and shield presentation stay
gated. Wentfly/Knocked/GetUp lifecycle extraction and static FinalIK inspection
can continue from source/installed artifacts; the already prepared reaction
bake remains a separate engine-evaluation need.
