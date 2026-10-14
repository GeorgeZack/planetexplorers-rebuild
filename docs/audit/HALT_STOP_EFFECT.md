# Halt and the foot-stop effect

October 12, 2026. Source-derived from checked-in C#, the female prefab and the
action table, implemented in C. Thirteen observed stops agree with its offsets
to 2 mm (see [observed comparison](#observed-comparison)). Since October 13,
by the user's decision, both hosts apply the lock and draw the effect; see
[live in both hosts](#live-in-both-hosts).

## What Halt is

`Action_Halt` (action 50, mask 16) plays no clip. `DoAction` starts
`IKAnimEffectCtrl.StopMove(phyMotor.velocity)` and sets the Halt mask. `Update`
keeps the action while `moveEffectRunning` holds, then clears the mask.
`EndImmediately` stops the effect and clears the mask.

Three places request it: `Action_Move.EndAction`, `Action_Sprint.EndAction`,
and `Action_Repulsed` once its stop time has passed.

**The mask matters to play.** Six action rows are forbidden while it is set:
Move, Jump, Gather, PickUpItem, Abnormal and Ride. So after a stop the original
refuses to start moving or jumping again until the effect has run out. Halt
itself is forbidden in the air, in water, while rotating, dead, holding or
aiming equipment, and in a few other states; it ends Move, Sprint and Rotate
immediately. A request while it runs is ignored.

## The effect

`IKMoveEffect.StopMove` projects the velocity on the ground plane and reads two
serialized curves at its magnitude:

| Speed (m/s) | Weight | Duration (s) |
|---|---|---|
| 0 | 0 | 0.2 |
| 2 | 0 | 0.22 |
| 4.5 | 0 | 0.42 |
| 5 | 0.17 | 0.5 |
| 6.5 | 0.72 | 0.76 |
| 8 and above | 1 | 0.9 |

Weight is exactly zero through 4.5 m/s. A stop from a walk or from rest still
runs for 0.2–0.42 s and still holds the mask; it just moves nothing. Only stops
from a run or sprint are visible.

The stepping foot is the right one when the vector from the right foot to the
left foot is more than 90° from the motion, otherwise the left. Exactly 90°
(feet level, moving straight ahead) and a zero velocity both give the left.

Each foot has its own effect with seven links: left foot, right foot, body,
both shoulders and both hands. Each link has a force curve and an up curve over
normalized time. Every solver pre-update, with `elapsed` measured before the
frame's time is subtracted:

```
crossfade = clamp01(elapsed / fadeTime)            // 0.01 s left, 0.3 s right
offset   += (force(elapsed/duration) * direction + up(elapsed/duration) * worldUp)
            * masterWeight * crossfade * weight
remaining -= deltaTime
```

The first frame therefore contributes nothing. At full weight the planted foot
is pushed about 0.58 m along the motion and the body dips about 0.46 m at
mid-effect; the solver decides what pose that gives. No link pins to world
position. The serialized step time, weight and direction on each effect are
editor leftovers that `DoEffect` overwrites; `m_StartEffect` has no links and
`StartMove` is empty.

`IKOffsetModifier` skips the whole pass when the component is disabled, the
master weight is not positive or `deltaTime` is zero, without consuming effect
time. `moveEffectRunning` is false in the first two cases, so the action ends
at once while the effect's remaining time lingers.

## C owner

[pe_halt](../../rebuild/src/pe_halt.h) holds the relation masks, the speed
profile, the foot choice, the lifecycle with its no-op repeat and missing-part
paths, and per frame the seven effector offsets with their FinalIK effector
ordinals. All state is transient and unsaved.
[The exporter](../../rebuild/tools/export_halt_effect.py) guards the five
source bodies, the three request sites, the serialized curves and links, the
component identity and relation row 50 including the six rows the mask blocks.

## Observed comparison

**Observed in the original, October 12, 2026.** The supervised
[solver capture](FINALIK_FBBIK.md#engine-comparison) recorded every effector
offset on the live female player for 1,399 frames, including thirteen stops. `analyze_reference_halt.py` replays `pe_halt` over the
recorded frame times and compares.

The capture does not hold the velocity Halt began with, so each stop is
fitted: foot, start frame, speed and heading. The comparison uses the channels
only this effect writes (body, both hands, vertical shoulder offsets); the
grounder shares the feet and horizontal shoulder offsets.

| Measure over 13 stops | Value |
|---|---|
| Worst error on Halt-only channels | 2.0 mm |
| Worst error on grounder-shared channels | 3.3 cm |
| Fitted foot against the foot rule on observed foot positions | 13 of 13 |
| Fitted speed against observed body motion | within 0.47 m/s |
| Fitted heading against observed body motion | within 4.6° |
| Fitted speeds | 4.5–4.8 m/s (eight stops), 6.9–7.7 m/s (five stops) |

The foot positions and body motion are not seen by the fit, so the last three
rows are an independent check on it. What this establishes:

- **The curves, crossfade, weight and duration profiles match.** The slower
  stops last four to five frames at about 11 frames per second, as
  0.43–0.47 s predicts; the faster ones nine to ten.
- **The first frame adds nothing.** Every fitted start is exactly one frame
  before the first nonzero offset, which settles the nominal frame order.
- **Shoulder offsets really do reach 11.6 m upward** at mid-effect in the
  fastest stops. That is the serialized shoulder up curve, not a capture artefact. The
  solver absorbs it: the arm chains pull back and the largest bone
  displacement in the whole capture is 0.67 m.

Limits: flat tutorial ground, so the grounder's share was small; speed and
heading are fitted, not measured at `DoAction`; the Halt mask and its Move/Jump
refusal were not observed at all.

```sh
python3 rebuild/tools/analyze_reference_halt.py CAPTURE.jsonl
make -C rebuild halt-capture-test
```

The tool's own test recovers known stops from a synthetic capture; it checks
the analyzer, not the original.

## Validation and remaining gate

The registered core suite covers masks, the speed profile, both feet and the
two left-foot boundary cases, the zero first frame, a 55-frame run stop and a
14-frame standing stop at 60 Hz, repeat requests, cancel, missing effect or
motor, zero and partial master weight, a disabled component, zero delta time
and atomic invalid input. Six exporter cases cover eight source mutations, the
relation rows and ten prefab mutations. Full results are in
[PLAYTEST](../PLAYTEST.md). No game was launched.

Still unqualified:

- **The solve.** FinalIK's `FullBodyBipedIK` is a third-party plugin outside
  the checked-in source. Offsets are requests; the resulting pose is unknown.
  It would need the installed plugin assembly read statically, or an engine
  capture of the stop. October 12 follow-through: the static read is done in
  [the solver audit](FINALIK_FBBIK.md), and an observed session agrees with
  `pe_fbbik` to 0.29 mm. Feet and pelvis still need the unrecovered grounder.
- **The mask.** The offsets are observed; the Move/Jump refusal while the
  effect runs is source-derived only.
- **Mono arithmetic.** Curve evaluation follows the order qualified earlier for
  Repulsed; not claimed bit-exact.

## Live in both hosts

October 13, 2026. **User decision: apply the lock as the original does.**
Implemented in C; the lock itself is source-derived and was not observed.

[pe_halt_host](../../rebuild/src/pe_halt_host.h) is called once per fixed
movement step by the sandbox and the tutorial, before the step reads its
input. From `ActionRelation` rows 1, 2, 4 and 50, `MotionMgrCmpt` and
`Motion_Move_Human`:

| Event | Result | Source |
|---|---|---|
| Movement released on the ground while moving | Halt starts with the current velocity | `Action_Move.EndAction`, `Action_Sprint.EndAction` |
| In the air, in water, dead, or hand equipment drawn | Halt refused | row 50 forbidden masks |
| Mask set | movement, sprint and jump requests dropped | rows 1, 2 and 4 forbid on the Halt mask |
| Jump, sprint/run switch, lost ground, action mask | Move ends without a Halt | `EndImmediately` lists |
| Death | Halt ends at once | row 9 `EndImmediately` |

So an unarmed player cannot move or jump for about 0.22 s after a walk stop,
0.5 s after a run stop and 0.9 s after a sprint stop. With a tool, knife or
torch drawn there is no Halt and no lock. Load, revival and reset clear it.
The mask also refuses Gather. `MainPlayerCmpt` requests it with a plain
`DoAction(PEActionType.Gather)` on the gather key, and a refused request does
nothing and shows nothing. Both hosts follow that: while
`pe_halt_host_masked` is true the sandbox owns the press but harvests nothing
and writes no message, and the tutorial does not begin a herb. The player
presses again after the lock. The mask lists PickUpItem too, but no source
code requests that action for the player (`grep -a` over `Assets/Scripts`
finds only its definition, registration and network sender), so hare-drop
pickup stays independent of the lock.

The offsets reach drawing through [the always-on solve](PLAYER_IK_DRIVE.md),
turned from world into the drawn model's frame. They never move the player, a
capsule or a contact query. Without the staged solver groups the lock still
applies and nothing extra is drawn. Rebuild speeds are 2, 5 and 8 m/s, so a
walk stop moves nothing, a run stop has weight 0.17 and a sprint stop weight 1.

Measured over the installed assets by the real sandbox actor: the largest
vertex displacement from the baked pose is 0.001 mm in a walk stop, 22 cm in a
run stop and 80 cm in a sprint stop. Mid-effect in a sprint stop the stepping
foot is 0.54 m along the motion and the chest 0.34 m forward and 0.23 m down,
the same at every body yaw.

Adapters, not original behavior: the 60 Hz fixed step stands in for the
original's frame; a running Halt is updated before the step's input is read;
a Halt begun in a step takes its zero first offset pass in that step; a jump
requested together with a release wins; the feet for the foot test come from
the blended staged pose; the velocity is the host controller's. The grounder,
which shares the foot and shoulder channels, is not connected.

Verification: the `halt_host` core suite covers each row above, lock lengths
for walk, run and sprint stops, the foot rule, repeat requests, death, reset
and atomic invalid input. `sandbox-actor-test` covers the world-to-model turn,
refused offset updates and reset. `test_player_ik_drive.py` runs the three
stops through the real actor over the installed assets. The tutorial's
15,000-frame progression passes with 5 stops and 65 locked steps. A
private-display sandbox session with ordinary key input shows a sprint stop as
a forward lunge and crouch that recovers within a second
(`rebuild/build/halt-private-20261013a/`). None of this compares the drawn
stop with the original's.
