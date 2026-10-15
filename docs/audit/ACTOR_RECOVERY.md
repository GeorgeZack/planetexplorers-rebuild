# Original actor recovery and native skeletal playback

The optional C actor path now reads the installed hare's actual mesh, skin weights, skeleton and sampled idle/run/walk and five melee animations. It does not generate an imitation motion or recover the complete original actor controller. The installed original remains the reference for behavior and appearance.

## Source and recovery command

The source is the existing local installation's `AssetBundles/Monster/m_lepus_hare.unity3d`: 1,551,933 bytes, Unity 5.2.4f1, SHA-256 `94409ad1bf9570048a52c42671986907e2a5ca131f55f73737e5b7d7cff76c35`. The supplied project's `FightingTask.cs` identifies the tutorial creature as `lepus hare_2146483648`. The exported data comes from the installed bundle; the original prefab, database, installed player and bundle are distinct sources and should not be assumed interchangeable across versions.

From the repository root, using the isolated environment described in [asset recovery](ASSET_RECOVERY.md):

```sh
/tmp/pe-asset-tools/bin/python rebuild/tools/export_reference_actor.py
```

The tool requires UnityPy 1.25.4 and accepts `--game-dir` and `--output`. Its default output is ignored `rebuild/build/reference-assets/`. It stages conversion before replacing only `hare.peac`, `hare-diffuse.png` and `hare-manifest.json`; it does not replace room, tool, kit or their manifests. Output inside the original installation is rejected. Missing or corrupt source leaves existing outputs intact. No Unity editor or graphical process is needed.

The manifest records source and output hashes, object IDs, original hierarchy/path hashes, material properties, diffuse tint, clip timing, channel counts, animation bounds and verification measurements. No recovered binary assets are checked into the repository.

## Selected model and coordinate convention

The bundle contains two skinned renderers referencing the same mesh, one for the normal actor and one for its ragdoll. Export selects renderer `6629980832213904343` under:

```text
M_lepus_hare/mlepus_hare_model/lepus_hare_model/lepus_hare
```

It excludes ragdoll renderer `-8379108483813280980`. The imported model root transform is `-8365773678112902782`; mesh `706393317374358210` has 1,766 vertices, 2,180 triangles, one material, 48 skin joints and 48 inverse-bind matrices. Each vertex retains four source weights and joint indices, position, normal and UV. Weight sums and index ranges are validated.

Positions remain original Unity XYZ meters, with no X reflection or unconditional winding reversal. The mesh's original winding is retained. The actor's world position, yaw and uniform scale are supplied separately by the C caller. Diffuse pixels are exported through UnityPy with the same convention as the static exporter; the raylib adapter flips UV V. The source diffuse tint is approximately `(0.910447776, 0.910447776, 0.910447776, 1)`, rendered as `(232,232,232,255)`. Original shader, normal-map and other material behavior are not reproduced.

## Recovering animation bindings

The selected clips use Unity's dense and constant muscle-clip storage, not ordinary `m_PositionCurves`/`m_RotationCurves` arrays. They have no streamed curves. `m_ClipBindingConstant.genericBindings` identifies transform paths by CRC32 and property type: position 1 has three scalar channels, quaternion rotation 2 has four, and scale 3 has three. All selected bindings resolve to paths relative to the imported model root, such as `Bone05/Bip01`. Excluding the duplicate ragdoll hierarchy avoids ambiguous joint names.

The ordered binding scalars partition into dense channels followed by constant channels. Every frame takes one dense row, appends the constant values, and assigns each binding's full TRS component. The exporter rejects missing paths, collisions, duplicate bindings, unsupported property types, mismatched channel counts and bindings crossing the dense/constant boundary. It validates unit quaternion tolerances and normalizes their small floating-point deviations before composing TRS matrices.

Parent-first hierarchy composition retains intermediate bones, including the source's `Bone05` rotation/0.2 scale and `Bip01` scale near 5. These transforms must not be discarded just because their product appears close to a simpler pose. For each frame and skin joint:

```text
skin_palette = inverse(model_root_world) × animated_bone_world × mesh_inverse_bind
```

The palette therefore transforms original mesh vertices directly into model-local animated vertices. Native playback must not apply inverse binds again. The exported inverse-bind array remains available for provenance and independent bind-pose checks. The serialized hierarchy is already posed; it is not assumed to equal the mesh's bind pose.

