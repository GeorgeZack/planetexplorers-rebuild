# Original-runtime pose capture

`reference_pose.py` captures live evaluated humanoid transforms from the installed native game. It is an observation tool, not a retargeter or proof of animation parity. For deliberate cloned-clip evaluation use the separate [Animator bake](../../docs/audit/ANIMATOR_BAKE.md).

## Prepare and run

```sh
python3 rebuild/tools/reference_pose.py
python3 rebuild/tests/test_reference_pose.py
```

Default execution builds offline only, in ignored `rebuild/build/pose-probe`. Explicit launch:

```sh
python3 rebuild/tools/reference_pose.py --launch \
  --user-data /home/admin/.local/share/planet-explorers-pose-reference \
  --output /home/admin/.local/share/planet-explorers-pose-reference/pose-captures/knife-01.jsonl \
  --frames 600 --seconds 20
```

Read [runtime/profile safety](../../docs/REFERENCE_RUNTIME.md) first. Profile paths cannot contain whitespace; output must be profile-scoped. Capture/arm/stop/manifest paths must be fresh. Preserve manual saves. Existing LD_PRELOAD is rejected rather than silently combined. The game still makes its own normal profile writes.

Navigate to the target state, then create the printed `.arm` path. Capture begins when player/model exist; `.stop` ends it. The probe never generates input, equipment/Animator changes, attacks or saves.

Default limit: first of600 frames,20 real seconds after start or256MiB; explicit maxima3600 frames/120s. Arm/player wait defaults900s, maximum1800s. Deadlines run on Unity frames and resume after suspension. Forced termination may leave a valid prefix without an end record; cleanup on normal destruction is best effort.

## What the tools do

| Tool | Boundary |
|---|---|
| `reference_pose_compile.c` | Separate embedded-Mono compiler process using installed compiler/references; no game executable/assembly loading |
| `reference_pose_shim.c` | Child-only x86_64 LD_PRELOAD; exact executable/Mono paths, one install on main OS thread at `Pathea.PeSingletonRunner.Update`. No worker-thread fallback or code patching |
| `ReferencePoseProbe.cs` | Hidden transient component reads existing `MainPlayerCmpt.gMainPlayer`, avoiding lazy creation; samples WaitForEndOfFrame and destroys itself |
| `reference_pose.py` | Offline build or explicit isolated-helper launch, child environment and launch manifest |

Loading/allocations/file IO affect runtime overhead; observation-only means gameplay behavior, not zero process effects. No original assembly rewrite, persistent plugin, ptrace/sysctl change or Steam bypass. Stopping the child removes preload; generated local artifacts remain.

## Capture format and interpretation

Schema1 JSONL: header, humanTraits, frames, clean end reason. Round-trip floats, null nonfinite values, row-major Unity-coordinate matrices.

Frames include clocks/deltas, gender, root/model/full hierarchy identities and local/world transforms; Animator/Avatar/controller state, root inverse and per-layer current/next state/transition/clips/weights; float/int/bool parameters; bone/skin-palette mapping. Triggers remain unknown/null. humanTraits records Unity5.2 muscle names/default limits/bone indices; absent HumanPoseHandler means no body/muscle samples from that newer API.

Full hierarchy includes inactive/ragdoll duplicates: select the active Animator/skin palette rather than bone names alone. End-of-frame is displayed pose after animation/IK, not unmodified muscles or a collision phase. Root-relative pose uses Animator-root inverse × bone-world matrix. Keep sex/view separate; use normalized clock/weights rather than wall-time speed assumptions.

## Validation

Fake-process/Mono tests cover callback/thread/path guards, once-only install and fresh profile paths; real compile uses installed assemblies/C11 warning errors. Runtime installation and external component acceptance were observed. Analysis must still validate matrices/palettes before export; headless tests do not prove pose accuracy.

## First original-runtime recording

October3 Tutorial mission746, female starter Knife, normal input: isolated clicks, hold, rapid clicks and F5. Local profile capture `knife-reference-01.jsonl`:566 frames/45.60962s, stop-file end; `.launch.json` and `.actions.json` retain limits/input times. FemaleModel/Model/FemaleAvatar and Attack6–9/firstpersonSwordAttack1–4 were observed. Camera labels require recorded parameter checks. No original enemy hit/kill was established; low sampling and live transitions/IK limit reuse. See [analysis/export evidence](../../docs/audit/POSE_CAPTURE_ANALYSIS.md).
