# Solver poses for the player clips

October 13, 2026. **Derived offline from existing engine bakes and implemented
in a disconnected C loader.** `pe_ik_pose` supplies the animated local pose of
the 25 solver transforms for each staged female clip, which is the input
[the full-body solver](FINALIK_FBBIK.md) needs. No host calls it and nothing is
drawn from it. No game was launched for this.

## Why it was missing

The staged player actors are skin palettes baked per frame. They carry no bone
hierarchy and no local transforms, and they omit bones that skin nothing
(`Bip01`, `Bip01 Spine`). The solver needs every transform from the model root
to the hands and feet.

The isolated-Animator bake captures that produced those actors already record
all 78 nodes per sample. `export_player_ik_pose.py` reads the same captures,
through the same validating analyzer, and keeps the solver's bones.

## Export

For each selected clip and sample the exporter takes the model-space matrix of
every animated solver bone, derives its local transform against the solver
parent, and stores position and unit quaternion. It refuses a bake whose
hierarchy differs from the generated rig, a missing bone, and any scale, shear
or mirroring. `FemaleModel`, `Model` and `IK` sit above the baked Animator
root and are stored as identity.

The staged group is `rebuild/build/reference-assets/player-ik-pose`
(`player-ik-pose.peik` and its manifest): 31 clips, 991 samples, the same
clips and captures as the staged defence actor. The format is PEIK1: bone
parents, then per clip a name, rate, sample count, duration and loop flag, then
seven floats per bone per sample. The final sample is exactly at the clip's
duration, as in PEAC2. The installer adds a new group and refuses to replace
one. Recovered assets are not committed.

```sh
python3 rebuild/tools/export_player_ik_pose.py --stage DIR --capture BAKE.jsonl=Clip,Clip ...
python3 rebuild/tools/export_player_ik_pose.py --stage DIR --install
```

## Loader

[pe_ik_pose](../../rebuild/src/pe_ik_pose.h) is a strict bounded loader. It
requires the file's bone count and parents to equal the given rig's, finite
values, unit quaternions and exact length, and fails closed otherwise. Sampling
uses the clip clock of `pe_actor_pose`; positions lerp and rotations take the
normalized shortest lerp. That interpolation is a rebuild adapter: the original
Animator evaluates its own curves.

## What the real poses show

Running `pe_fbbik` over all 991 staged samples:

- **Every solve succeeds**, and a 0.2 m body offset moves the body by up to
  0.16 m.
- **Baked limb segments match the prefab rest lengths to a micrometre**, so the
  solver's rigid-length assumption holds for these clips.
- **With no offsets the solve is not always the identity.** It is exact (under
  0.01 mm) on 21 of 31 clips and moves a bone by up to 1.9 cm on the others
  (`RunSwordAttack` 19 mm, the walk strafes 13 mm, `NormalWF` 7 mm,
  `NormalIdle` 5.5 mm). All 61 limb samples extended beyond 99.9% of their
  length move, as the solver's limb limit requires; 111 other limb samples
  move as well, most likely through the bend constraint, which was not
  isolated.

The last point agrees with the original: in the observed session's 1,082
frames without effects, the original's own solve moved bones by up to 3.3 mm,
and `pe_fbbik` reproduced those frames. So the original applies this small
correction every frame the solver is enabled, not only during an effect.

## From a solve to skin

[pe_ik_skin](../../rebuild/src/pe_ik_skin.h) turns an animated pose and its
solved pose into one rigid model-space motion per solver transform. Whatever is
rigidly attached to a bone in the animated pose, such as a skin palette row, a
defence capsule or an attachment, follows by that bone's motion. It is pure
arithmetic and no host calls it.

That only works if the staged poses and the staged skin palettes are the same
motion in the same space. They are: a point skinned to a joint of the staged
defence actor stays fixed in the corresponding solver bone's animated frame to
2.4 micrometres, for all 20 joints the two share, over all 991 samples.

## Verification

The core suite builds a synthetic PEIK file and checks clip metadata, sampling
at and between samples, the shortened final interval, clamping, looping, and
twelve malformed files. `test_player_ik_pose.py` checks the decomposition
against the rig's rest locals, refusal of scale, encoding bounds, and that
exported poses sample correctly in C; when the staged group is installed it
also runs the solver over every real sample and checks the poses against the
defence actor's palettes. Those two tests are skipped without the staged
assets, and a skip is not a pass. The `ik_skin` core suite checks identity on
an unchanged pose, that each delta carries its bone and a point fixed to it
onto the solved bone after a real solve, orthonormality, and atomic failure.

```sh
python3 rebuild/tools/run_tests.py -k ik_
```

## Remaining boundary

- **Applying a solve to the drawn character.** The per-bone motions exist; which
  bone drives each skin joint of each rendered part (fingers, twist bones and
  attachments follow their nearest solver ancestor) is not exported, and
  `pe_actor` has no entry point that takes them.
- **Blended poses.** The rebuild blends skinned vertices across clips, not
  bone transforms; what pose the solver should read during a blend is
  undecided.
- **Other clips and the male rig** are not staged.

No gameplay gate was opened. PECSv22 is untouched.
