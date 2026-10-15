# Original game reference runtime

Verified reference: Steam-installed native Linux V1.1.4, Unity 5.2.4f1 (`98095704e6fe`), at `/home/admin/.local/share/Steam/steamapps/common/Planet Explorers`. Menu, Tutorial, Adventure and Single Player Build have run. This establishes a packaged reference, not full parity or a working Unity source build. Ordinary Steam Play/Proton startup remains unverified.

## Launch and profile safety

Use the isolated helper with Steam running:

```sh
mkdir -p rebuild/build/compiler-tmp
TMPDIR="$PWD/rebuild/build/compiler-tmp" python3 rebuild/tools/launch_reference.py \
  --user-data /home/admin/.local/share/planet-explorers-sandbox-reference
```

Default profile without `--user-data`: `/home/admin/.local/share/planet-explorers-reference`. Paths must contain no whitespace: the original argument parser splits on spaces. The helper prepares full game settings (custom quality 6, antialiasing 0), engine preferences (`UnityGraphicsQuality=0`) and an isolated `XDG_CONFIG_HOME=PROFILE/engine-config`. It backs up changed settings and replaces them atomically; ordinary saves/shared engine preferences stay separate. Normal Steam initialization remains enabled.

| Profile | Preserve |
|---|---|
| `planet-explorers-sandbox-reference` | Adventure manual 1 `audit` (`GameSave/save0`), Build manual 2 `buildaudit` (`save1`); later checkpoint slots also contain evidence |
| `planet-explorers-pose-reference` | Live evaluated-pose captures |
| `planet-explorers-bake-reference` | Isolated pose bakes and manual saves |
| `planet-explorers-hunt-reference` | Hunt/defense diagnostic checkpoints |

These roots live under `/home/admin/.local/share`. Hash the relevant manual saves before/after instrumentation; baseline comparisons exclude autosaves. Adventure reload was observed; the initial Build checkpoint was not reload-tested. Do not overwrite either casually.

The TMPDIR override addresses an observed quota failure during LocalDatabase's temporary SQLite copy (`no such table: skEffect` was downstream). The launcher does not set TMPDIR itself. Diagnose current failures before assuming the same cause.

## Input and capture

Only one operator controls the desktop. Use `reference_input.py --pid CURRENT_PID ...`; inspect the current process instead of reusing audit PIDs. Holds are bounded and release in cleanup. Read tool `--help` before use.

[Source-first workflow](audit/SOURCE_FIRST_REBUILD.md) requires a specific unresolved question and smallest useful capture before launching. Inspect existing source/archives first. [Pose observer](../rebuild/tools/REFERENCE_POSE.md) reads live evaluated transforms; [isolated Animator bake](audit/ANIMATOR_BAKE.md) deliberately evaluates cloned clips and has different side effects. Both compile offline by default and need explicit launch. Never confuse leaf-pose bakes with live layer composition or action timing.

Native acceptance can use a fresh supervised private display, following [native private input](audit/NATIVE_PRIVATE_INPUT.md). Never adopt a display or use `-displayfd`; preserve socket ownership and earned-save baselines.

## Graphics diagnostics and limits

| Observation | Supported conclusion |
|---|---|
| Initial native black screen; GLX changed from 0 to 4 samples, then BadMatch/context errors. Same launch rendered after XML antialiasing changed 1→0. | Antialiasing/context transition caused the tested failure; not a universal driver diagnosis. |
| Unity persisted quality 3 and could fail before XML loaded. Helper reset isolated engine quality to 0 and rendered again. | XML-only changes are insufficient for repeatable startup. |
| October 6 cores repeatedly dereferenced 0xc in generated `.gl` code with matching NVIDIA 580.178.04 callers. | Confirmed graphics-worker fault; no specific gameplay/probe/recording cause established, no inspected OOM evidence. |
| `__GL_WRITE_TEXT_SECTION=0` retry crashed identically. | Ineffective for that run. |
| `--single-threaded-rendering` completed a defense session; later shield observer crashed with the same fault. | Useful diagnostic, not a reliable fix. |
| llvmpipe observer recorded 187 frames, slowly, with no qualified shield poses. | Limited software-rendering capture only. |
| Zink rendered on Quadro K1200; user called the Adventure session playable. Later sampler startup aborted in a different Zink/NVIDIA stack. | Usable for one test, not reliable startup or measured performance. |
| `ZINK_DEBUG=flushsync,nobgc` retry completed 48 backward samples and 120 cleanup frames. | One successful comparison; no permanent-fix claim. |

For the process-only Zink configuration used in these tests:

```sh
env -u LIBGL_ALWAYS_SOFTWARE \
  __GLX_VENDOR_LIBRARY_NAME=mesa MESA_LOADER_DRIVER_OVERRIDE=zink \
  GALLIUM_DRIVER=zink VK_DRIVER_FILES=/usr/share/vulkan/icd.d/nvidia_icd.json \
  ZINK_DEBUG=flushsync,nobgc TMPDIR="$PWD/rebuild/build/compiler-tmp" \
  python3 rebuild/tools/launch_reference.py --single-threaded-rendering \
  --user-data /home/admin/.local/share/planet-explorers-sandbox-reference
```

Requires the installed Mesa/NVIDIA ICDs. No packages, system configuration or Steam preferences were changed by this variation. Do not launch over an active session. The launcher places Unity's valueless `-force-gfx-direct` before valued game arguments; appending it last caused a separate IndexOutOfRangeException in PECommandLine.

Evidence: [original fault](reference/user-playtest-crash-20261006.json), [render retries](reference/reference-render-diagnostics-20261006.json), [shield crash](reference/shield-observer-crash-20261006.json), [software run](reference/shield-observer-software-20261006.json), [Zink comparison](reference/shield-zink-workaround-20261006.json), [sampler startup fault](reference/sword-backward-zink-crash-20261006.json).

UI Quit has sometimes ended with status 143 or SIGKILL, including successful instrumented sessions. Record actual teardown status; do not call it exit 0 or invent a crash cause.

## External troubleshooting rollback

These October 3 changes preceded the isolated helper and affected ordinary settings:

| File | Change and rollback |
|---|---|
| Steam installation `OptionConfig.xml` | Windowed, CPU, userdata `/home/admin`; backup `OptionConfig.xml.before-launch-fix-20261003-144221` beside it |
| `/home/admin/PlanetExplorers/Config/SystemSaveData.xml` | Created quality 0 / AA 0; previously absent |
| `/home/admin/.config/unity3d/Pathea Games/Planet Explorers/prefs` | Quality reset to 0; adjacent backup `prefs.before-launch-fix-20261003-144421` |

Quit client/launcher before restoration. Preserve newer user settings, restore named backups and move the newly created XML aside only if restoring the prior absent state. Isolated helper changes can be reversed through their adjacent backups; retain profiles containing saves/captures.

## Source and editor boundary

SystemSettingData defaults AA to 1; custom quality 6 maps positive AA to four samples. Invalid XML restores defaults. `GameConfig` honors only an existing userdata directory. `-ocl Cpu` returns before OpenCL device creation and selects CPU extraction; OpenCL hardware is unnecessary for this tested configuration.

The exact Unity editor/source project has not been built or upgraded. Upstream omitted third-party plugins, leaving source dependencies unresolved. The user's installed Steam build supersedes the failed anonymous SteamCMD/download investigations; those are not installation instructions. A local launch cannot restore the original multiplayer lobby/U-link service.

See [PLAYTEST](PLAYTEST.md) for bounded gameplay observations and [sandbox alignment](audit/SANDBOX_ALIGNMENT.md) for fixture provenance.
