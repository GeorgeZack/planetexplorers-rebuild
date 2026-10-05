# Isolated original-Unity authored-clip sampler

The installed Unity 5.2.4f1 player now supplies a complete isolated bake of ten source clips, without attaching the game's controller or copying gameplay components. All 345 poses pass structural validation and contain actual bone motion; later stable Attack6 frames also agree closely with an independent original-player recording. **Two sampling runs aborted after writing complete output during cleanup, including a stop-before-dispose attempt.** The current mitigation retains a bounded inactive graph/hierarchy per clip until process exit. A two-clip run completed 84 poses, survived the measured 120-update check and remained responsive for more than five minutes; an ordinary window-close exit subsequently completed without a matching coredump. A subsequent early cancellation also survived 120 updates. Its window-close exit returned signal 9, consistent with the shipped game's explicit self-kill handler; a fresh full ten-clip run also completed and survived the measured 120-update check. Pose evidence and clean process teardown are separate requirements.

This is a recovery tool for baking source clips into native assets. It does not implement a humanoid solver in C, reconstruct Animator transitions, or establish general Unity animation parity. It follows the [observed player recording](POSE_CAPTURE_ANALYSIS.md); that observed recording includes procedural/IK and state-machine effects which the isolated sampler intentionally does not reproduce.

## Commands and limits

Prepare a source-bound clip plan using optional UnityPy, then compile against the legally installed original's assemblies:

```sh
/tmp/pe-asset-tools/bin/python rebuild/tools/inventory_reference_bake.py
python3 rebuild/tools/reference_animator_bake.py
python3 rebuild/tests/test_reference_animator_bake.py
```

The second command builds only. Thirteen headless tests verify exact source identity, timing/selection bounds, malformed plans, isolated profile/environment construction, build-only behavior, rejection of an existing preload, and preservation of native exit/signal status. The actual new C# sampler compiles with the installed Mono compiler, mscorlib, System.Core and UnityEngine assemblies. Existing main-thread shim tests apply unchanged; no probe or original game files are replaced.

An explicit supervised launch is separate:

```sh
python3 rebuild/tools/reference_animator_bake.py --launch \
  --output "$HOME/.local/share/planet-explorers-bake-reference/pose-captures/female-clips-01.jsonl"
```

Enter the tutorial with a female player and create the printed `.arm` file. The first camera mission already has the required live Animator; obtaining the knife is unnecessary. Creating the matching `.stop` file terminates sampling. The tool generates no input. Root investigators own launches and desktop testing.

Default profile: `~/.local/share/planet-explorers-bake-reference`, with isolated engine configuration in its `engine-config` directory. Its settings use the established no-MSAA startup preparation. Outputs must be fresh and inside that profile. The launcher refuses paths with whitespace because of the original command-line parser. Original files, the user's Steam profile and shared engine preferences remain untouched.

Default selection is ten clips at 30 samples per authored second, currently 345 total samples: NormalIdle, NormalWF, NormalRF, Jump, Fall, Land and Attack6–9. Use repeated `--clip NAME` to select a subset; `--sex male` selects the source male override. Bounds are 10 clips, 512 transforms, 3,600 total samples, 256 MiB output, 4 MiB per line, 180 real seconds of baking by default (maximum 600) and 900 seconds waiting for arm/player (maximum 1,800). Sampling retains the exact final authored time rather than extending the clip to a whole sample interval.

The plan records installed `sharedassets1.assets` SHA-256, Unity version, Avatar/controller identities, source clip object IDs, duration, rate, humanoid binding count, loop metadata and events. Launcher SHA verification refuses another asset version or a selected source clip without humanoid bindings. At runtime the sampler reads only the matching live player's controller clip array, deduplicates repeated references, and selects a unique clip with the expected name, length (tolerance 0.0001 s) and source sample rate (0.001 Hz). It rejects wrong sex/Avatar/controller, ambiguous clip identities, legacy clips and missing clips. NormalSprint is deliberately not selected: the controller contains distinct clips sharing that name and duration.

## Exact installed API and isolation

The compile-checked path uses `UnityEngine.Experimental.Director`:

