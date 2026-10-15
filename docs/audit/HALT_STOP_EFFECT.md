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

`pe_halt_host` runs once before each fixed movement step in both hosts.
Releasing movement starts the source foot-stop effect only while grounded,
alive, outside water and with hands stowed. Drawing offsets never move the
controller, capsules, contact queries or camera. Death/load/revival/reset clear
transient state; missing solver groups retain the same input policy.

**User-selected native control policy, October 15:** renewed movement (including
sprint) or jump input cancels Halt before this step's offsets and admission.
The request proceeds in the same step; other action locks still govern it.
This replaces the earlier source-derived Move/Jump refusal after the user
reported that the demo's stop lock felt bad. It is not an original-game
observation or a new source-parity claim.

While idle, the source effect/profile is unchanged: about .22s walking,
.5s running and .9s sprinting, with weights0/.17/1 at2/5/8m/s. Gather remains
refused while its mask is held; hare-drop pickup stays independent. Movement
input cancels that mask immediately. Fixed-step pacing, update-before-admission,
zero first offset, release-plus-jump priority and blended-foot/controller inputs
remain labelled adapters. Grounder and Halt offsets combine for drawing only.

Existing actor measurements: largest vertex displacement from baked pose
.001mm walking,22cm running and80cm sprinting. Mid-sprint foot offset.54m,
chest.34m forward/.23m down, invariant across tested yaws. These are native
asset measurements, not physical or original-game acceptance.

Regression proof: `core:halt_host` covers immediate movement/jump interruption
at walk/run/sprint speeds, first/later effect updates, zero remaining offsets,
repeat requests and independent movement admission. Source-only `core:halt`
keeps original curve, mask and cancellation rules. The new input policy is
implemented/tested in C; user acceptance after rebuilding remains pending.

Verification: Halt regressions pass; full `check` reports280 passed and5
LeakSanitizer/ptrace sandbox failures (1021 Python tests,98 optional skips).
Earlier functional reruns of those five suites passed with leak scanning
disabled (26 tests). Full core ASan/UBSan/leak checks pass:113 suites,0
failures/skips. The user explicitly requested commit/push with the known
full-check limitation. No clean full-check or post-change physical acceptance
is claimed.
