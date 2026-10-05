# Offline player pose-capture analysis

`rebuild/tools/analyze_reference_pose.py` validates the observation-only `ReferencePoseProbe` JSONL format and can convert a selected interval into native sampled player parts. The tool uses Python's standard library, launches no process other than its own CLI, sends no input, and performs no Unity evaluation. A completed original-runtime capture now passes the strict validator and produces recorded player scenes. The first default-body scene also passed a native graphical preview. This is recorded pose recovery, **not general humanoid retargeting**; source contact activation and successful damage remain unobserved.

This follows [player mesh/Avatar recovery](PLAYER_ASSET_RECOVERY.md) and [player animation/event recovery](PLAYER_ANIMATION.md). The input rig is produced by `export_reference_player.py`; the runtime sampler is prepared separately by `reference_pose.py`. A sampler that compiles or loads is not evidence that it has captured the intended character or animation.

Subsequent [isolated authored-clip baking](ANIMATOR_BAKE.md) recovers idle, locomotion, jump and knife clips through a separate pure Animator hierarchy. Its valid pose outputs and observed process-lifetime failures are documented separately from this observation-only capture.

## Commands

After the operator supplies a completed original-runtime capture:

```sh
python3 rebuild/tools/analyze_reference_pose.py /path/to/capture.jsonl \
  --rig rebuild/build/reference-player/player-male-default-rig.json \
  --report rebuild/build/pose-analysis/report.json
```

Use the female rig for a female capture. Gender and the serialized Avatar name must agree; the analyzer refuses to guess. `--first-sample N --last-sample M` selects an inclusive interval after validating the entire input. The report includes active Avatar/Animator identity, exact captured bone paths and instance IDs, missing source bones, skin-reference membership, per-layer clip/state/transition spans, normalized-time endpoints and clip-weight ranges. It also retains captured HumanTrait names, limits and bone/muscle index mappings.

A complete, successfully mapped interval can be exported:

```sh
python3 rebuild/tools/analyze_reference_pose.py /path/to/capture.jsonl \
  --rig rebuild/build/reference-player/player-male-default-rig.json \
  --report rebuild/build/pose-analysis/report.json \
  --first-sample 120 --last-sample 180 \
  --export-dir rebuild/build/pose-analysis/recording --fps 30
```

The numbers above illustrate interval selection; they do not identify a measured Attack6 interval. Select actual samples from the report. At least two poses are required. A missing optional facial bone prevents exporting that part; repeated `--part-index N` flags explicitly select a smaller subset (indices 0–3 are torso, legs, hands and feet in the current default-rig exporter). Analysis still reports all missing bones. No bone is silently substituted.

`--allow-incomplete` permits analysis of a **stable snapshot** missing its final end record. It still rejects a truncated JSON line, and incomplete captures cannot produce PEAC recordings. Stop capture or copy a stable snapshot before analysis; do not analyze a file while it is being appended. Existing reports are replaced atomically only after successful analysis. Actor output is staged before replacing generated files, preserving unrelated files in the destination.

## Validation and mapping

The reader rejects files above 256 MiB, lines above 4 MiB, nonfinite JSON constants, duplicate JSON keys and unsupported headers. It requires schema 1, row-major matrices and `WaitForEndOfFrame` sampling. Capture bounds cannot exceed 3,600 frames. Samples must start at zero and remain sequential; Unity frame IDs and real time increase strictly, while scaled and fixed time may stay unchanged during a pause. End-record frame counts must match.

Each transform must have a unique nonzero instance ID and unique path. Local position/quaternion/scale must reconstruct the captured local matrix; quaternions must have unit length within float tolerance. For every represented parent, `parentWorld × local` must match the child's captured world matrix. All non-root model transforms require their parent and must lie below the selected model root. Cycles, inconsistent paths, missing parents, conflicting duplicate root records, non-affine matrices and invalid Animator inverse matrices fail validation. Matrix comparisons allow 0.001 absolute plus 0.00002 times the largest expected matrix element for float accumulation at world offsets; the maximum measured discrepancy is reported rather than hidden.

Animator, layer, clip, renderer and bone-reference counts are bounded. Numeric state times, weights, speeds and parameters must be finite. Renderer/human-bone references must point to represented transforms. Disabled/inactive animators are not selected. Exactly one enabled, active Animator must match the exported Avatar name and capture gender.

The exported Avatar name hashes and full-path hashes identify each source skin bone. The analyzer resolves its exact path beneath the selected Animator, retaining the captured instance ID. An extra source bone absent from the Avatar path map requires an unambiguous matching name under that same Animator. It does not match across the ragdoll branch or by a global basename search. Animator/Avatar/root identity and mapped bone identities/paths must stay stable throughout the selected interval; select separate intervals for character replacement. Renderer visibility changes, such as first-person body hiding, do not invalidate an otherwise identical bone mapping.