```csharp
isolated.avatar = original.avatar;
isolated.runtimeAnimatorController = null;
isolated.fireEvents = false;
isolated.applyRootMotion = false;
isolated.cullingMode = AnimatorCullingMode.AlwaysAnimate;
isolated.Rebind();
var playable = new AnimationClipPlayable(sourceClip);
isolated.Play(playable);
playable.state = PlayState.Paused;
playable.speed = 0;
playable.time = authoredSeconds;
isolated.Update(0f);
```

Unity's [5.2 Playables manual](https://docs.unity3d.com/520/Documentation/Manual/Playables.html) documents clip-only Animator playback and manual time on a paused playable. These signatures also compile against this installation. The sampler waits a full player-loop frame after each seek before reading the pose at end of frame, allowing the old experimental graph to evaluate; immediate `Update(0)` flushing is not assumed. Unity describes [`Animator.Update`](https://docs.unity3d.com/520/Documentation/ScriptReference/Animator.Update.html) as explicit evaluation with a delta and cautions that this differs from ordinary game-loop/physics evaluation.

The original Animator/controller is never written. The sampler snapshots names, parent indices and initial local TRS beneath the selected original Animator, then makes a fresh hierarchy of empty GameObjects for each clip, places its root at identity, and adds exactly one Animator. It copies no Renderer, Collider, Rigidbody, gameplay MonoBehaviour, Animator controller or state-machine behavior. Every sampled frame verifies that only Transform and that Animator remain on the clone, that the controller stays null, and that event/root-motion flags stay disabled. The hierarchy is hidden, not rendered, and not parented to the live player. Original Avatar and AnimationClip assets are borrowed read-only; source gameplay Transform references are not retained after the snapshot. Each fresh clone uses the same initial snapshot and calls Rebind before its graph is attached. Whether all unanimated extra bones match the intended source default pose must be assessed from the output.

Using the original controller would be a materially different operation: installed FemaleAnimator2 contains 140 unique state-machine behavior objects, represented by AnimatorParamCtrl, AnimatorEventSender, PEAnimatorAudio_Human and AnimatorLayerWeightCtr. Attack6 has a human-audio behavior. Disabling AnimationEvents does not itself disable those callbacks. The clip-only graph never attaches that controller, avoiding this dependency rather than assuming callbacks are harmless.

Clip selection uses these verified female source IDs (male mapping comes from the serialized override): NormalIdle 684, NormalWF 531, NormalRF 539, Jump 263, Fall 500, Land 505, Attack6 231, Attack7 240, Attack8 247 and Attack9 254. Locomotion leaves occur beneath `Base Layer.GroundLocomotion.Move`; Jump/Fall/Land use the corresponding Jump substates. Source controller blend parameters and transitions remain separate work.

## Output and validation contract

`pe-animator-bake-1` JSONL is deliberately distinct from the observational capture format. A header records limits, coordinates and disabled event/controller/root-motion flags. A skeleton record maps each source transform instance ID and exact relative path to its clone node, parent index and initial local matrix, plus original model/Animator path, Avatar identity and human-bone mapping. Each clip record contains its source object ID, runtime instance ID, source length/rate, loop flag, sample count and event metadata. No event is emitted to game logic.

Each pose records clip/sample indices, requested authored time, actual Unity frame/realtime, clone local/world matrices, clone root matrix, Animator rootPosition/rootRotation and deltaPosition/deltaRotation. The final record distinguishes complete sampling from cancellation, time/byte bounds, errors or shutdown. Used graphs are disabled/deactivated and retained, not disposed/destroyed during the run; at most ten graphs and 5,120 empty transforms can remain. Their managed Playable finalizers are suppressed because that finalizer releases a native graph; its existence and Dispose behavior were verified in the installed assembly. Static strong references preserve the bounded objects until process exit. This intentional lifetime retention is a containment measure for the observed native cleanup crashes, not proof of their exact cause. The launcher records source/build hashes alongside the capture. New launches also write a separate `.exit.json` after the native child returns, preserving its raw return code (including negative signal codes), signal number and UTC observation time. The launch manifest is not rewritten; interrupted launchers may not produce an exit record. Each clip supplies its own clone instance-ID array, while the skeleton's IDs describe the first clone. The analyzer requires distinct IDs across retained clips.

