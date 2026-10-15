# Isolated original-Unity clip sampler

The installed Unity 5.2.4f1 player bakes source clips into native poses without attaching the game's controller or copying gameplay components. It recovers clips only. It does not implement a humanoid solver, reconstruct Animator transitions, or establish general animation parity. The [observed player recording](POSE_CAPTURE_ANALYSIS.md), by contrast, includes IK and state-machine effects.

## Tools and bounds

```sh
rebuild/build/unity-tools/bin/python rebuild/tools/inventory_reference_bake.py --output PLAN.json   # SHA-bound clip plan
python3 rebuild/tools/reference_animator_bake.py --plan PLAN.json [--preset NAME | --clip NAME ...]  # build only
python3 rebuild/tools/reference_animator_bake.py --launch --plan PLAN.json ... --output FRESH.jsonl # supervised
```

- **Plan.** The plan records the installed `sharedassets1.assets` SHA256, Unity version, Avatar and controller identity, source clip IDs, duration, rate, humanoid binding count, loop flag, events, and the controller leaf states it verifies.
- **Launch.** The launcher refuses another asset hash, clips without humanoid bindings, an existing `LD_PRELOAD`, and paths with whitespace. Outputs are fresh and inside the dedicated profile `~/.local/share/planet-explorers-bake-reference`.
- **Presets.** `default` (NormalIdle, NormalWF, NormalRF, Jump, Fall, Land, Attack6–9), `torch-sword`, `sword-backward`, `normal-directional`, `hit-reaction` and `gather` (female only).
- **Bounds.** 32 clips per selection, 512 transforms, 3,600 samples, 256 MiB, 4 MiB per line, 180 s of baking (maximum 600), 900 s waiting for arm (maximum 1,800).
- **Allowlists.** The Python `CLIPS` catalog and the managed `Allowed` list must be identical. A test enforces this, because run 07 failed when only Python grew.
- **Excluded.** NormalSprint is never selected, because two distinct clips share its name and duration.

## Isolation contract

`UnityEngine.Experimental.Director` is used on a fresh clone:

```csharp
isolated.avatar = original.avatar;
isolated.runtimeAnimatorController = null;
isolated.fireEvents = false;
isolated.applyRootMotion = false;
isolated.cullingMode = AnimatorCullingMode.AlwaysAnimate;
isolated.Rebind();
var playable = new AnimationClipPlayable(sourceClip);
isolated.Play(playable);
playable.state = PlayState.Paused; playable.speed = 0; playable.time = authoredSeconds;
```

- **Seek and read.** Each seek waits one full player-loop frame before reading at end of frame. An immediate `Update(0)` flush is not assumed.
- **Clones.** Each clip gets a fresh hierarchy of empty GameObjects built from one snapshot of names, parents and local TRS, with an identity root and exactly one Animator. No renderer, collider, rigidbody, gameplay MonoBehaviour, controller or state behaviour is copied. Every frame re-verifies that only Transform and Animator are present.
- **Controller left alone.** The original Animator is never written. Its controller would bring 140 state-machine behaviours (AnimatorParamCtrl, AnimatorEventSender, PEAnimatorAudio_Human, AnimatorLayerWeightCtr), and disabling events does not disable them.
- **Clip selection.** The sampler picks from the live player's controller clips the unique one matching name, length (±0.0001 s) and rate (±0.001 Hz). Wrong sex, Avatar or controller fails, as do ambiguous or legacy clips.
- **Getters.** `Motion.isHumanMotion` and `isLooping` throw `MissingMethodException` at runtime even though they compile (run 01), so humanoid proof and loop flags come from the plan.
- **Timing.** Sample times are taken in double precision, and the last sample is forced to the runtime clip length.

## Lifetime

Disposing a graph caused native aborts (runs 02 and 03, fault address `0xbc2a77`). Stop-before-dispose did not help. Used graphs are therefore disabled and retained until process exit, at most one per clip, with Playable finalizers suppressed. After output closes, a 120-update watchdog logs `Post-cleanup alive120frames`; that and the process exit are separate evidence from the JSONL end record.

Window close or Quit normally returns −9 (signal 9). Both source [PEMutex.OnDestroy](../../Assets/Scripts/ZhouXun/AC/PEMutex.cs) and the installed IL call `Process.GetCurrentProcess().Kill()` outside the editor, after `GlobalBehaviour.OnApplicationQuit` cleanup (IL dump in the ignored `rebuild/build/animator-bake/original-quit-handler-il.txt`). The analyzer always reports `process_teardown_validated: false`.

## Output and analysis

`pe-animator-bake-1` JSONL holds:

- a header with limits and flags;
- a skeleton record: clone↔source paths, parents, initial matrices, Avatar and the human-bone map;
- clip records: source and runtime IDs, length, rate, loop, sample count, events and clone IDs;
- poses: requested authored time, frame, local, world and root matrices, Animator root, and deltas;
- an end record that distinguishes complete, stopped, bounded, error and shutdown.

Root-motion fields are diagnostic only. Paused seeks at zero delta do not give cumulative motion; see [RUNNING_KNIFE_ROOT_MOTION](RUNNING_KNIFE_ROOT_MOTION.md).

