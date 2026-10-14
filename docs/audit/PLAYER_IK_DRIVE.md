# Always-on FinalIK solve in both hosts

October 13, 2026. **Implemented in C and connected to drawing in the sandbox
and tutorial hosts.** `pe_ik_drive` runs [the full-body solver](FINALIK_FBBIK.md)
on [the staged solver poses](PLAYER_IK_POSE.md) every drawn frame, with no
effector offsets, and the player's skin follows the result. It is the smallest
visible use of the recovered chain: a correction of millimetres to two
centimetres. No Halt, hit, grounder or other offset is supplied, no gameplay
gate is opened, and nothing is saved.

This is presentation only. Blade capsules, incoming-defence capsules,
root motion and every contact query still read the baked clip pose.

## What the original does

Source-derived from `IKCmpt.cs`, `BiologyViewCmpt.cs` and `Action_Tool.cs`:

- **The solver is enabled every frame near the camera.** `IKCmpt.UpdateFBBIKState`
  enables `FullBodyBipedIK` when the character is within 32 m of the main
  camera and has a grounder; the female player has one.
- **It stops while ragdolled.** `ActivateRagdoll` sets `ikEnable` false, which
  sets the solver weight to 0. Climb, drive, leisure, live and train actions do
  the same; the rebuild has none of them.
- **Held hand equipment switches the arm mapping off.**
  `Action_HandChangeEquipHold` and `Action_AimEquipHold` call
  `SetSpineEffectDeactiveState`, which sets both arm mapping weights to 0 and
  the grounder's spine bend to 0. Swords, knife, torch and axes use the first
  action, dig tools the second. The arms then keep their animated pose under
  the solved chest.

Observed in the original (October 12 capture, unarmed): solver weight 1 and
arm mapping weights 1 on every frame, and a solve that moved bones by up to
3.3 mm in frames without any effect.

## Implemented in C

[pe_ik_drive](../../rebuild/src/pe_ik_drive.h) loads two staged groups and
answers two questions:

- **Which solver bone moves each skin joint?** `player-ik-drivers` (PEID1)
  lists, for every part of every installed player scene, one solver bone per
  joint: the joint's own bone when the solver owns it, otherwise its nearest
  solver ancestor. Entries are keyed by group folder and part order, and bound
  to the exact actor geometry by `pe_actor_skeleton_hash`. A drawn and a
  stowed knife share geometry but follow different bones, which is why the
  key is the group and not the geometry.
- **What are the bone motions this frame?** `pe_ik_drive_solve` takes the
  drawn clip samples by name, blends their staged bone poses, solves, and
  returns one rigid model-space motion per solver bone from `pe_ik_skin`.

`pe_actor_pose_blend_driven` applies those motions: each joint's blended
palette is moved by its driver bone's motion before skinning. With identity
motions the positions equal the existing blend to float rounding.
`pe_actor_scene_view` gains `bind_ik` (attach a group's drivers to an uploaded
view) and `set_ik` (copy one frame of motions for the following draws).

Both hosts follow the same rules:

| Rule | Reason |
|---|---|
| Solve from the same clips, clocks and weights that are drawn | one pose for body and attachments |
| Arm mapping off while equipment is drawn | source, above |
| No solve when HP is 0 | source: ragdoll disables IK |
| No solve in the inventory preview | the preview is a separate idle display |
| Every drawn scene must be bound, and every blended clip staged, or the whole pose stays baked | never mix a solved body with a baked attachment |
| A present staged group that fails to load fails startup; absent groups keep the baked presentation | the recovered-asset rule |

The sandbox torch light and flame positions follow the hand through the same
solve as the visible torch.

## Adapters, not original behavior

- **The pose read during a blend** is the weighted blend of single-clip bone
  poses: positions averaged, rotations summed in one hemisphere and
  normalized. The original solves the Animator's own controller pose. The
  motions are then applied to the existing frame-skinned vertex blend, which
  is a slightly different pose; for corrections this small the mismatch is
  second order.
- **Fingers, toes, twist, face and mount joints** follow their solver ancestor
  rigidly. Their own animated motion is already in the baked palettes.
- **Normals** of a driven pose come from the moved blended palette, not from
  per-sample normals.
- **The solver initiates from the prefab rest pose.** Which pose the original
  holds at `Start` is not qualified; the observed replay was insensitive to it.
- **Arm mapping weight 0 is source-derived and unobserved.** The capture was
  unarmed.