After closing the output, the observation host counts 120 distinct Unity frame updates and logs `Post-cleanup alive120frames`, without appending to the completed JSONL. This marker and a normal subsequent exit are external lifetime checks; a successful end record alone cannot establish them.

Strict analysis checks exact frame/clip counts, finite invertible matrices, consistent parent/local/world composition, stable identity root, correct Avatar/path mapping, nontrivial bone motion and repeatability at identical authored times. Compare a stable Attack6 authored pose with the observed recording only after matching clip clock; differences may include procedural IK, entry/exit blending and root handling. No fake matrices or generated motion stand in for a failed bake.

Root-motion fields are diagnostic. With paused seeks, zero-delta evaluation and `applyRootMotion=false`, delta values may be zero or discontinuous and do not establish cumulative authored displacement. The observed attack recording moved its model root approximately 0.529 m. A sampled body pose held in model space does not reproduce that whole attack trajectory; a separate validated root-motion pass is needed.

## Offline analysis and native export

Only after a successful `complete` end record is written:

```sh
python3 rebuild/tools/analyze_reference_bake.py /path/to/female-clips-01.jsonl \
  --rig rebuild/build/reference-player-starter/player-female-starter-rig.json \
  --report rebuild/build/animator-bake/report.json \
  --export-dir rebuild/build/animator-bake/player-female
python3 rebuild/tests/test_reference_bake_analysis.py
```

Nine synthetic tests verify hierarchy and Avatar mapping, authored timing including fractional terminal samples and legacy arithmetic, distinct retained-clone identities, rejection of incomplete/malformed data, explicit detection of frozen poses, observed-pose comparison, native skin palette output and combined multi-clip geometry sharing. When a C compiler is available, the actual headless native scene parser also loads the two-clip synthetic result. Tests do not stand in for a real completed bake.

The analyzer validates the entire closed stream, then maps source skin bones by exact Avatar relative paths. It reports per-clip maximum bone matrix/position change, changing-bone count, hierarchy error and missing mappings. Frozen samples remain visibly reported as frozen; no procedural substitute is generated. Optional export writes one separate scene per source clip and a combined `all/recording.peas`. The combined PEAC v2 files share mesh/skin/material data once and append the same ordered clip list for every material actor. Source clip names, authored FPS/duration and loop flags remain explicit; the last frame occurs at the exact serialized duration. `recording-manifest.json` retains source hashes and all sampling limitations. Texture/material handling matches the earlier multipart recording exporter.

## Runtime attempt log

The first supervised attempt, `female-clips-01.jsonl`, entered the female tutorial and ended cleanly with an error and zero samples **before creating the clone**. The native player threw `MissingMethodException` for `UnityEngine.Motion.get_isHumanMotion`, although that property compiled against the installed managed assembly and its registration name is present in the native executable. This demonstrates why API metadata/compilation alone is insufficient runtime evidence.

The sampler now omits `Motion.isHumanMotion` and the related inherited `isLooping` metadata getter. Humanoid binding proof and loop flags come from the exact installed asset's SHA-bound inventory instead. AnimationClip length, frameRate and legacy getters were reached successfully before the failure.

The second supervised run, `female-clips-02.jsonl`, wrote a complete end record: **345 samples, ten clips, 78 pure clone transforms**, approximately 7.9 MiB. SHA-256: `1d319e64e32745ee277031864b89fbd751ecf2ba1c9af06293b2727dee36ef46`. Samples span 25.7491 real seconds while their animation timeline is requested authored time. FemaleAvatar/FemaleAnimator2 match the source plan. All 42 selected skin bones resolve; maximum parent/world matrix error is **1.14144e-6** and every clone root matrix is identity. Every clip changes all 42 mapped bones. Maximum displacement from each clip's first pose is approximately 0.0101 m for idle, 0.684 m for walking, 0.982 m for running and 1.159 m for Attack6. These are bone trajectories, not player root travel.

