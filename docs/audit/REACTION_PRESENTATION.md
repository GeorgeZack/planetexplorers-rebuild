# Whacked and Repulsed animation contract

October12 bake: the prepared engine bake below was run and qualified, and its
25 clips are staged as a standalone, uninstalled group. See
[Bake result and staged group](#bake-result-and-staged-group). Statements
below that no reaction pose exists describe the state before that capture.

October12 follow-through: [Wentfly/Knocked/GetUp lifecycles](RAGDOLL_ACTIONS.md) and [hit IK](HIT_IK.md) now have disconnected C request owners. Their source requests do not recover ragdoll physics, FinalIK solving or reaction/get-up poses.

October 12, 2026. Source-derived from the installed controller, checked-in C#
and installed CIL, with a disconnected C owner for the Whacked lifecycle.
Neither gameplay host calls it. No reaction pose is installed or presented in
the rebuild; live hit reactions, stun and knockback remain gated.

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

Smallest useful capture: one isolated female bake of twenty-five clips with the
existing [sampler](ANIMATOR_BAKE.md) — NormalIdle as the repeatability anchor,
IdleBeHit, SwordBeHit, the sixteen stagger leaves and BlockRepulsed, plus the
next animation set described below (four ground get-up clips and
SprintSwordAttack) so one session answers all of them. At 30 Hz that is 1,052
samples, inside the 32-clip and 3,600-sample bounds. The other eight
per-weapon hit clips, the swim branch and the water get-ups are left out until
those weapons or swimming are supported.

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

## Bake result and staged group

Observed in the original, October 12: `female-reaction-01.jsonl` was captured
in a supervised session (tutorial, female player, `-force-gfx-direct`, no Zink
override, Steam client running). It holds all 25 clips and 1,052 poses, ends
`complete`, and the player log carries the 120-frame post-cleanup marker. The
process then ended with signal 9, as on earlier bakes; teardown is not
validated. [Compact evidence](../reference/reaction-pose-result.json) records
hashes, per-clip figures and the checks below.

Strict analysis passes: every clip moves all 42 mapped bones, none is missing,
and the maximum hierarchy matrix error is 1.07e-6. Three cross-checks were
made on the capture:

- **Anchor.** NormalIdle matches validated capture 06 to 4e-7 in local and
  world matrices when matched by bone path. This capture's hierarchy has 78
  nodes against 77, so index order is not comparable.
- **Mirroring.** The engine applied authored clip mirroring in the isolated
  graph. GetUpBack/GetUpBackMirror, GetUpFront/GetUpFrontMirror, Left/Right
  and LeftFast/RightFast are left/right mirror images to within 3.5 mm at the
  hands, against 0.13–0.45 m unmirrored. A native mirror pass must not be
  added on top of these poses.
- **BlockRepulsed** moves each hand about 1.5 cm over the clip. It is a near
  static braced pose, not a sampling failure: all 42 bones change.

Implemented as staged data only: `rebuild/build/starter-pair-reaction-01` is
a complete 25-clip group with the body, Wood Sword drawn/stowed and Wood
Shield attachments and a 25-clip defence actor, all on identical clocks. Its
16 defence capsules are byte-identical to the 33/42 stage's.

```sh
python3 rebuild/tools/analyze_reference_bake.py \
  ~/.local/share/planet-explorers-bake-reference/pose-captures/female-reaction-01.jsonl \
  --rig rebuild/build/reference-player-starter/player-female-starter-rig.json \
  --report rebuild/build/animator-bake/female-reaction-01-report.json \
  --export-dir rebuild/build/animator-bake/player-female-reaction
TMPDIR="$PWD/rebuild/build/compiler-tmp" rebuild/build/unity-tools/bin/python \
  rebuild/tools/export_starter_pair_assets.py --pose-set reaction \
  --capture ~/.local/share/planet-explorers-bake-reference/pose-captures/female-reaction-01.jsonl \
  --body-scene rebuild/build/animator-bake/player-female-reaction/all/recording.peas \
  --output rebuild/build/starter-pair-reaction-assets-FRESH
TMPDIR="$PWD/rebuild/build/compiler-tmp" rebuild/build/unity-tools/bin/python \
  rebuild/tools/export_starter_pair_unified.py --standalone-reaction \
  --sword-pair rebuild/build/starter-pair-reaction-assets-01 \
  --sword-body rebuild/build/animator-bake/player-female-reaction/all \
  --sword-capture ~/.local/share/planet-explorers-bake-reference/pose-captures/female-reaction-01.jsonl \
  --output rebuild/build/starter-pair-reaction-FRESH
```

The group is standalone because the retained defence actor has 42 clips and
24 more would pass the native 64-clip actor limit. The
[33/42 stage](STARTER_SWORD_POSE_GROUP.md) is unchanged. The user chose a
second host group over raising the limit; see
[Host reaction group](#host-reaction-group).

Twenty-one stage and attachment tests pass with UnityPy and no skips; the ten
stage tests also pass under ASan/UBSan, including native loading of all four
scenes and every defence frame of all 25 clips. These are headless checks. No
asset was installed, no gameplay changed and no native graphics ran.

## Host reaction group

Implemented in C, October 12, as loading and validation only.
`pe_sandbox_actor` accepts an optional second group beside the 33-clip one,
under these folders of the asset root:

| Host folder | Staged source |
|---|---|
| `player-reaction` | `player` |
| `player-reaction-wood-sword-stowed` | `player-wood-sword-stowed` |
| `player-reaction-wood-sword-drawn` | `player-wood-sword-drawn` |
| `player-reaction-wood-shield` | `player-wood-shield` |
| `player-reaction-defence` | `player-defence` |

The four scenes are all-or-nothing. Each must hold exactly the 25 clips in
capture order, with the exact recovered frame counts, durations and loop
flags; NormalIdle must equal the body's own NormalIdle; and every attachment
clock must equal the reaction body's. Any difference fails the whole actor
load before GPU upload. An entirely absent group changes nothing.

When the group is present, the sandbox host also requires
`player-reaction-defence` with the same 25 ordered clocks and loadable
capsules, and refuses to start without it. The 42-clip defence actor is
rejected in that role.

`pe_sandbox_actor_reaction_ready` and
`pe_sandbox_actor_reaction_defence_ready` report the result. Nothing reads
them to draw: no reaction clip is selected, drawn or offered as an action
pose, and existing knife, sword, shield, tool and running readiness are
unchanged with the group loaded. Readiness is not permission to present a
reaction, get-up, sprint attack or block.

The group is **not installed**. `rebuild/build/reference-assets` has none of
these folders, so the live game loads exactly what it did before. Installing
is a copy under the names above and belongs with host composition. Until
something presents the clips, an installed group would also upload a second
copy of the body mesh and textures for no visible effect.

The mocked actor test covers every incomplete combination, wrong counts,
names, order, anchor, source clock, loop flag and attachment clock, and the
defence check. A real-asset test loads the installed assets with the staged
group mapped to the host names, confirms unchanged readiness, the 25-clip
defence with its 16 capsules, rejection of the 42-clip actor, and that an
idle draw touches no reaction scene. Both are headless; the sandbox host's
own start-up path with the group present has not been run in a window.

## Next animation set: get-up and sprint attack

The clip/state identities below were recovered from the installed controller
on October 12. The [action lifecycle](RAGDOLL_ACTIONS.md) is now recovered
separately; no get-up pose or state presenter is qualified yet.

**Halt, Wentfly and Knocked play no clip.** `Action_Halt` runs a procedural
`IKAnimEffectCtrl.StopMove` on the controller velocity, now
[recovered separately](HALT_STOP_EFFECT.md). `Action_Wentfly` and
`Action_Knocked` hand the body to the ragdoll. They need IK and ragdoll owners
or explicit adapters, not a bake.

**Get-up.** `Action_GetUp` deactivates the ragdoll and holds its mask until the
view reports the ragdoll gone. The clips are four base-layer states under
`Ragdoll`, each a direct speed-1 leaf entered by its own Any State trigger and
left at normalized time 1. State and clip names are crossed in the controller:

| State | Trigger | Clip | Length |
|---|---|---|---|
| GetupBack | GetUpBack | GetUpBackMirror 322 (mirrored) | 1.4667 s |
| GetupFront | GetUpFront | GetUpFrontMirror 326 (mirrored) | 1.0667 s |
| GetupBackMirror | GetUpBackMirror | GetUpBack 311 | 1.4667 s |
| GetupFrontMirror | GetUpFrontMirror | GetUpFront 317 | 1.0667 s |

Each entry is also conditioned on `ragdollInWater`; four `_Water` states hold
the swimming variants. The clips carry no events. Which trigger the ragdoll
controller picks, and its pose blend from the ragdoll, are not recovered.

**Sprint attack.** `FullBody.Attack.SprintSwordAttack` is a direct leaf of clip
401 (male override 310), 1.8 s, entered by the `SprintSwordAttack` trigger. Its
authored events are StartAttack 0.1685 s, WeightInputStart 0.3503 s, EndAttack
0.5680 s, WeightInputEnd 0.7164 s and EndAction 1.1924 s. The original selects
it above the sword's sprint threshold (7 m/s in the installed prefab).

The weapon's sprint skill is 20110013. Its effect row 41 plays
`SprintSwordAttack, attack7, attack8, attack9`, so a chained sprint attack
continues with the ordinary second to fourth swings, exactly as the running
attack does after `RunSwordAttack`. It uses the same damage trigger 237 and the
same three combo conditions. Unlike the running (20110014) and stationary
(20110011) skills it is not marked interruptable; what that flag gates in the
skill system is not traced here. Compared with the running lunge the blade
opens earlier (0.169 s against 0.297 s) and closes earlier (0.568 s against
0.709 s).

Implemented in C on October 12 as timing and admission only:
`PE_COMBAT_SPRINT` carries the clip-401 event clock and the stage sequence,
and `pe_sandbox_melee` accepts the sprint branch only when a host declares
`sprint_motion_ready` with matching SprintSwordAttack poses. No host does.
**In the live game a press above 7 m/s is still rejected before any stamina
debit**, the motion router refuses a sprint first stage rather than borrow the
running curve, and nothing about sprinting or attacking changes.

To open it: bake the pose (in the prepared preset), stage body, held weapon
and defence clips as a complete group, and capture its root motion with the
existing root sampler as was done for clip 408. That root capture is a second,
separate probe.

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

- **Poses.** Baked and staged as isolated leaves, not installed or presented.
  The directional blend, layer mix and ragdoll-to-get-up blend are not in them.
- **Animator evaluation.** The frame on which Unity consumes the trigger,
  starts the exit and calls `OnStateExit`, and how a mid-transition state is
  mixed, are engine behavior. The C clock is the authored nominal and may
  differ from the original by about a display frame.
- **Blend weights.** The freeform-directional weight algorithm and the
  `BeatWeight`/layer mixing are not ported.
- **Other reaction paths.** Hit IK (`m_ForceToHitWeight`/`m_ForceToHitTime`),
  Halt, Wentfly, Knocked, GetUp and ragdoll are separate.

Before gameplay integration: capture SprintSwordAttack root motion, install
the reaction group under its host names, then compose reaction,
body, attachments, defence and cancellation with the actual host. This increment supplies no permission to
enable live stun or knockback.
