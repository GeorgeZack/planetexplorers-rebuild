# Whacked and Repulsed animation contract

October 12, 2026. Source-derived from the installed controller, checked-in C#
and installed CIL, with a disconnected C owner for the Whacked lifecycle.
Neither gameplay host calls it. No reaction pose exists in the rebuild yet;
live hit reactions, stun and knockback remain gated.

## What existing exports hold

None of the eleven captures in the bake profile, and no JSON under
`rebuild/build`, contains a reaction clip pose. Earlier controller inspections
name some of these clips but hold no poses. The clips are humanoid muscle clips (127 humanoid bindings each), which
this project recovers only through the original engine. So the graph, clip
identities and end signal below are recovered offline, and the poses need one
bounded engine bake, prepared here and not run.

## Whacked

`Action_Whacked` is action 5 with mask 47. `DoAction` sets the mask and, with
an animator, the `BeHit` trigger. It has no timer. `OnAnimEvent("HitAnimEnd")`
asks the manager to end it; `EndImmediately` clears the mask, sets
`ResetFullBody` and resets `BeHit`. The shipped CIL matches the checked-in C#.

Installed `FemaleAnimator2` (controller 779, also the male base):

- **Entry.** On layer 1 (`FullBody`), an Any State transition on `BeHit` enters
  `FullBody.BeHit.Blend Tree` with zero duration and no exit time. It may
  re-enter itself.
- **Layer.** Override blending, default weight 1, every body-mask bit set, no
  skeleton mask. The hit clip replaces the whole body.
- **Clips.** A one-dimensional blend on the `MoveStyle` float, thresholds 0–10:
  IdleBeHit 139, SwordBeHit 157, RifleBeHit 188, HandgunBeHit 173, BowBeHit 200,
  ShotgunBeHit 220, MineBeHit 210, IdleBeHit (styles 7 and 8), GrenadeBeHit 227,
  GlovesBeHit 116. All are 30 Hz, unlooped and about 0.9333 s.
  `Motion_Move_Human` smooths `MoveStyle` toward the held style by
  `Lerp(5 * deltaTime)`, so a style change shortly before a hit blends two
  clips. The starter Sword item's hand-change style is 1.
- **End signal.** The clips carry no events. The state's only authored exit has
  exit time 0.9 and duration 0.1 of the state (not a fixed duration), through
  an exit selector to the layer default `FullBody.DoNothing`. The state carries
  an `AnimatorEventSender` whose serialized fields are an empty enter event,
  exit event `HitAnimEnd` and no timed events. `OnStateExit` sends it. The
  nominal reaction therefore lasts one state length, about 0.933 s.
- **After.** `ResetFullBody` enters `FullBody.Empty` by Any State with zero
  duration; `Empty` leaves for `DoNothing` at exit time 0.02. `DoNothing` sends
  `OnEndFullAnim` on entry and carries `AnimatorLayerWeightCtr`, which fades the
  layer weight by `Lerp(weight, 0, 5 * deltaTime)` each update and restores 1 on
  exit.

The male override lists every clip but really replaces two of these: NormalIdle
684→688 and MineBeHit 210→110 (0.9 s).

Action relation row 5 forbids the masks Repulsed, Wentfly, Knocked, Death,
GetUp, Revive, Sleep, GetOnVehicle, Sit, Operation, Lie, GetOnTrain, Cutscene,
Whacked, Cure and Ride. Unlike Repulsed it does not forbid InAir, Jump or Fall.
It pauses actions 13, 15, 18, 19, 22, 28 and 47 and immediately ends its
recovered list, including SwordAttack 14. A request while Whacked is running
bypasses dependencies and calls the inherited empty `ResetAction`: a second hit
neither restarts nor extends the reaction.

One source quirk is preserved: without an animator, `Update` returns true, the
manager drops the action and resumes the paused actions, but `EndImmediately`
never runs, so the Whacked mask stays set.

## Repulsed

Repulsed has no state of its own. `Action_Repulsed.Update` drives four floats:
`ForwardSpeed` and `RightSpeed` from the controller's local velocity, and
`BeatWeight` and `SheildBlockF` by `Lerp(5 * deltaTime)` toward 1 and the block
flag. Cleanup sets `BeatWeight` and `SheildBlockF` to 0.

In `Base Layer.GroundLocomotion.Move`, node 0 blends by `BeatWeight` between the
normal locomotion subtree and node 193. Node 193 is a freeform-directional
blend over (`RightSpeed`, `ForwardSpeed`) with seventeen leaves:

| Point | Clip | Point | Clip |
|---|---|---|---|
| (0, 0) | NormalIdle 684 | (0, 3) | Forward 109 |
| (0, 5) | ForwardFast 226 | (2.1, 2.1) | ForwardRight 154 |
| (3.5, 3.5) | FowardRightFast 235 | (3, 0) | Right 136 |
| (5, 0) | RightFast 243 | (2.1, −2.1) | BackRight 170 |
| (3.5, −3.5) | BackRightFast 250 | (0, −3) | Back 186 → BlockRepulsed 691 |
| (0, −5) | BackFast 257 → BlockRepulsed 691 | (−2.1, 2.1) | ForwardLeft 198 |
| (−3.5, 3.5) | ForwardLeftFast 265 | (−3, 0) | Left 209 |
| (−5, 0) | LeftFast 271 | (−2.1, −2.1) | BackLeft 218 |
| (−3.5, −3.5) | BackLeftFast 278 | | |