The runtime produced the extended-product sample count near float grid boundaries (NormalRF and Attack6), including terminal samples. Older captures without a policy marker are validated against the two exact possible float-product rounding paths, with the observed path reported; arbitrary sample counts remain rejected. The corrected sampler explicitly multiplies in double precision and forces the last sample to the runtime clip length. New headers identify that policy, eliminating ambiguity.

`rebuild/build/animator-bake/player-female/all/recording.peas` contains all ten ordered source clips for each of nine body/material parts. Per-clip directories remain available. Native graphical validation of these newly baked clips is tracked separately by the root investigator. Animator rootPosition remains zero; recorded deltaPosition is sometimes nonzero during attacks (maximum vector length 0.52855 m). Paused seeking and zero-delta evaluation make those deltas diagnostic, not a validated cumulative motion track.

At 18:24:55 CDT the second run's original process, PID 1129350, faulted on the main thread and ultimately aborted. Root core analysis found Unity/Mono frames, no OOM evidence and no symbolized native function names sufficient to prove the mechanism. Review found a concrete lifecycle problem: `Playable.Dispose` releases its native graph but does not detach the Animator/DirectorPlayer output; the previous final path disposed the graph before deferred clone destruction. A dangling player graph is a supported hypothesis, not a symbolized crash diagnosis. The first attempted correction called inherited `DirectorPlayer.Stop`, disables the isolated Animator, disposes the graph, deactivates the clone and schedules its destruction. The same order applied between clips. The actual installed API compiles, and [Unity 5.2 documents Stop as stopping player/playable playback](https://docs.unity3d.com/520/Documentation/ScriptReference/Experimental.Director.DirectorPlayer.Stop.html). The subsequent minimal run tested this correction and disproved its safety, as described next.

That stop-before-dispose attempt also failed: run 03 completed 62 idle samples, remained alive briefly, then PID 1149086 aborted at 18:31:46 CDT at the same unsymbolized fault address `0xbc2a77`. All 62 × 42 idle bone matrices are exactly identical between captures 02 and 03, supporting pose repeatability while disproving the proposed lifetime fix. Static disassembly shows an indirect virtual call through an object's field at offset `0x398` inside a cleanup-like function; it does not identify the owning native type. The stop/dispose strategy is therefore superseded by the bounded retention mode described above. Run 04 exercised fresh per-clip clones and continued updates, as described next.

Run 04, `female-clips-04.jsonl`, completed **84 samples across NormalIdle (62) and NormalRF (22)** using separate retained clones. SHA-256: `b2c2e1ab6601f5f4ffb4ec47f577f251fe581d52b77b2f5514bd1ebc7fc3dd6e`. Strict analysis validates distinct clone IDs, all 42 mapped skin bones, identity roots and a maximum hierarchy error of **9.88935e-7**. Comparing identical authored times with run 02 gives exactly identical idle matrices and maximum running bone-position error **6.68e-8 m** (maximum matrix-element error 6.6e-8). Reports are `rebuild/build/animator-bake/female-clips-04-report.json` and `repeatability-02-04.json`; the existing run 02 exports were preserved.

The root investigator observed `Post-cleanup alive120frames` in the original's log, a responsive tutorial acknowledgement click and continued survival beyond five minutes. This provides positive evidence for bounded retention during continued play, unlike runs 02/03. The root investigator then sent WM_DELETE_WINDOW to the verified original window. The native process and launcher exited, and the preserved log records database disconnects and stopped terrain worker threads. `coredumpctl info 1158531` found no matching coredump. The exit code was not retained, so this is not a claim of return code zero. Log: `rebuild/build/animator-bake/female-clips-04-Player.log`, SHA-256 `da056e248e7787cd1b544f1578229af02c4be36ef2d41237b19f7accd81f7ad5`. Run 04's unknown exit status must remain unknown: the game's explicit self-kill quit path, verified during the next run, means normal window-close behavior should not be equated with a zero return code. The later full ten-clip result is recorded below. The analyzer deliberately continues to report `process_teardown_validated: false`: external lifecycle evidence cannot be inferred from the JSONL end record.