| Native clip | Source clip | Dense frames | Rate | Authored duration | Dense / constant scalars |
| --- | --- | ---: | ---: | ---: | ---: |
| `idle` | `normal_leisureidle` | 72 | 30 Hz | 2.333333492 s | 312 / 318 |
| `run` | `move_run` | 20 | 30 Hz | 0.600000262 s | 310 / 310 |
| `walk` (index 7) | `move_walk` | 38 | 30 Hz | 1.200000048 s | 309 / 311 |

The idle/run/walk clips loop and report zero average root speed; the five exported melee clips are nonlooping. Dense storage includes terminal guard samples; playback wraps at the authored duration, **not** `frame_count / fps`. Original frames and guard samples are retained rather than resampled to a made-up duration. Root-motion extraction and the original Animator state machine are not implemented.

## PEAC version 1

All numeric fields are little endian. Matrices contain the first three rows of a 4×4 affine matrix in row-major order; the final row is implicitly `(0,0,0,1)`.

| Section | Layout |
| --- | --- |
| Header, 24 bytes | `PEAC` magic; u32 version 1, vertex count, index count, joint count, clip count |
| Vertices, 56 bytes each | f32 position[3], normal[3], UV[2], weights[4]; u16 joints[4] |
| Triangle indices | u32[index count], three per triangle |
| Inverse binds | joint count × 12 f32 |
| Clip header, 48 bytes | NUL-padded ASCII name[32]; f32 FPS; u32 frame count; f32 duration; u32 loop flag |
| Clip palettes | frame count × joint count × 12 f32; frame-major, then original skin-joint order |

Each clip header immediately precedes its palettes. No trailing bytes are accepted. The eight-clip hare file is 835,096 bytes (the initial idle/run-only file was 339,448 bytes). The format is a bounded local conversion artifact, not a Unity asset container, game save or general interchange format.

`pe_actor.c` validates file size, counts, indices, weights, finite numeric ranges, affine invertibility, unique clip names and frame coverage before exposing data. It stores inverse binds but applies only the already-composed palettes during animation. The CPU skinning API writes caller-owned vertex arrays without per-frame allocation. The raylib adapter expands triangle indices into a dynamic mesh, uploads changed positions/normals and owns its model and optional diffuse texture until unload.

Between samples, C playback linearly interpolates the positions and normals skinned at the two neighboring frames. This is not the original quaternion/TRS interpolation and can differ between authored samples. Normals use inverse-transpose palette matrices and normalization; near-canceling normals fall back to the source normal. Animator transitions, blending, IK, event dispatch and ragdoll remain separate work. Recovered animated capsule geometry is available through the headless APIs and optional preview overlay described in [hitbox recovery](HITBOX_RECOVERY.md); damage processing and attack integration remain separate. Attack timing metadata is recovered below; the renderer does not dispatch it.

## Melee clips and event provenance

The exporter resolves controller `4131577587453613724`, its state constants, clip references and `m_StateMachineBehaviourVectorDescription` ranges. It then verifies each selected `skMain._effPrepare` record against `skEffect._anms`. This establishes a database skill → named state → actual clip relationship instead of guessing from clip names. Existing native clip indices 0/1 remain idle/run, and `walk` appends at index 7 after all five attacks; the following nonlooping clips append at indices 2–6. Callers should use `pe_actor_find_clip` by name.

| Native name | Source clip | Skill → prepare effect | Duration / dense frames | Active normalized interval and bone |
| --- | --- | --- | --- | --- |
| `attack0` | `attack0_handhitleftfront` | 30100003 → 100 | 1 s / 32 | .372–.642, right forearm |
| `attack1` | `attack1_handhitrightfront` | 30100004 → 101 | 1.000000954 s / 32 | .372–.642, left forearm |
| `attack2` | `attack2_handhitfront` | 30100008 → 102 | 1.500000477 s / 47 | .24–.34 right; .50–.66 left |
| `attack3` | `attack3_normalattack0` | 30100046 → 103 | 1 s / 32 | .241–.631, right forearm |
| `attack4` | `attack4_normalattack1` | 30100047 → 104 | 1.066667080 s / 34 | .419–.700, both forearms |

All five are sampled at 30 Hz and controller state speed 1. The apparent left/right mismatch in some **clip names** is preserved: collision bones above come from actual state behaviour fields, not English interpretation of the motion name. Maximum vertex displacement from frame zero is approximately 2.081, 2.172, 2.026, 2.242 and 2.192 m respectively.

Their `AnimationClip.m_Events` lists are empty. The active collision windows are stored in `PEAnimatorAttackTrigger` and `PEAnimatorAttackTriggerEX` state behaviours. The manifest retains both the empty raw clip event lists and the actual behaviour records, including source class, object ID, normalized windows, bones, controller speed and derived seconds. It also records source-file hashes for the database, behaviour implementations and death handler.

