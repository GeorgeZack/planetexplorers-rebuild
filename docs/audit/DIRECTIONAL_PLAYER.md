# Directional player pose recovery

October 6, 2026. The installed original controller supplies distinct side/back and turning clips. Earlier native body/attachment actors contained only ten clips: NormalIdle, NormalWF, NormalRF, Jump, Fall, Land and Attack6–9. Separate Felling/Spade/Mine captures supplied nine action clips; none of those earlier bakes contained side/back locomotion. The installed player defence group contained those nineteen clips. The older auxiliary `build/player-defence` output still contained only ten and must not be mistaken for the installed group.

## Source identities and branch differences

`inventory_reference_bake.py` verifies the exact installed FemaleAnimator2 controller779 in sharedassets1.assets, its male override identities, humanoid bindings and unique runtime clip identities. All added leaves are in `Base Layer.GroundLocomotion.Move`, under BeatWeight0 and MoveStyleH/V `(0,0)`. The inventory records and checks the complete branch ancestry, blend parameters, points, thresholds and exact source clip IDs.

| Clip | Female source ID | RightSpeed / ForwardSpeed point | Additional branch |
| --- | ---: | --- | --- |
| NormalWB |547|`(0,-2)`|direct|
| NormalWL |517|`(-2,0)`|FirstPerson1|
| NormalWR |524|`(2,0)`|RightSpeed1|
| NormalRB |696|`(0,-5)`|direct|
| NormalRL |447|`(-5,0)`|FirstPerson1|
| NormalRR |510|`(5,0)`|FirstPerson1|
| NormalWalkTurnLeft |751|`(-2,0)`|FirstPerson0|
| NormalWalkTurnRight |752|`(2,0)`|RightSpeed0|
| NormalRunTurnLeft |753|`(-5,0)`|FirstPerson0|
| NormalRunTurnRight |754|`(5,0)`|FirstPerson0|

The walk-right branch really uses RightSpeed rather than FirstPerson in the installed controller; the exporter does not normalize this asymmetry. Selecting the pure side-stepping clips for native fixed-facing third-person movement is an explicit control adapter. These samples alone do not establish the original complete Animator graph, transitions, blend weights or procedural IK. Torch still uses the bounded Normal-style pose group; its separate Sword recovery remains [prepared but unintegrated](TORCH_SWORD_RECOVERY.md).

## Bounded sampler and coherent outputs

`reference_animator_bake.py --preset normal-directional` retains the original ten clips first and appends the ten listed leaves. Female30Hz sampling plans621 poses. Default selection remains the original ten. The Python planner and managed sampler now allow up to32 unique clips, matching the existing native actor format, while retaining the3,600-sample,512-transform and256MiB bounds. Each sampled clip keeps its own inactive graph until process exit; the capture records the32-graph limit. The analyzer accepts historical10-graph recordings under their original limit and new32-graph recordings under the new limit. Completion is separate from the120-update cleanup-survival check and ordinary process exit.

`--single-threaded-rendering` is optional and forwards to the original launcher's existing `-force-gfx-direct` workaround. The default launch command is unchanged. Read [ANIMATOR_BAKE](ANIMATOR_BAKE.md) and [REFERENCE_POSE](../../rebuild/tools/REFERENCE_POSE.md) before a supervised launch. Default tool execution only builds the managed sampler; only `--launch` starts the game and prepares its isolated profile.

`export_directional_player.py` requires a closed exact female20-clip capture with launch provenance, the source starter rig/materials and three existing Felling/Spade/Mine captures. It stages eleven complete scenes: body, held/stowed knife, held/stowed shovel, both held axes, held/stowed pickaxe and held/stowed Torch. It also stages the separate23-joint defence actor and authored capsule file with all29 clips. All attachment parts must match the body's ordered clip headers byte for byte; the first20 defence clocks must also match. Source mount transforms are reused without invented offsets. Existing tool-action scenes remain separate.

The stage manifest hashes every staged file and all input captures. Installation is a separate explicit command, after the native client stops. It first verifies the complete group, copies incoming folders, then renames existing folders to a fresh backup directory. A failed directory rename rolls back the preceding moves. The original stage remains available for provenance and review. This is a sequence of filesystem moves, not an atomic whole-directory swap for readers; do not run the client during installation.

Example staging after the supervised capture closes:

```sh
rebuild/build/unity-tools/bin/python rebuild/tools/export_directional_player.py \
  --capture "$HOME/.local/share/planet-explorers-strafe-reference/pose-captures/female-directional-01.jsonl" \
  --extra-capture "$HOME/.local/share/planet-explorers-bake-reference/pose-captures/female-clips-08.jsonl" \
  --extra-capture "$HOME/.local/share/planet-explorers-bake-reference/pose-captures/female-clips-09.jsonl" \
  --extra-capture "$HOME/.local/share/planet-explorers-bake-reference/pose-captures/female-mine-10.jsonl" \
  --stage rebuild/build/directional-player-20261006
```

Use a fresh stage path if it already exists. Installation, after inspection and stopped-client lifecycle checks:

```sh
rebuild/build/unity-tools/bin/python rebuild/tools/export_directional_player.py \
  --stage rebuild/build/directional-player-20261006 --install \
  --backup rebuild/build/player-before-directional-20261006
```

## Verification status

Eighteen sampler tests pass, including exact branch mutation rejection, unchanged default selection,20-clip preset,32-clip and3,600-sample bounds, managed allowlist synchronization and optional launch flag forwarding without an actual launch. Twelve analyzer tests pass, including historical retention limits,32 distinct retained hierarchies and a32-clip exported scene loaded by the native C parser. Five installer/asset tests pass, including real staged scenes and all29 defence clips through native C loaders and all capsule samples under ASan/UBSan with leak detection. Transaction cases cover: complete preservation, missing/changed/linked or mismatched groups failing before mutation, and mid-install rollback. The managed probe compiled successfully against installed Unity5.2 assemblies. The original capture `female-directional-01.jsonl` closed with621 poses (SHA-256 `62983324ea0e242c1ee084c6bcc5b932ce83e4dc8526a54c0e09f4ac3dece57d`). The desktop operator verified the120-update cleanup-survival log and a responsive subsequent tutorial reply. Ordinary Quit Game and its confirmation ended the installed client with signal9 (`returncode=-9`), recorded at2026-10-06T15:59:27.969125Z; this is not an exit0 claim.

All eleven scenes and the29-clip defence group were staged at `rebuild/build/directional-player-20261006`, then installed after the operator confirmed both original exit and no running native client. All66 installed files exactly match the stage manifest (SHA-256 `cb7cbbafc1187ec5296811c7fd6ab2b54793f05938d8f1d2b89d00e98cdbce55`). The twelve original directories remain in `rebuild/build/player-before-directional-20261006`. Maximum reconstructed defence point error across the29 clips is1.8744091e-7m. The subsequent [native graphical movement check](../PLAYTEST.md#october-6--directional-player-assets-and-native-strafe-check) exercised all six new side/back selections with an existing drawn Knife33 in free-camera sandbox. Selected-clip mask4037 and the [clear right-run image](../reference/strafe-native-run-right.png) provide bounded host evidence; backward views were obscured. The native run ended through Pause→Main menu→Quit with exit0, no attacks and no contacts. This staged hunt-return fixture establishes neither ordinary quest completion nor full original movement/blend parity.

The full15000-frame tutorial regression also passes with exit0 using the installed asset group: tutorial mission756 (gathering ledger752), all nine NPCs,19 construction additions/one removal/score50, and eight melee starts/24 contacts/hareHP0. Locomotion mask63 covers the original six clips; this run does not physically exercise tutorial side/back selection. See `rebuild/build/strafe-training.log` and the [playtest record](../PLAYTEST.md#october-6--directional-player-assets-and-native-strafe-check).

Final full optimized/content/23-CLI verification and full ASan/UBSan with `detect_leaks=1` pass for this directional increment. The sanitizer run exits0 with zero core failures; logs are `rebuild/build/strafe-verification.log` and `strafe-sanitize.log`. Optional skips in ordinary Python suites remain skips, not passed asset checks.

A separate physical captured-camera tutorial check subsequently exercises all six side/back selections:4314 frames, mask4033 (idle plus six directions), no attacks or contacts. [D-running](../reference/strafe-tutorial-d-run.png) and [A-walking](../reference/strafe-tutorial-a-walk.png) frames show the recovered player poses clearly. [Inputs and hashes](../reference/strafe-tutorial-result.json) remain separate from the scripted regression. Native source-turn/blend/handedness parity is not implied; Pause→Main menu followed by main-menu Quit reaped the native process with exit0.