Run 05 exercised cancellation while a graph was active. The bounded fixture observed eight poses and created the stop file; the sampler wrote **nine poses** before a closed `stop-file` end record, reflecting normal file-observation timing. Capture SHA-256: `77ccbd11041f75da8d20231f20f327926a472da8c6f9c5fb44eff0d6cce9c9ca`. The log contains `Post-cleanup alive120frames`, and the root investigator confirmed the process remained alive until an explicit WM_DELETE_WINDOW request. This partial bake is deliberately rejected by the strict complete-bake analyzer and was not exported as an authored clip.

The new `female-clips-05.exit.json` preserves **returncode −9, signal 9**, observed at `2026-10-03T23:49:20.548385+00:00`. The log records stopped terrain workers and database disconnects before termination. This does **not** establish return code zero or a signal-free shutdown. The source [PEMutex.OnDestroy](../../Assets/Scripts/ZhouXun/AC/PEMutex.cs) explicitly calls `System.Diagnostics.Process.GetCurrentProcess().Kill()` outside the editor, after releasing its mutex. Independent metadata/IL inspection of the installed `Assembly-CSharp.dll` confirms that shipped method contains the same `Application.isEditor` guard and `GetCurrentProcess`/`Kill` calls. The installed `GlobalBehaviour.OnApplicationQuit` likewise calls terrain cleanup, database release and settings save, explaining why orderly application cleanup can precede forced termination. This is a source-and-installed-code-supported explanation for the observed signal, not a captured syscall attribution or evidence of an external kill. Root reported no OOM evidence for the interval.

The preserved log is `~/.local/share/planet-explorers-bake-reference/pose-captures/female-clips-05.player.log`, SHA-256 `e8d2f88303f70dc35ec3ea4e0965fb6d56314689b2c0da3a67b75f4219abe1e7`; the installed method inspection and assembly hash are in `rebuild/build/animator-bake/original-quit-handler-il.txt`. No original methods were invoked by that offline inspection. No sampler/native changes were made for this diagnosis. Run 05 supports continued-play stability after cancellation; the previously observed sampling-cleanup aborts remain distinct historical failures.

Run 06 completed the full **345-sample, ten-clip** selection with ten distinct retained clones. SHA-256: `118ba317b73395e25557e86abeed4c3c65ea5a673ac78edadb638f4eae308bff`. Strict analysis again resolves all 42 skin bones and validates identity roots, all per-clip clone identities and a maximum hierarchy error of **1.14144e-6**. Against run 02 at matching authored times, idle and all four attack clips have exactly identical bone matrices; the other clips differ by at most **1e-7 m** in bone position. This confirms that the per-clip retention strategy preserves recovered poses as well as avoiding the previously observed runtime cleanup abort during the measured check.

Root observed `Post-cleanup alive120frames` and native PID 1195048 still alive more than five minutes after launch. Root subsequently closed the original window; the separate exit record reports −9/signal 9 at `2026-10-03T23:58:34.619705+00:00`, consistent with the shipped self-kill handler described above. This is not a zero-status claim. The preserved original log is `female-clips-06.player.log` alongside the capture. Reports are `rebuild/build/animator-bake/female-clips-06-report.json` and `repeatability-02-06.json`. A fresh independent export is available at `rebuild/build/animator-bake/player-female-06/all/recording.peas`; active run 02 scene files remain unchanged. Full retention survival, two-clip survival and active-graph cancellation now each have supervised runtime evidence. This remains intentionally bounded resource retention through process termination, not a verified reusable graph-disposal implementation.

## Comparison with observed Attack6

`compare_reference_bake.py` compares the isolated bake with independently observed poses, using each observed state's normalized time multiplied by its runtime clip length. It requires a single matching current clip, layer/clip weights at least 0.99, the requested FirstPerson endpoint and no reported transition. Each model root is removed independently. All errors remain reported; the tool does not silently discard an entry outlier.

```sh
python3 rebuild/tools/compare_reference_bake.py /path/to/female-clips-02.jsonl \
  /path/to/knife-reference-01.jsonl \
  --rig rebuild/build/reference-player-starter/player-female-starter-rig.json \
  --first-sample 48 --last-sample 68 \
  --report rebuild/build/animator-bake/attack6-observed-comparison.json
```

