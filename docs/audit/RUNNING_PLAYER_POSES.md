# Running Knife33 pose staging and installation

October6,2026. The exact female RunSwordAttack408 clip has been sampled from the installed original and staged with matching body, drawn/stowed Knife33 and player defence. **The four pose groups and separately qualified root-motion curve are now installed locally with preserved backups.** Installation does not establish native graphical/gameplay validation. Pose recovery alone does not supply authoritative root displacement or authorize ordinary WASD movement during the source attack.

## Original capture and lifecycle

The source controller is FemaleAnimator2, object779 in installed sharedassets1.assets. RunSwordAttack is the direct `FullBody.Attack.RunSwordAttack` leaf, source clip408. The isolated sampler records57 frames at30Hz over1.8333396911621094 authored seconds, with the exact terminal frame and `loop=false`. The analyzer found no missing rig bones and detected actual pose motion. The original ten/twenty-clip default groups were not replaced by the sampler.

The desktop operator launched a separate sampler process and armed the female player in the tutorial. Capture:

`~/.local/share/planet-explorers-strafe-reference/pose-captures/female-running-pose-01.jsonl`

SHA-256: `47f842efc9d751f8c9d47635824207b93ed901ad03b1660138dde36a12e3bb5e`.

The capture closed with reason `complete` and57 samples. The operator verified the post-cleanup120-frame watchdog, then clicked the first Andy reply and reached the CameraTips instruction (`rebuild/build/running-pose-after.png`). Ordinary Quit Game and confirmation produced the original's observed self-kill result: exit sidecar `returncode=-9`, signal9, at2026-10-06T16:18:06.894444Z. This establishes post-sampling responsiveness and records actual exit behavior; it is not an exit0 claim. No native graphics result follows from this original sampling run.

The sampler uses isolated pure Animator hierarchies, detached controllers and `applyRootMotion=false`. Recorded root diagnostics are not accumulated gameplay motion. [The next-combat audit](SANDBOX_COMBAT_NEXT.md) describes the separate source skill, movement thresholds and root-motion recovery requirement.

## Reproducible staged extension

[export_running_player.py](../../rebuild/tools/export_running_player.py) consumes the closed single-clip capture with launch provenance, the preserved twenty-clip [directional capture](DIRECTIONAL_PLAYER.md), starter rig/materials and existing Felling/Spade/Mine captures. It appends only compatible PEAC2 payloads after checking identical mesh geometry, joint/bind data, textures, material ordering and authored hitbox bytes. Old clip blocks remain byte-identical; only the header clip count changes. Conflicting names, excess clips, truncated data and changed geometry/binding fail.

| Staged group | Clips | New leaf |
| --- | ---: | --- |
| player |21|RunSwordAttack at index20|
| player-knife-drawn |21|same exact clock at index20; original mountMain and attack capsule|
| player-knife-stowed |21|same exact clock at index20; original mountWaist|
| player-defence |30|same exact clock at index29;23 joints and16 authored capsules|

The defence retains its prior twenty-nine clips in their original order, then appends the running leaf. This includes all existing tool-action defence clocks. Other tool attachment scenes stay in the installed twenty-clip group and are not altered by staging or this installation. Runtime code must resolve clips by name rather than assuming the defence shares the body's running index.

```sh
rebuild/build/unity-tools/bin/python rebuild/tools/export_running_player.py \
  --capture "$HOME/.local/share/planet-explorers-strafe-reference/pose-captures/female-running-pose-01.jsonl" \
  --base-capture "$HOME/.local/share/planet-explorers-strafe-reference/pose-captures/female-directional-01.jsonl" \
  --extra-capture "$HOME/.local/share/planet-explorers-bake-reference/pose-captures/female-clips-08.jsonl" \
  --extra-capture "$HOME/.local/share/planet-explorers-bake-reference/pose-captures/female-clips-09.jsonl" \
  --extra-capture "$HOME/.local/share/planet-explorers-bake-reference/pose-captures/female-mine-10.jsonl" \
  --stage rebuild/build/running-player-20261006
```

The stage path must be fresh. The completed local stage contains34 files across the four candidate groups, with capture/rig/input hashes and a snapshot of the installed base file hashes. `running-manifest.json` SHA-256 is `545d4cc21ae5d55b416f92d36b28e8c3c84ca204fa3208286a7daa382360a919`. All installed base file hashes remained unchanged during staging. This exporter has no installation or runtime-enabling mode; the subsequent explicit installation is recorded below.

## Verification and remaining integration

Five [focused tests](../../rebuild/tests/test_running_player.py) pass. Synthetic tests cover byte preservation, duplicate/overflow/truncated or changed-bind rejection, immutable inputs, and texture/hitbox mismatch failing before output creation. The optional real-stage test loads all three scenes through the native C manifest parser, checks the running knife capsule at every new frame and poses all sixteen defence capsules across935 total frames,14,960 capsule poses. ASan/UBSan with leak detection passes. All thirty defence clips reconstruct against source bone-local evaluation with maximum error1.8744091e-7m; the new running clip's maximum error is1.2116514e-7m. These checks are separate from ordinary-interpreter optional UnityPy skips.

```sh
TMPDIR="$PWD/rebuild/build/compiler-tmp" python3 rebuild/tests/test_running_player.py
```

`PE_RUNNING_STAGE` can select another compatible stage. The optional test verifies the original base against the manifest hash snapshot, using the installed base before installation or the preserved `player-before-running-20261006` backup afterward. `PE_RUNNING_BASE` selects an explicit preserved base. When the running group is installed, its files must match the stage and the native validator reads the installed scenes; it never compares retained prefixes against themselves.

The separate root-motion curve now passes strict clone qualification and is installed with these reviewed poses. Native running-attack validation must still establish the correct source skill/timing, swept collision movement and shared body/weapon/defence clocks; installation alone does not establish those outcomes. Sprint, first-person and airborne attacks, full Animator blends and procedural IK remain separate work.


## Explicit reversible installation

After root06 qualified and the desktop operator authorized installation, a bounded offline transaction verified all34 staged file hashes and all34 existing base hashes against `running-manifest.json`. It also checked retained actor bytes, exact new body/knife/defence clocks and the separate motion export's deterministic check before creating incoming copies. No C client was active; the original Steam process uses different installed assets.

The transaction renamed the existing `player`, `player-knife-drawn`, `player-knife-stowed` and `player-defence` directories into fresh `rebuild/build/player-before-running-20261006`, then installed those four staged groups and `player-running-motion` beneath `rebuild/build/reference-assets`. Directory swaps had rollback handling. All34 installed pose files and both motion files match their stages; all32 files in the other directional groups remain unchanged. Both source stages remain intact. The backup's `installation.json` records old/new hashes, with SHA256 `614a7c12aa8674579882dd0fb7140686de3c18ee21ef35b41ce1ef1588870ff8`; [the bounded installation record](../reference/running-player-install.json) records the separate installed status. The original stage manifest's `installed=false` is its preserved export-time state.

Installed checks pass: native ASan/UBSan with leak detection loads all three21-clip scenes, poses all57 running blade frames and all16 defence capsules across30 clips/935 frames (14,960 capsule poses). The installed curve passes strict exporter `--check` and3,547 actual raw-source sample/interval comparisons under the same sanitizers. Five pose tests, eight motion exporter tests, seven axe tests and seven pickaxe tests pass; the axe/pickaxe suites used the installed UnityPy environment with no skips. The tool comparisons allow only the body's appended Knife-only RunSwordAttack leaf while retaining exact equality for every shared tool clip, including all side/back locomotion and separate action groups. No runtime code, generated/content outputs or game profiles were modified by this installation.