```sh
python3 rebuild/tools/analyze_reference_bake.py CAPTURE.jsonl \
  --rig rebuild/build/reference-player-starter/player-female-starter-rig.json \
  --report REPORT.json --export-dir EXPORT_DIR
```

The analyzer checks:

- exact frame and clip counts;
- finite, invertible matrices with consistent composition;
- an identity root and correct Avatar mapping;
- distinct clone IDs, and real motion (frozen clips are reported as frozen, not fixed).

It maps source skin bones by exact Avatar path and exports one scene per clip, plus a combined `all/recording.peas` in which the PEAC v2 parts share mesh, skin and material. Clip names, rates, durations and loop flags stay explicit.

## Runs

| Run | Clips, poses | Result |
|---|---|---|
| 02 | Default 10, 345 | Complete. 42/42 bones, hierarchy error 1.14144e−6, SHA `1d319e64…6ef46`. The process aborted later, during disposal. |
| 03 | Idle, 62 | Same abort. Matrices identical to run 02. |
| 04 | Idle + RF, 84 | First retained-graph run. Survived the watchdog for over 5 minutes. Running error vs 02 was 6.68e−8 m. |
| 05 | Cancelled, 9 | Stop during an active graph; survived; exit −9. |
| 06 | Default 10, 345 | Complete with ten retained clones, SHA `118ba317…bae308bff`. Identical to 02 for idle and attacks, within 1e−7 m otherwise. Export at `player-female-06`. |
| 07 | Felling | Failed in configuration (`Invalid clip plan row`): the managed allowlist was missing the clips. |
| 08 | Felling1–3, 75 | SHA `c552e654…c30c3e`, error 1.01702e−6. Body at `player-female-cut`. |
| 09 | Spade1–3, 91 | SHA `f73df8c2…6d1bdf`, error 1.03176e−6. Body and shovel held and stowed at `player-female-dig`. |
| 10 | Mine1–3, 91 | SHA `a0064634…bc309`, error ≤ 1.049657993e−6. Installed as `player-pickaxe`, `player-pickaxe-stowed`, `player-mine` and `player-pickaxe-mine` ([pickaxe](SANDBOX_PICKAXE.md)). |
| female-gather-01 | NormalIdle + Herb leaves, 126 | Complete ([Gather](GATHER_LIVE_CAPTURE.md#lane-a-leaf-pose-bake-female-gather-01)). |

Sword, directional, reaction and running runs are recorded in [STARTER_SWORD_POSE_GROUP](STARTER_SWORD_POSE_GROUP.md), [DIRECTIONAL_PLAYER](DIRECTIONAL_PLAYER.md), [REACTION_PRESENTATION](REACTION_PRESENTATION.md) and [RUNNING_PLAYER_POSES](RUNNING_PLAYER_POSES.md).

## Tool clip catalog

All clips sample at 30 Hz. The sampler uses authored time and does not apply controller state speed.

| Clip | Female ID, length | Male ID, length | Leaf index, state speed | Events (female) |
|---|---|---|---|---|
| Felling1 | 244, 0.600000381 s | 249, 0.600002289 s | 399, 1 | — |
| Felling2 (loop) | 252, 1.100006104 s | 256, 1.000003815 s | 400, 1 | `Fell` 0.612334669 s |
| Felling3 | 259, 0.600000381 s | 264, 0.600002289 s | 401, 1 | — |
| Spade1 | 189, 0.699996948 s | same | 397, **2** | — |
| Spade2 (loop) | 201, 1.333335876 s | 208, 1.600000381 s | 394, 1 | `ChangeTarget` 0.048801865, `DigTerrain` 0.603514791 (male 0.128666133 / 0.648101509) |
| Spade3 | 211, 0.833335876 s | same | 398, 1 | — |
| Mine1–3 | 221, 228 (loop), 237 | loop 234 | entry speed 1.5 | Mine2 `ChangeTarget` 0.164528400, `DigTerrain` 0.838300884 |

Felling transitions:

- The entry clip, and Felling2 when `Fell` becomes false, exit at about 0.9 with a fixed 0.1 s transition.
- Felling3 exits at about 0.531251, with a normalized duration of 0.468749.

These event times are clip metadata. They are not the tutorial's scripted timers ([TRAINING_GATHERING](TRAINING_GATHERING.md), [TRAINING_MINING](TRAINING_MINING.md)).

## Observed Attack6 comparison

[`compare_reference_bake.py`](../../rebuild/tools/compare_reference_bake.py) compares a bake with observed poses at matching clip time. It requires one current clip at weight ≥ 0.99, the requested camera endpoint and no transition, and it removes each model root separately. Against `knife-reference-01` samples 51–56:

- Sample 51, the first frame after entry, differs by up to 1.006 m, cause unresolved.
- Samples 52–56 differ by at most 3.85, 4.76, 4.28, 5.47 and 8.83 mm.

Reports: `rebuild/build/animator-bake/attack6-*-comparison.json`. Stable body poses are recovered. Entry blending, root motion and IK are separate questions.