The six qualifying observed frames are samples 51–56. Sample 51, the first frame reporting current Attack6 after entry, differs by up to **1.006 m** in bone position despite reporting no transition. Its cause remains unresolved; it cannot be used as an isolated-clip equivalence point. Samples **52–56** compare all 42 bones with per-frame maximum position errors **3.85, 4.76, 4.28, 5.47 and 8.83 mm**. The full six-frame report and an explicitly selected later-five report are both retained. This is useful evidence that stable body poses are recovered correctly, with interpolation/procedural differences, while entry blending, root motion, other clips and general humanoid behavior remain separate validation work.

## Optional tree-cutting clip inventory

The inventory catalog additionally exposes three verified source clips, without changing the default ten-clip selection. A separate local plan can be prepared while retaining the original plan used by earlier captures:

```sh
/tmp/pe-asset-tools/bin/python rebuild/tools/inventory_reference_bake.py \
  --output rebuild/build/animator-bake/clip-plan-felling.json
```

After the preceding original process has exited, a separately supervised launch may select only these clips:

```sh
python3 rebuild/tools/reference_animator_bake.py --launch \
  --plan rebuild/build/animator-bake/clip-plan-felling.json \
  --clip Felling1 --clip Felling2 --clip Felling3 \
  --output "$HOME/.local/share/planet-explorers-bake-reference/pose-captures/female-clips-08.jsonl"
```

At this tree-cutting checkpoint the catalog contained 13 clips; a single selection still permits at most ten clips, 3,600 poses and ten retained graphs. Existing ten-entry plans remain accepted and default selection is unchanged. Twelve launcher/inventory tests verify these bounds, optional selection and direct controller-state mapping. The first attempt added the Python catalog but omitted the independent C# allowlist, causing run 07 to fail before writing a capture or creating a clone. The managed allowlist was then extended by those same three names; a cross-language consistency test now requires it to match the Python catalog exactly. MaxClips remains ten, and graph/sampling/lifetime code is unchanged. A running assembly is never rebuilt until that original process exits.

| Clip | Female source ID | Female duration | Male override ID | Male duration | Loop |
|---|---:|---:|---:|---:|---|
| Felling1 | 244 | 0.600000381 s | 249 | 0.600002289 s | No |
| Felling2 | 252 | 1.100006104 s | 256 | 1.000003815 s | Yes |
| Felling3 | 259 | 0.600000381 s | 264 | 0.600002289 s | No |

All have source sample rate 30 Hz. The female plan requests 75 poses; the male plan requests 72. These are inventory-derived counts; the female count was subsequently confirmed by run 08 below, while the male selection has not been sampled. Each identity is unique among distinct clip objects referenced by the respective installed controller/override using the sampler's name, duration and rate criteria. These clips are already controller references when the tutorial player Animator exists; cutting gameplay need not be activated merely to retrieve them. Runtime sampling remains subject to the same exact identity checks.

Installed controller 779 directly maps `FullBody.Fell.Felling1/2/3` leaf clip indices 399/400/401 to source objects 244/252/259, all at state speed 1. The entry clip transitions at normalized exit time approximately 0.9 with a fixed 0.1-second transition; Felling2 does likewise when `Fell` is false. Felling3 exits with normalized exit time approximately 0.531251 and normalized transition duration 0.468749. The plan retains these serialized state/transition records, but clip-only sampling does not apply those transitions.

Female Felling2 contains `AnimEvent("Fell")` at authored **0.612334669 s**. This must remain distinct from [HolotreeCutter](../../Assets/Scripts/Jaluca/TrainingField/HolotreeCutter.cs), which toggles the `Fell` boolean and uses independent tutorial timers: entry 0.6 s, repeating stage 1.1 s, hit 0.6 s, exit 0.533 s. The source exit clip lasts approximately 0.6 s, and the male repeating clip differs from the female one. Neither a clip's length nor its general `Action_Fell` event is a replacement for the tutorial's scripted HP/reward timing. See [TRAINING_GATHERING.md](TRAINING_GATHERING.md) for that separate behavior. Inventory alone establishes no cutting-animation playback; the subsequent run 08 sampling and headless native checks are recorded below.

