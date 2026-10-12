# Sword lunge: root motion in source controller order

October 12, 2026. Source-derived and implemented in the live Adventure sandbox.
This changes how blade-attack root motion reaches the player body. It adds no
attack, weapon, branch or save field, and opens no closed gate.

## What was checked

"The lunge" is the running blade attack: skill 20110014, clip RunSwordAttack
408, admitted at full speed above 4.5 and up to 7 m/s. It has been live for
Knife33 since October 6 with a qualified captured root curve.

The rebuild already used the original's request formula from
`Action_SwordAttack.UpdateSkillState`: project the animator's root delta on the
ground plane, divide by `deltaTime`, and limit the change from the current
velocity to 12 per update. It then applied that request to the body in the same
tick. The original does not. The [HumanPhyCtrl qualification](HUMAN_PHY_CTRL.md)
shows what happens between the request and the body:

| Step in the original | Earlier rebuild | Now |
|---|---|---|
| The delta used is `AnimatorCmpt.m_LastMove`, written by `OnAnimatorMove` during the previous frame's animator update | this tick's delta | previous tick's delta |
| `ApplyMoveRequest` holds the summed request to `MaxSubSpeed` 10 m/s | no hold | held to 10 |
| `LateUpdate` latches the request; the next frame's `FixedUpdate` replaces the grounded body's velocity with it | applied at once | applied one tick later |
| `SwordAttack` ends Move and Sprint immediately, so with nothing latched the body only coasts under ground drag `1.5·v²` | velocity replaced on the first tick | coasts until the first request arrives |
| On a normal end, `OnEndAction` calls `CancelMoveRequest` in the next Update: one more `FixedUpdate` runs first, and the ending frame's delta is never applied | terminal delta applied | terminal delta dropped |
| `EndImmediately` sets horizontal velocity to zero, resets inertia and cancels requests | action cancelled, velocity kept | velocity zeroed, requests dropped |

Together: a root delta reaches the body two frames after its pose.

## Measured effect on the recovered lunge

At animator speed 1.37 the recovered curve peaks at 13.7 m/s about 0.07 s into
the clip and exceeds 10 m/s for three 60 Hz ticks. In the constructed flat
fixture, entering at 5 m/s, the lunge travels 3.9405 m by the earlier direct
path and 3.8666 m in source controller order: 0.074 m shorter. The hold removes
about 0.14 m; the first coasting tick gives some back. The body now owns 52
ticks, the last one after the action has ended.

The same order applies to every blade stage, including the source-derived
Attack6–9 motion, because they share `UpdateSkillState`.

## Implementation

[pe_sword_motor](../../rebuild/src/pe_sword_motor.h) holds a
[pe_human_phy](../../rebuild/src/pe_human_phy.h) request latch and the previous
root delta. Each native tick calls it in the original callback order:

1. `consume` — the FixedUpdate half. With a blade action owning the tick, it
   returns the horizontal velocity for the collision step: the latched request,
   or the current velocity after one step of source drag.
2. The existing swept collision step moves the body.
3. `update` — the Update, animator and LateUpdate halves. It converts the
   previous delta into a request with the existing 12-per-update correction,
   applies the 10 m/s hold, records this tick's delta and latches.

The host keeps one authoritative interval for pose, blade and defence. Only the
root displacement is delayed. `pe_running_attack_motion_delta` and
`pe_sword_root_motion_world_delta` expose the yaw-rotated displacements the
request functions already computed. The host detects an action cut short (no
terminal interval, or ended between ticks) and applies the `EndImmediately`
rule; a load resets the motor without touching the saved velocity. Legacy saves
keep their original controller.

The motor state is transient and unsaved. PECSv22 is unchanged.

## Native adapters, stated

- **Pacing.** One native 1/60 s tick stands for one original frame with one
  `FixedUpdate`. The original runs physics at 0.02 s with at most two steps per
  frame, so its lag in seconds depends on frame rate.
- **Coasting.** Only the source quadratic drag is applied. PhysX contact
  friction on the capsule is not modelled.
- **Collision.** The rebuild's swept capsule, not PhysX, resolves the motion.
- **After the attack.** The original leaves the body with its last velocity
  under drag until Move takes over. The rebuild returns to its native walking
  step after the one trailing tick.
- **Root curve.** Stage 408 is the captured isolated-clip curve; Attack6–9 use
  source knots. Neither is a live motor trajectory from the original.

## Validation

`sword_motor` core cases cover the idle pass-through, the two-tick lag, the
coast, the 10 m/s hold, the 12 correction against the velocity at Update,
diagonal requests, the trailing tick, the dropped terminal delta, release,
reset, airborne drag and atomic invalid input. `running_attack_pipeline` now
runs the real staged lunge through both orders and checks the peak, the hold
duration, the single trailing tick and the distance difference. Host routing
tests keep their earlier assertions on top of the displacement router.

A private-display session of the production build (display `:100`, checkpoint
`strafe-physical-start.pesave`, SHA-256 `26a9d596…`, unchanged afterwards)
drew the knife and logged four blade starts with no motion error. Screenshots
and the input journal are under
`rebuild/build/sword-motor-private-20261012`. Those were attacks from rest or
just after releasing W: the private input tool cannot hold a movement key and
click at once, so **no running start was produced there**. The session was
ended by the tool's stop, not a menu quit.

Not verified: a running lunge in the live host after this change, its look and
feel, and any comparison of trajectory with the original game. The only
original observation of this branch remains the 2 fps live03 capture, which
cannot resolve a two-frame lag or a 7 cm difference.

## Still not the original

- Sprint attack (`m_SkillInSprint`, above 7 m/s), air and water attacks, first
  person and male remain unsupported. The sprint attack needs its own pose and
  root capture from the engine.
- Attack7–9 after a running first stage still use the stationary continuation
  clocks.
- Aim IK, `m_RotateAcc` facing, hit IK and reactions are separate.