- **Contact geometry is not driven.** In the original, colliders are children
  of the solved bones. Here capsules stay on the baked pose, so a blade or
  defence capsule can differ from the drawn mesh by the size of the
  correction (see below). Changing that would move qualified contact timing
  and needs its own decision.
- **Wood Sword locomotion clips are not staged**, so Sword poses stay baked.
  Reaction scenes are never drawn.

## Size of the correction

Measured headlessly by the real sandbox actor over the installed assets, as
the largest vertex displacement between the baked and driven pose
(`test_player_ik_drive.py`):

| State | Largest displacement |
|---|---|
| Idle | 5.6 mm |
| Walk, turn | 7.4 mm |
| Run | 2.6 mm |
| Knife stowed / drawn, standing | 5.5 / 5.6 mm |
| Knife attacks 6–9 | 1.0 mm |
| Running attack | 18.8 mm |
| Shovel, axe, pickaxe actions | 5.6 mm |
| Walking with a drawn torch | 0.001 mm |

Standing poses have nearly straight legs, which the solver's 99.9% limb limit
bends slightly. With the arm mapping off, a walking pose is left alone.

## Export

`export_player_ik_drivers.py` reads the installed player scenes, the saved
starter rig (per-part bone names) and one bake capture's skeleton row
(hierarchy paths). It launches nothing.

- Body parts: joint order is the source mesh bone order, checked by comparing
  each actor's bind matrices with the starter rig's inverse binds.
- Mounted parts (tools): one rigid joint at the mount path their own exporter
  recorded; the driver is the nearest solver bone on that path.
- The defence actor is included for later use; nothing reads its entry.

The installed group holds 64 parts across 23 groups. The installer adds a new
group and refuses to replace one. A reinstalled or changed scene needs a new
export, because entries bind exact geometry.

```sh
python3 rebuild/tools/export_player_ik_drivers.py --stage DIR --capture BAKE.jsonl
python3 rebuild/tools/export_player_ik_drivers.py --stage DIR --install
```

## Verification

- `ik_drive` core suite: synthetic PEIK and PEID files; lookup by group, part
  and geometry; a settled pose solves to the identity within 1e-4; a straight
  pose moves and stays rigid; with the arm mapping off the arm motion equals
  the chest's; invalid samples leave the output alone; truncated and eight
  kinds of malformed table fail closed.
- `actor` core suite: identity motions reproduce the blend; a translated and
  a turned bone move their joints by the weighted formula; joint points
  follow; invalid drivers, nonfinite or flattening motions leave the output
  alone.
- `actor-blend-test`: the real view and scene-view code with stubbed graphics:
  binding is all-or-none, a frame is copied, blends and single clips are
  driven, and a refused motion falls back to the baked pose.
- `sandbox-actor-test`: the real host with mocked views: driven when alive,
  clips passed by name with drawn clocks and weights, arm mapping off only
  when equipment is drawn, baked in the preview and when dead, baked as a
  whole when one scene is unbound or the solve fails, and the torch emitter
  follows.
- `test_player_ik_drive.py`: exporter rules on synthetic input; with the
  installed assets, the manifest hashes every part and the real sandbox actor
  draws idle, walk, run, turn, stop, knife, attacks, running attack, tools and
  torch through the solve on the CPU. Those asset tests are skipped without
  the staged groups, and a skip is not a pass.
- Rendered: both hosts pass their smokes with the groups installed. The
  sandbox 90-frame smoke reports 90 poses drawn through the solve, the
  tutorial 1,500-frame smoke 1,353. The same tutorial frame rendered without
  the groups differs only in scattered pixels along the player's arm, hip and
  leg.

```sh
python3 rebuild/tools/run_tests.py -k ik_drive
make -C rebuild actor-blend-test sandbox-actor-test
```

Counts and logs are in [PLAYTEST](../PLAYTEST.md).

## Remaining boundary

- **No comparison of the drawn result with the original.** The solver is
  observed on the original's own inputs; the rebuild's blended input pose is
  not the Animator's.
- **Halt offsets are connected** since the user's October 13 decision; see
  [the Halt audit](HALT_STOP_EFFECT.md#live-in-both-hosts). **The grounder is
  connected** too: [its audit](FINALIK_GROUNDER.md#live-in-both-hosts). Hit
  offsets are not.
- **Contact geometry** stays on the baked pose.
- **Male rig, other clips and NPCs** are not staged.

Live reactions, stun/knockback, get-up, shield, pounce/aggression and Town
Defense gates stay closed. PECSv22 is untouched.