Run 07 failed during managed probe configuration with `ArgumentException: Invalid clip plan row`. The complete failure log is preserved as `female-clips-07.install-failure.log` beside its launch manifest. No JSONL, clone, sampled pose or cut export was produced. This was an exporter integration error, not a Unity animation evaluation or cleanup failure; runtime validation requires a fresh run after the allowlist correction.

Run 08 completed **75 poses across Felling1/Felling2/Felling3**, using three distinct retained clones. Capture SHA-256: `c552e6542d06042de3161c117476c4ef70d93769294058b04a5c47173ff30c3e`. Strict analysis resolves all 42 skin bones; all 42 move in each clip. Maximum parent/world reconstruction error is **1.01702e-6**. The clips have 20/35/20 samples and maximum bone displacement from their initial poses of approximately 0.7561/0.9135/0.7563 m. The observed Felling2 event metadata matches source `AnimEvent("Fell")` at 0.612334669 authored seconds. These are sampled poses and event metadata, not dispatched cutting damage.

The fresh nine-part body scene is `rebuild/build/animator-bake/player-female-cut/all/recording.peas`; the report is `female-clips-08-report.json` in the same build area. The actual native scene loader and skinning code successfully loaded all nine parts and evaluated every authored frame of all three clips under AddressSanitizer/UndefinedBehaviorSanitizer. Root subsequently observed `Post-cleanup alive120frames` and a responsive welcome screen, preserved `female-clips-08.player.log`, then closed the original window. Its exit sidecar reports −9/signal 9 at `2026-10-04T00:08:53.629172+00:00`, consistent with the shipped self-kill handler. Graphical axe/body alignment and tutorial integration remain separate root-owned observations. No live player input or original process control was performed by the exporter.

## Optional shovel clip inventory

After run 08 exited, the verified catalog and matching managed allowlist were extended to include Spade1/2/3. At this shovel checkpoint the catalog had **16** clips; default selection remains the original ten and a single bake still selects at most ten. No graph lifetime, event isolation or sampling logic changed. The separate plan `rebuild/build/animator-bake/clip-plan-spade.json` was generated and the probe compiled successfully offline for **91 female poses**. Twelve headless tests include matching managed/Python allowlists, explicit shovel selection and preservation of state speed as metadata.

```sh
/tmp/pe-asset-tools/bin/python rebuild/tools/inventory_reference_bake.py \
  --output rebuild/build/animator-bake/clip-plan-spade.json
python3 rebuild/tools/reference_animator_bake.py --launch \
  --plan rebuild/build/animator-bake/clip-plan-spade.json \
  --clip Spade1 --clip Spade2 --clip Spade3 \
  --output "$HOME/.local/share/planet-explorers-bake-reference/pose-captures/female-clips-09.jsonl"
```

The installed controller directly references `FullBody.Dig.Spade1/2/3`: leaf indices **397/394/398**, female source IDs **189/201/211**. Male uses the same entry/exit clips and replaces the loop with **208**. Names are unique under runtime matching criteria. Female authored lengths are **0.699996948 / 1.333335876 / 0.833335876 seconds**; male's middle clip is **1.600000381 seconds**. All sample at 30 Hz and only the middle clip loops.

Controller state speed is **2** for Spade1 and **1** for Spade2/3. The sampler records authored clip time at unit speed and does not apply this controller speed. Female Spade2 events are `AnimEvent("ChangeTarget")` at **0.048801865 s** and `AnimEvent("DigTerrain")` at **0.603514791 s**. Male's corresponding events occur at **0.128666133 / 0.648101509 s**. These metadata values do not establish full controller timing, transitions, action eligibility or event dispatch. [TRAINING_MINING.md](TRAINING_MINING.md) describes that separate action contract. The subsequent run 09 supplies sampled poses, as recorded below; graphical shovel playback remains a separate check.