Gender/Avatar/bone agreement does not prove matching clothing or appearance. The generated default body is driven by observed poses; original runtime blend shapes, body customization, equipment meshes, shader parameters and combined-renderer mesh generation are not reconstructed by this step.

## Sampled PEAC recordings and materials

For each captured pose, the model-local bone transform is:

```text
inverse(captured modelRoot.world) × captured bone.world
```

Each selected source part retains its original mesh-local vertices, normals, UVs, skin weights/joint order and inverse-bind matrices. The exported palette is:

```text
inverse(captured modelRoot.world) × captured bone.world × source mesh inverseBind
```

The model-root transform is removed exactly once; source inverse binds are included exactly once. Input and output coordinates retain Unity XYZ. The end-of-frame pose includes whatever animation blending, procedural motion and IK ran in that original frame; it is not an isolated evaluation of the named clip. External actor placement remains the native renderer's responsibility.

Irregular observations are resampled at the requested FPS by componentwise affine-matrix interpolation. Time is recorded `realtimeSinceStartup`, so the recording preserves pauses and speed changes rather than silently interpreting normalized state time as clip seconds. The final guard frame clamps to the last captured pose. Clips are named `captured` and do not loop. Interpolated singular palettes are rejected. This sampling may differ from Unity quaternion interpolation and from the original physics/contact phase; frame metadata includes scaled/fixed time for further comparisons.

PEAC v1/v2 support one index set and one externally supplied diffuse image per actor. A default player has several parts and materials. The exporter therefore writes **one PEAC per part/material slot**, plus the corresponding diffuse PNG and `recording-manifest.json` with exact material properties, texture hashes and file mapping. A material without a diffuse image gets an explicitly recorded 1×1 solid PNG using its `_Color`, or white when no color is present; the current recovered default-player materials already have diffuse images. It does not place a random body texture over all submeshes or invent an atlas. Submeshes retain their original indices; a part's vertex/skin data can be duplicated between material files. `recording.peas` provides the bounded native scene handoff:

```text
PEAS1
player-part-0-material-0.peac player-male-default-texture-1234.png
```

New recorded exports use **PEAC v2**: interior palette samples remain at `frame/fps`, while the final palette is timestamped exactly at the serialized duration. This reaches a non-grid terminal capture pose without stopping short of the terminal pose. Existing PEAC v1 files retain their original uniformly spaced timestamps, including hare clips. Body and knife exports round FPS/duration to their serialized f32 values before determining frame count, avoiding duplicate terminal timestamps. Native scene loading rejects a mix of v1/v2 clock semantics even when the other clip fields agree.

The example texture name illustrates the format. Actual lines use generated safe ASCII basenames, with no whitespace, absolute paths or parent traversal; at most 64 material parts are allowed. Every actor has exactly one `captured` clip, the same FPS/frame count/duration and no looping, and shares the same model-local root space. The existing renderer can load each individual PEAC with its matching texture. A separate scene-viewer task consumes the PEAS manifest; this exporter does not itself open that viewer.

## Explicit hypothesis comparison

`--hypothesis FILE` compares user-supplied model-local bone matrices to selected observed frames. The schema is:

```json
{
  "schema": 1,
  "coordinate_space": "model-local",
  "frames": [
    {
      "sample": 120,
      "frame": 12345,
      "bone_matrices": {
        "Bip01/Bip01 Pelvis": [1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1]
      }
    }
  ]
}
```

This is a format illustration, not a measured pose. Paths are relative to the selected Animator as reported in `rig_mapping`; matrices are row-major, expressed in **model-root local space**. Both sample and Unity frame IDs must match, and every requested bone must resolve. Missing selected frames fail rather than producing misleading partial comparisons. The result reports comparison count, maximum absolute matrix-element error, translation RMS and maximum translation error in meters. No angular error is inferred from matrices that may contain scale/shear. A small residual establishes agreement only for that supplied hypothesis and those captured poses; it does not establish general retargeting, collision eligibility or attack damage.

## Tests and remaining runtime work

```sh
python3 rebuild/tests/test_reference_pose_analysis.py
```

Eleven synthetic tests pass. They cover consistent parent/TRS reconstruction, exact Avatar/bone mapping, clip spans, malformed matrices/IDs/clocks/gender/NaN, duplicate JSON keys, incomplete capture/export rejection, explicit hypothesis error and frame alignment, skin palette multiplication/order/timing, renderer visibility changes, byte-identical exports, PEAS handoff/explicit texture fallback and existing-report preservation on failure. When a C compiler is available, a generated synthetic PEAC is also loaded and skinned by the actual headless `pe_actor.c`, verifying a known midpoint vertex position. No graphics context is opened.