`PEAnimatorAttackTrigger.cs` activates its attack triggers when normalized state time is within **inclusive** start/end bounds, and clears/disables them outside the window and on state exit. `PEAnimatorAttackTriggerEX.cs`, used by attack2, activates each window at its inclusive start and disables it at its **exclusive** end, resetting hit information and sending `MonsterEndAttack`. Its two windows are approximately .360–.510 s and .750–.990 s at source speed. Attack4's .419–.700 normalized interval means approximately .446934–.746667 seconds, not .419–.700 seconds. Original skill timing, behaviour cooldown, Animator transition timing and actual collision remain separate gates; these intervals must not automatically award damage.

The manifest fields `clips[].combat.attack_windows` and `clips[].combat.state_behaviours` carry this evidence. PEAC v1 remains a rendering/pose format: the binary does not silently turn this metadata into a gameplay event interpreter. Pounce is deliberately outside this selection; its separate movement and skill rules are described in [training enemy](TRAINING_ENEMY.md).

### Hurt and death are not missing named clips to guess

No hurt/death clip or state exists in this installed hare controller. The source `BiologyViewCmpt.OnDeath` calls `ActivateRagdoll(null, false)`; the bundle includes a separate ragdoll hierarchy and physical joints. Exporting an arbitrary attack, sleeping pose or reversed motion as a death animation would be false recovery. Accordingly no `hurt` or `death` clip is manufactured. Future death/injury replication requires the appropriate reaction/ragdoll path, while a deliberately frozen or removed C actor must be documented as a substitute. The manifest records this absence and all controller state names for inspection.

## Native preview and training placement

```sh
./rebuild/build/planet_explorers \
  --reference-actor rebuild/build/reference-assets/hare.peac

./rebuild/build/planet_explorers \
  --reference-actor rebuild/build/reference-assets/hare.peac \
  --smoke 90 --screenshot /tmp/pe-hare.png
```

The preview uses 1 for idle, 2 for run, Left/Right to select among all clips, R to restart the selected clip, Space for pause, H for the optional recovered capsule overlay, right drag for orbit, wheel for zoom and Escape to exit. A smoke run below 180 frames plays idle/run; longer runs cycle all clips and enable the overlay when an adjacent `hare.pehb` is present. Runs fail on incomplete execution, loading, posing or screenshot errors. The new overlay is described in [hitbox recovery](HITBOX_RECOVERY.md); its graphical validation is still pending. The adjacent `hare-diffuse.png` is used when present. The root agent completed the 90-frame idle/run graphics smoke and visually inspected coherent textured limbs and deformation; [the run screenshot](../reference/rebuilt-hare-run.png) is retained. A 1,500-frame training sequence also passed at mission 746 with the optional hare loaded, knife owned/equipped, nine Herbal Juice and 500 player HP. These checks do not establish attack playback, damage, enemy death or combat parity; no original live damage/kill measurement is claimed.

Tutorial now connects the actor to actual hostile contacts/death through [training melee](TRAINING_MELEE.md) and [training combat](TRAINING_COMBAT.md). Adventure has a separate bounded [hunt host](ADVENTURE_HUNT_COMBAT.md). The early leisure-only/no-target checkpoint is obsolete. Missing optional actors never create an animated substitute.

## Verification

```sh
python3 rebuild/tests/test_reference_actor.py
/tmp/pe-asset-tools/bin/python rebuild/tests/test_reference_actor.py
```

The standard-library run passes three math/binding tests and skips two optional installed-asset tests. With UnityPy and the installation, all five pass. They verify model-root cancellation, one-time inverse-bind application, binding partition rejection, quaternion/hierarchy validation, source counts/timing, exact skill/state/clip mappings, attack-window bounds and events, full binary parsing, actual deformed vertex positions, idle/run loop closure, hashes, repeatable output and preservation of previous files on missing/corrupt source.

Measured maximum vertex displacement from frame zero is 0.0847009 m for idle and 1.3500423 m for run. The corresponding loop-endpoint gaps are below 0.0000003 m and 0.00005 m. The newly appended walk clip retains its source endpoint difference of approximately 0.0002726 m; it is not artificially closed. These are computed from recovered samples, not inferred from clip names. Native loader/skinning tests also passed with AddressSanitizer and UndefinedBehaviorSanitizer against both malformed synthetic fixtures and the exported hare; they check weighted poses, inverse-transpose normals, looping by duration, truncation, invalid indices and non-finite data. They establish the converted data and supported playback subset, not complete original-animation or game parity.