The two straight-back points are themselves `SheildBlockF` blends from the
stagger clip to BlockRepulsed. Apart from NormalIdle, none of these clips is
used by normal locomotion: they are a separate stagger set. Seven are stored
with the clip-level mirror flag (the right-hand ones). The water Move state has
the same structure with five swim leaves.

So a knockback is presented as stagger steps in the direction the body is
actually moving, at a weight that eases in. Combined with
[HumanPhyCtrl](HUMAN_PHY_CTRL.md), a hit from the front moves the body backward
at up to 10 m/s, which lies beyond the outer ring at 5; the freeform blend then
selects by direction.

## C owner

[pe_whacked](../../rebuild/src/pe_whacked.h) holds the Whacked relation masks,
the no-op repeat, mask and trigger requests, the `MoveStyle` clip pair and
weight, and a nominal BeHit clock: state time advances by `deltaTime` over the
blended length, the exit blend runs from 0.9 to 1, and `HitAnimEnd` with the
source cleanup is reported at 1. The missing-animator quirk is kept. Clip
numbers are identities, not poses. All state is transient and unsaved.

[The exporter](../../rebuild/tools/export_reaction_presentation.py) guards the
C# class bodies, the pinned CIL bodies and their string literals, the installed
asset hashes, every transition, threshold, point, leaf, clip flag, layer field
and serialized behaviour above, and relation row 5. Its
[generated header](../../rebuild/generated/pe_reaction_presentation_source.h)
also carries the Repulsed leaf tables as data. No Repulsed blend code exists:
`pe_repulsed` already reports the parameter requests.

## Prepared engine bake

Unresolved question: the humanoid poses of the reaction clips. Source, assets
and existing captures cannot answer it, because the clips are muscle curves
that only the original engine retargets.

Smallest useful capture: one isolated female bake of twenty clips with the
existing [sampler](ANIMATOR_BAKE.md) — NormalIdle as the repeatability anchor,
IdleBeHit, SwordBeHit, the sixteen stagger leaves and BlockRepulsed. At 30 Hz
that is 836 samples, inside the 32-clip and 3,600-sample bounds. The other
eight per-weapon hit clips and the swim branch are left out until those
weapons or swimming are supported.

```sh
rebuild/build/unity-tools/bin/python rebuild/tools/inventory_reference_bake.py \
  --output rebuild/build/animator-bake/clip-plan-reaction.json
TMPDIR="$PWD/rebuild/build/compiler-tmp" python3 rebuild/tools/reference_animator_bake.py \
  --preset hit-reaction --plan rebuild/build/animator-bake/clip-plan-reaction.json
```

Both commands were run: the plan validates each leaf at its exact parameters,
values and clip, and the sampler compiles with the extended allowlist. Neither
launches anything. The supervised step adds `--launch` and a fresh `--output`
under the bake profile, then needs an operator to enter the tutorial with a
female player and create the printed `.arm` file, as for earlier bakes. It was
not run in this increment because it takes the desktop.

Review points for that bake: the seven mirrored clips should show right-hand
motion (the plan records `clip_mirror`); the sampler evaluates leaves in
isolation, so it gives neither the directional blend nor the full-body layer
mix.

## Validation and remaining gate

The registered core suite covers the relation masks, clip selection and
clamping, the 0.9/1.0 exit clock at two step sizes, repeat hits, manager
cleanup, the missing-animator quirk and atomic invalid-input failure. Fourteen
exporter cases cover source, evidence, string-literal, relation, behaviour and
controller mutations. Two new bake-tool cases cover the reaction branches and
the build-only preset. Full results are in [PLAYTEST](../PLAYTEST.md).

No game launch, desktop input, save or installed-asset change was made.
PECSv22 and live damage are unchanged.

Still unqualified:

- **Poses.** None baked; see above.
- **Animator evaluation.** The frame on which Unity consumes the trigger,
  starts the exit and calls `OnStateExit`, and how a mid-transition state is
  mixed, are engine behavior. The C clock is the authored nominal and may
  differ from the original by about a display frame.
- **Blend weights.** The freeform-directional weight algorithm and the
  `BeatWeight`/layer mixing are not ported.
- **Other reaction paths.** Hit IK (`m_ForceToHitWeight`/`m_ForceToHitTime`),
  Halt, Wentfly, Knocked, GetUp and ragdoll are separate.

Before gameplay integration: run and qualify the bake, export the clips as a
complete staged group, then compose reaction, body, attachments, defence and
cancellation with the actual host. This increment supplies no permission to
enable live stun or knockback.