The real-capture results below confirm one player/Avatar hierarchy and measured clip-clock ratios. Pending: compare a complete mathematically specified humanoid pose hypothesis, recover repeatable locomotion, and establish source contact activation/damage. The root investigator subsequently verified the eleven-part starter-clothing/knife composition in a 38-frame graphics check and the idle clothed player in a 1500-frame training check; see the [recorded playtest evidence](../PLAYTEST.md#evaluated-player-recording-and-recovered-outfit). Original successful knife contact/kill remains unconfirmed. A captured animation recording can provide a useful reference fixture without implying that the complete Unity humanoid solver has been replicated.

## Completed original-runtime capture: knife-reference-01

The root investigator recorded and closed `~/.local/share/planet-explorers-pose-reference/pose-captures/knife-reference-01.jsonl`; the end record is `stop-file`. SHA-256 is `dc94b9c0ae1c2dffc99bffba2681c68b6ecf011c147bb8c3757d2de4bd3ba40e`. The companion `.actions.json` records intended operator input; actual captured Animator state determines the analysis, rather than assuming those input intentions succeeded.

The 566 consecutive Unity frames (16754–17319) span 45.60962 real seconds. Strict TRS/world/parent validation passes with maximum matrix-element discrepancy **4.3114e-6**. The selected actor is Female/FemaleAvatar at `EntityRoot/Tutorial_20000/FemaleModel(Clone)/Model`; every selected female source skin bone resolves. FirstPerson is 0 initially, becomes 1 at sample 298 (24.18640 s), and returns to 0 at sample 544 (43.89148 s). Third-person Attack6–9 and all four `1stP SwordAttack` clips are present, confirming that the recorded inputs triggered attacks and reached first person.

The actual animated model root ends at `(23.5133419,1.48621225,18.1559753)` with Unity yaw approximately +30°. The `MainPlayerCmpt` owner transform remains at `(27,2,11)` with yaw zero; it must not be used as the live model position in this capture. Model and Animator roots coincide here.

Capture cadence is approximately 12.3877 Hz: median interval 78.12 ms, range 19.65–413.69 ms. There are no missing Unity frame IDs, but this does not provide dense authored motion samples between observed frames. Resampling at 30 Hz interpolates those observations; it cannot recover unseen contact geometry or improve their temporal evidence.

The report `rebuild/build/pose-analysis/knife-reference-01-starter.json` records complete mapping/validation results. The initial default-body recording passed a 180-frame native multipart preview with nine material actors; the root investigator observed coherent textured body, face and hair. Its base underwear appearance differed from the original starter clothing, prompting the source-backed preset below. That preview establishes recording/rendering feasibility, not complete character appearance or combat parity.

## Source starter clothing and recorded scenes

`SingleGameInitData.AddTutorialInitData` calls `AddDefaultClothToPlayer`, whose item list is `{113,149,210,95,131,192}` and is filtered by item sex. Female starter equipment is **95/131/192**, mapping through `PrototypeItem._modelPath` to `Model/PlayerModel/female01-torso`, `female01-legs` and `female01-feet`. Male equivalents are **113/149/210**. Replace masks 2/64/128 correspond to torso/legs/feet in `AvatarData.GetSlot`; nude hands, default head and three default hair parts remain selected. `EquipmentCmpt.AddAvatarModel` and `AvatarCmpt` merge those equipped parts over the nude slots.

The existing player exporter now provides an explicit separate preset:

```sh
/tmp/pe-asset-tools/bin/python rebuild/tools/export_reference_player.py \
  --sex female --appearance starter
```

Its default destination is `rebuild/build/reference-player-starter`, leaving the earlier default-body export intact. The female rig is `player-female-starter-rig.json`. Selected clothing meshes are torso **7134**, legs **6078** and feet **7273**. The complete eight-part body has 4,001 source vertices and 5,560 triangles, exported as nine material actors because the head has two submeshes.

There is independent structural corroboration from the real capture: the combined renderer `PlayerAppearance1` has **67 bone entries**, and the source starter parts reproduce all 67 captured bone IDs in exact order. The ordering follows `AvatarData` slots HairF/HairT/HairB/Head/Torso/Hands/Legs/Feet; `CustomModelTmpls.cs` duplicates each part's bone array once per submesh. This explains the second five-bone head array. The nude-body selection does not match that captured array. A matching array supports the selected source parts; it is not a captured mesh/texture hash or proof of runtime body morphs and shader colors.

Three generated reference scenes use the matching starter rig:

| Directory under `rebuild/build/pose-analysis` | Inclusive captured samples | 30 Hz output | Meaning |
|---|---:|---|---|
| `female-starter-idle` | 0–20 | 57 frames, 1.86535 s | NormalIdle weight 1, no transitions; model position constant. Stowed-body reference, not invented locomotion. |
| `female-starter-tp-attack6` | 48–68 | 48 frames, 1.54517 s | First isolated TP attack with observed entry and exit blending. |
| `female-starter-fp-attack1` | 306–329 | 55 frames, 1.78235 s | First isolated first-person attack interval with surrounding state changes. |

Each directory contains `recording.peas`, nine matching material PEAC files, diffuse images and full JSON provenance. These are nonlooping **recordings**, not standalone authored attack clips. Model-root movement is removed from the skin palettes; the TP interval's model root moves approximately 0.529 m even though it is an isolated attack input. Replaying this recording in place does not reproduce that source displacement. The knife visual is a separate rigid recording prepared by the collision/weapon exporter and requires composition with the same sample interval/FPS/root space.

## Measured clock calibration and event estimates

`rebuild/tools/measure_reference_pose_clock.py` makes the clock analysis repeatable. It first runs strict capture/rig validation. For adjacent frames in the same state, with no transition, one current clip at weight at least 0.99, and unchanged Animator speed and FirstPerson, it computes:

```text
delta(normalizedTime) × runtimeClip.length / delta(Time.time)
```

The aggregate is duration-weighted. Animator `speed` is distinct from the `AnimSpeed` float parameter: the parameter remained 1 while Animator speed took values 1, 1.37 and 1.23299992.

| Runtime clip | Animator speed | Qualifying pairs | Clip seconds / scaled second |
|---|---:|---:|---:|
| Attack6 | 1.37 | 43 | 1.370054 |
| Attack7 | 1.37 | 8 | 1.369835 |
| Attack8 | 1.37 | 5 | 1.370245 |
| Attack9 | 1.37 | 4 | 1.369711 |
| Attack8 | 1.23299992 | 8 | 1.233075 |
| 1stP SwordAttack1 | 1.37 | 45 | 0.890521 |
| 1stP SwordAttack2 | 1.37 | 14 | 0.890396 |
| 1stP SwordAttack3 | 1.37 | 13 | 0.890443 |
| 1stP SwordAttack4 | 1.37 | 9 | 0.890446 |

These observations support a stationary TP branch multiplier of 1 and FP multiplier of **0.65**, consistent with the inverse of the recovered FP leaf duration factor 1.53846. They do not establish arbitrary partial-FP blending, moving attacks, hit response or all weapons. Full results, including slower FP examples, are in `rebuild/build/pose-analysis/knife-clock-rates.json`.

```sh
python3 rebuild/tools/measure_reference_pose_clock.py /path/to/knife-reference-01.jsonl \
  --rig rebuild/build/reference-player-starter/player-female-starter-rig.json \
  --output rebuild/build/pose-analysis/knife-clock-rates.json
python3 rebuild/tools/measure_reference_pose_clock.py /path/to/knife-reference-01.jsonl \
  --rig rebuild/build/reference-player-starter/player-female-starter-rig.json \
  --first-sample 48 --last-sample 68 \
  --source-clip rebuild/build/player-animation-clips/clip-231.json \
  --output rebuild/build/pose-analysis/female-starter-tp-attack6-clock.json
```

For the TP48–68 recording, scene time zero is sample 48 at real time 1331.91162 and scaled time 1267.7041. Sample 50, scene time 0.15283, contains next-state Attack6 at normalized time zero. Fitting the six stable current-state samples 51–56 gives clip clock rate **1.370042** and estimated normalized-zero scaled time **1267.864596**, with maximum clip-clock fit residual 0.0000583 s.

Mapping source clip 231's authored event times through this fitted clock and then piecewise between recorded scaled/real timestamps gives:

| Authored event | Source clip seconds | Estimated recording seconds | Observed sample bracket |
|---|---:|---:|---|
| TP_StartAttack_SingleHandSword | 0.068724 | 0.215386 | 50–51 |
| TP_WeightInputStart | 0.156946 | 0.279470 | 51–52 |
| TP_EndAttack | 0.299463 | 0.373674 | 52–53 |
| TP_WeightInputEnd | 0.451433 | 0.486151 | 54–55 |
| TP_EndAction | 0.709126 | 0.675733 | 56–57 |

These are **estimated authored-event alignments**, not captured event callbacks or measured collider activation. Their small clock-fit residual does not eliminate the much wider pose sampling brackets. The probe did not record attack-trigger active flags, collider enablement, accepted contacts or damage. Successful original knife contact and a kill remain unconfirmed.
