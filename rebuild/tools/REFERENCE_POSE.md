# Original-runtime pose capture

This optional diagnostic captures evaluated humanoid transforms from the installed original Linux game. It supplies calibration evidence for the C rebuild's retargeting work; it does not implement retargeting or claim animation parity. The probe and compiler/shim build successfully against the installed Unity 5.2 assemblies. An isolated original-runtime test on 2026-10-03 confirmed installation on the Unity main thread and a clean 566-frame recording, with female knife attacks in both camera views. This is evaluated-pose evidence; it does not establish general humanoid retargeting parity.

## Prepare and run

```sh
# Offline build only; no game launch or profile settings changes.
python3 rebuild/tools/reference_pose.py
python3 rebuild/tests/test_reference_pose.py

# Explicit operator-controlled launch, with a fresh capture filename.
python3 rebuild/tools/reference_pose.py --launch \
  --user-data /home/admin/.local/share/planet-explorers-pose-reference \
  --output /home/admin/.local/share/planet-explorers-pose-reference/pose-captures/knife-01.jsonl \
  --frames 600 --seconds 20
```

An existing isolated reference profile can be supplied instead. The profile path must contain no whitespace because the original command-line parser cannot handle it. The output must be inside that profile. The helper refuses existing capture, arm, stop, or launch-manifest paths. Its default profile is separate from normal Steam and other reference profiles. No binaries, assemblies, Steam configuration, desktop configuration, or system settings are modified.

The helper prints the exact `.arm` and `.stop` paths. Navigate the original to the desired lesson and equipment, then create the printed `.arm` file. Capture starts once the main player and model exist. Perform the intended camera/attack actions normally. Creating `.stop` ends the probe. The probe never generates input, attacks, equipment changes, Animator parameters, animation playback, IK changes, or saves. The original game's ordinary profile writes remain governed by the game itself.

The default recording limit is the first of 600 sampled frames, 20 real-time seconds after capture begins, or 256 MiB. Defaults can be reduced or explicitly increased up to 3,600 frames and 120 seconds. Waiting for the arm/player is limited to 900 seconds by default (maximum 1,800). These deadlines are checked on Unity frames; if Unity stops processing frames while unfocused or suspended, teardown runs after frames resume. Closing the game invokes best-effort output cleanup. A forced process kill may leave a valid JSONL prefix without its final record.

## What the tools do

- `reference_pose_compile.c` embeds the installed Mono runtime in a **separate compiler process**, loads only its bundled compiler and the supplied compilation references, and calls `Mono.CSharp.CompilerCallableEntryPoint.InvokeCompiler`. No SDK/download is required. It does not load the game executable or game assembly.
- `reference_pose_shim.c` is an x86_64 Linux `LD_PRELOAD` library scoped to the launched executable. It intercepts the lookup of `mono_runtime_invoke` only when both executable and Mono-library canonical paths match. It installs once at `Pathea.PeSingletonRunner.Update` on the process's main OS thread. This source callback drives the component manager (`Assets/Scripts/Assist/PeSingletonRunner.cs`) and is therefore an appropriate Unity-main-thread fence. If Unity uses another invocation mechanism, this hook may never fire; the shim does not fall back to unsafe worker-thread installation or patch executable code.
- `ReferencePoseProbe.cs` adds one hidden transient observation component. It reads the existing `MainPlayerCmpt.gMainPlayer` field, avoiding lazy singleton creation. It samples through `WaitForEndOfFrame` and destroys its own object when finished. DLL loading, one observation object, allocations, and file IO necessarily affect process state/performance; “observation-only” describes its gameplay behavior, not zero runtime overhead.
- `reference_pose.py` builds these files into ignored `rebuild/build/pose-probe`. Default execution stops there. `--launch` uses the existing isolated-profile launcher settings, sets the child-only preload/probe environment, and writes a capture launch manifest. It refuses an existing `LD_PRELOAD` configuration to avoid silently combining unknown hooks. Ending the launched process removes the preload entirely; deleting generated build/capture files removes the remaining artifacts.