Run 09 completed **91 poses** across Spade1/2/3, with **22/42/27 samples** respectively. Capture SHA-256: `f73df8c26b13233dc2c388c7f92f159b6f250805bdd7d18c33f65cc28c6d1bdf`. All 42 mapped body bones move in every clip; maximum hierarchy reconstruction error is **1.03176e-6**. Maximum displacement of a bone from its initial pose is approximately 0.598/0.776/0.585 m. Runtime event metadata agrees with the source female ChangeTarget and DigTerrain times. Spade1 remains sampled in authored time; controller speed 2 is not folded into its duration.

The nine-part body export is `rebuild/build/animator-bake/player-female-dig/all/recording.peas`. Corresponding single-part held/stowed shovel scenes are `player-female/shovel-dig-all/recording.peas` and `player-female/shovel-stowed-dig-all/recording.peas` beneath the same animator-bake directory. Every clip clock matches the body exactly. Actual native loading and skinning evaluated every frame of all three scenes under AddressSanitizer/UndefinedBehaviorSanitizer. The source report is `female-clips-09-report.json`; [SHOVEL_ASSETS.md](SHOVEL_ASSETS.md) records source geometry and mounts. Root independently observed `Post-cleanup alive120frames`, preserved `female-clips-09.player.log` and captured the responsive welcome screen before ordinary window close. The exit sidecar reports −9/signal 9 at `2026-10-04T00:13:16.155722+00:00`, consistent with the shipped self-kill handler. Graphical or gameplay mining results are not inferred from these sampling/lifecycle checks.


## Mine pickaxe capture10

On October 5 the verified catalog and C# allowlist were extended with Mine1/2/3, bringing the catalog to 19 clips while preserving the original ten-clip default and maximum ten selected clips. Thirteen launcher/catalog tests pass, including managed/Python allowlist equality and Mine direct-state identity, authored durations, source entry speed1.5 and separate male loop234. This changed the selected source identities only; graph lifetime and sampling behavior remain the previously validated bounded-retention implementation.

The root-operated `female-mine-10.jsonl` completed91 poses across three isolated clones: Mine1(221)22frames, Mine2(228)42frames and Mine3(237)27frames at 30 Hz. Capture SHA256 is `a006463469ba00bf7768f1e5748d46437f74d9fb205848a09672ec93f62bc309`; installed sharedassets1 SHA256 remains `d9c65f820a2186116d9eb73d815fe07d9f46924ddfd103ee9510bf254f9a3d67`. The report `rebuild/build/animator-bake/female-mine-10-report.json` binds the capture, source rig and launch manifest hashes. It validates FemaleAvatar/FemaleAnimator2, all 42 skin bones with no missing mapping, and hierarchy reconstruction error at most 1.049657993e−6. All 42 mapped bones vary in every clip; their maximum positional changes are approximately .598/.948/.585m. Female Mine2 retains ChangeTarget at .164528400 s and DigTerrain at .838300884 s; these are source event metadata, not executed digging events.

Root observed the 120-update watchdog and a responsive original tutorial, preserved `female-mine-10.player.log`, then used QuitGame followed by Yes. The original exited and the sidecar records −9/signal 9 at `2026-10-05T13:18:30.436471+00:00`, consistent with the shipped self-kill quit handler documented above. No exit-code-zero or reusable graph-disposal claim follows. The original sampler DLL was not rebuilt during the run.

The nine-part body export `rebuild/build/animator-bake/player-female-mine/all/recording.peas` and exact one-part pickaxe attachment share the three PEAC v2 authored clocks, including terminal samples. Both passed native every-frame skinning under ASan/UBSan. The two accompanying held/stowed locomotion attachments reuse run02's345 poses and match the existing body clocks exactly. All four complete groups were installed together as `player-pickaxe`, `player-pickaxe-stowed`, `player-mine` and `player-pickaxe-mine` under `rebuild/build/reference-assets`. Seven pickaxe-export tests pass, including real-source hashes, exact timeline checks and byte-identical repeat output. [The pickaxe audit](SANDBOX_PICKAXE.md) records the source mesh/material/mount identity, exact regeneration commands, remaining transition/IK/root-motion limitations and separate native action integration. Graphical and physical C mining outcomes are pending separate root-owned validation at this checkpoint; no original-client pickaxe gameplay outcome was observed.
