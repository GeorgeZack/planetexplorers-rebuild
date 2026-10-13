# Halt and the foot-stop effect

October 12, 2026. Source-derived from checked-in C#, the female prefab and the
action table, implemented in a disconnected C owner. Neither gameplay host
calls `pe_halt`. No pose results from it yet.

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
  [the solver audit](FINALIK_FBBIK.md); the disconnected `pe_fbbik` turns
  offsets into a pose with unqualified engine-native adapters.
- **Frame order.** Whether the action's first `Update` runs in the frame of
  `DoAction`, and the exact solver phase, are nominal here.
- **Mono arithmetic.** Curve evaluation follows the order qualified earlier for
  Repulsed; not claimed bit-exact.

Not done, and a behavior change if done: applying the Halt movement lock in the
live sandbox. The rebuild's player can currently move or jump again immediately
after stopping; the original cannot for 0.2–0.9 s. That lock needs no pose and
is source-portable, but it changes how movement feels, so it is left for an
explicit decision.