All Mono dependencies are resolved from the user's installed game. The tool does not circumvent access controls, DRM, Steam checks, or process-attachment restrictions. There is no `ptrace`, sysctl change, loader-thread Unity call, original assembly rewrite, or persistent plugin installation.

## Capture format and interpretation

JSONL schema 1 contains a header, a `humanTraits` record, zero or more frames, and an end reason when cleanly closed. Floats use round-trip formatting; nonfinite values serialize as `null`. Matrices are row-major arrays, retaining Unity coordinates without conversion.

Each frame records:

- Frame index, game time, real time, frame delta, unscaled delta, fixed time and fixed delta.
- Source player gender, player root and model root, full transform paths and instance/parent IDs, local position/quaternion/scale, local matrices and world matrices, and active-hierarchy flags.
- Every model Animator's identity, Avatar name/instance ID, controller name, enable state, speed, root motion setting, root matrix/inverse, and active hierarchy state.
- Per-layer current/next state hashes, normalized time, length, speed/multiplier, transition flag, layer weight, and current/next clip names/IDs/weights/lengths.
- Animator float/int/bool parameter values, including the original first-person parameter where present. Trigger parameters are represented as unknown/null rather than inferred or consumed.
- Runtime `HumanBodyBones` → transform-instance mapping; skin-renderer mesh names and palette transform IDs.

The `humanTraits` record preserves Unity 5.2's muscle names/default angular limits and bone-to-muscle indices. `HumanPoseHandler` is absent from this installed UnityEngine API, so body/muscle samples from that newer API are **not** claimed. The full hierarchy intentionally retains inactive and ragdoll transforms with identity/path/activity metadata; select the active Animator and its skin palette rather than matching duplicate bone names blindly. Model and ragdoll paths are distinct.

End-of-frame sampling captures a consistent final displayed pose after ordinary animation/IK processing, not an intermediate unmodified muscle pose or collision-physics phase. It therefore includes live IK, controller transitions, body scaling, and equipment attachments. Root-relative transforms can be computed using the recorded Animator root inverse multiplied by a bone's world matrix. Compare male/female and third/first-person captures separately and use evaluated normalized time plus actual clip weights; do not infer clip speed from wall time alone.

## Validation

Six headless tests use a fake Mono library and process. They verify that a wrong callback and a worker thread cannot install, the exact executable/library guards hold, installation runs once, and capture paths are fresh and profile-scoped. The real probe compiles against the installed `mscorlib.dll` and `UnityEngine.dll`, and native tools compile with C11 warnings treated as errors. The subsequent original-runtime test independently confirmed the actual invocation path and acceptance of the external MonoBehaviour. Capture analysis must still verify frame-consistent matrices and matching palettes before exporting a recording; the headless probe tests alone do not establish pose accuracy.

## First original-runtime recording

The original was launched with the isolated `planet-explorers-pose-reference` profile and navigated through the tutorial using normal keyboard/mouse input. No mission, equipment, Animator, or player state was assigned by the probe. At mission 746, after equipping the supplied knife, the recording was armed for a bounded sequence of isolated clicks, held-button input, rapid clicks and F5 camera changes. Input timestamps are retained separately; their intended camera labels must be checked against the captured Animator parameters.

Local evidence lives under `/home/admin/.local/share/planet-explorers-pose-reference/pose-captures/`:

- `knife-reference-01.launch.json`: launch settings and limits.
- `knife-reference-01.actions.json`: input sequence timestamps.
- `knife-reference-01.jsonl`: 566 frames spanning 45.60962 seconds, followed by an explicit `stop-file` end record.

The installation log reports both `[pose] Probe installed on Unity main thread` and the arm-file wait. Observed identities include `FemaleModel(Clone)/Model` and `FemaleAvatar`. Clips include Attack6–9 and firstpersonSwordAttack1–4, confirming that ordinary input reached both stationary knife families. The game was paused after capture. This run does not demonstrate a successful enemy hit or kill. Its low sampling rate and live transition/IK influence must remain visible in any playback/export claims. See [the capture analyzer audit](../../docs/audit/POSE_CAPTURE_ANALYSIS.md) for validation and export results.
