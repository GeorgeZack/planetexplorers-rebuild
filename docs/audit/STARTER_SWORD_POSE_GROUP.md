# Starter sword/shield pose group

October 6, 2026. This records source/capture asset qualification and the subsequent offline C actor groundwork. The staged assets are not installed; normal Sword/Shield gameplay remains disabled, and no new graphical check is established. See [the equipment source audit](STARTER_SWORD_SHIELD.md) for the exact modern item contracts.

`export_starter_pair_assets.py` recovers the modern ResourceManager Wood Sword1 and Wood Shield34 rigid meshes/materials, sword mountMain/mountBack, shield animated Bone L Forearm and the source sword capsule. The shield has no source collider; no shield block capsule is invented. The animated forearm is checked against the exact Avatar path and skin palette, rather than incorrectly requiring an animated pose to equal the prefab bind rotation. Main/back marker local transforms remain strict source checks.

The original `female-sword-01.jsonl` capture supplied304 poses for ten exact female Sword leaves. It completed with120 cleanup watchdog frames; root observed a responsive original UI and Quit confirmation followed by native exit137, not a clean zero exit. These sampled leaves do not recover controller blending, masked shield blocking, draw/stow transitions or inverse kinematics.

## Separate reaction group

October 12: the 25 hit-reaction, get-up and sprint-attack leaves are staged
apart from this group as `rebuild/build/starter-pair-reaction-01`
(`export_starter_pair_unified.py --standalone-reaction`), because appending
them to the 42-clip defence actor would pass the 64-clip limit. The 33/42
stage below is unchanged. See
[the reaction contract](REACTION_PRESENTATION.md#bake-result-and-staged-group).

## Current33/42 stage

The completed `female-sword-backward-02.jsonl` capture now qualifies SwordWB441 and SwordRB708 separately:26+22 samples, looping at30Hz, exact lengths0.8000030517578125s and0.6666717529296875s. All42 body bones map; maximum hierarchy matrix error is1.036415e−6. Source clip identity, length, loop and launch provenance pass the analyzer. These are isolated sampled poses, not measured root motion or the full controller graph.

`rebuild/build/starter-pair-unified-03` is the **current integration candidate**:33 body/drawn/stowed/shield clips and one42-clip defence actor. It appends the two backward clips to unified02 without altering any existing31 body/equipment or40 defence payload, inverse bind, texture or capsule. Both earlier stages remain preserved and installed assets are untouched. New inputs are `rebuild/build/sword-backward-body-02/assets/all` and `rebuild/build/starter-pair-sword-backward-assets-02`.

The original capture completed and120 cleanup watchdog updates followed while the UI remained responsive. Root advanced Andy's Continue to Camera tips, opened SYSTEM and selected QuitGame→YES. The sidecar records−9 (signal9); launcher137 matches the previously observed original self-kill pattern, not exit0. The run used process-only Zink options `flushsync,nobgc`; this successful bounded capture does not establish a general crash fix. [Compact provenance and checks](../reference/sword-backward-pose-result.json) preserve the distinct capture, source, headless and lifecycle evidence.

Extension command, after analyzer export and `export_starter_pair_assets.py --pose-set sword-backward`:

```sh
TMPDIR="$PWD/rebuild/build/compiler-tmp" rebuild/build/unity-tools/bin/python rebuild/tools/export_starter_pair_unified.py \
  --append-backward-to rebuild/build/starter-pair-unified-02 \
  --sword-pair rebuild/build/starter-pair-sword-backward-assets-02 \
  --sword-body rebuild/build/sword-backward-body-02/assets/all \
  --sword-capture /home/admin/.local/share/planet-explorers-bake-reference/pose-captures/female-sword-backward-02.jsonl \
  --output rebuild/build/starter-pair-unified-FRESH
```

Followup checks are separate from the earlier full project suite: seven unified-stage tests pass normally and with native ASan/UBSan leak checks, including native33/42 loading and every frame of all42 defence clips. Ten backward attachment checks compare every palette against the actual sampled body bone and validate source capsule/material contracts. No graphics or installation occurred during export/verification.

## Earlier31/40 stage, preserved as input

`rebuild/build/starter-pair-unified-02` contains:

- `player/recording.peas`: existing21 body clips plus ten Sword leaves,31 total.
- `player-wood-sword-drawn/recording.peas`, `player-wood-sword-stowed/recording.peas`, `player-wood-shield/recording.peas`: identical31 clocks, with actual sampled attachment palettes.
- `player-wood-sword-drawn/wood-sword-drawn.pehb`: exact source attack capsule, requiring separate host authorization.
- `player-defence/player-defence.peac` and `.pehb`: one40-clip defence actor, preserving the existing30 clips (including all nine Felling/Spade/Mine clips) and adding ten Sword leaves. Every body clip has an exact matching defence name/clock.
- `unified-manifest.json`: source/file hashes, preserved prefix counts, limitations and `installed:false`.

Existing actor geometry, inverse binds and retained clip payloads are byte-identical; only the clip-count header changes in combined files. Existing defence capsules are byte-identical. All existing installed groups remain untouched. The separate initial21-clip wood stage and ten-clip Sword stage are preserved as inputs. An earlier `starter-pair-unified-01` experiment split defence30+10; it is superseded by unified02 and is not the integration target.

The PEAC v1/v2 encoding is unchanged. Native/offline clip-count limits now share64; the only fixed per-scene clip array uses the same bound. Per-file256MiB, scene512MiB, vertex/index/joint/frame and twelve-pose blending bounds remain unchanged. The original Unity sampler's separate32 retained-graph bound remains32.

Reproduction, using a **fresh** output path:

```sh
TMPDIR="$PWD/rebuild/build/compiler-tmp" rebuild/build/unity-tools/bin/python rebuild/tools/export_starter_pair_unified.py \
  --normal-pair rebuild/build/starter-pair-assets-01 \
  --sword-pair rebuild/build/starter-pair-sword-assets-01 \
  --sword-body rebuild/build/sword-body-01/all \
  --sword-capture /home/admin/.local/share/planet-explorers-bake-reference/pose-captures/female-sword-01.jsonl \
  --output rebuild/build/starter-pair-unified-FRESH
```

`test_starter_pair_unified.py` checks source hashes, exact concatenated payloads/materials, retained30 defence provenance, native31/40 loading and all40 defence clips' frame/capsule queries. The actor/scene C fixtures cover40 and64 clips, reject65, reject truncated final headers/palettes and retain existing scene clock mismatch checks. Existing attachment tests compare every recovered rigid palette with the corresponding body bone palette. These are headless checks, not visual verification.

## Directional mapping gate

Read-only installed `sharedassets1.assets`, FemaleAnimator2 controller779, `Base Layer.GroundLocomotion.Move`, Sword subtree nodes `0→1→41` establishes the following. Node41 blends `RightSpeed/ForwardSpeed`; style selection uses the authored MoveStyleH/V point near(.94,.342). The bounded full subtree extraction is retained locally at `rebuild/build/starter-unified-tests/sword-directions.json`.

| Branch | Authored leaf | Available in unified33? |
|---|---|---|
| Idle node42 | AttackIdle90 | Yes |
| Forward run43→45, ForwardAngle threshold−15 | SwordRF435 | Yes |
| Forward walk61→63, ForwardAngle threshold−15 | SwordWF133 | Yes |
| Forward downhill43→44 or61→62, threshold−25 | NormalRunForwardDown755 | No |
| Backward run53 at(0,−5) | SwordRB708 | Yes, captured backward02 |
| Backward walk68 at(0,−2) | SwordWB441 | Yes, captured backward02 |
| TP right run47→48 / left run57→58 (`FirstPerson=0`) | NormalRunTurnRight754 / Left753 | Yes, shared source leaves |
| TP right walk65→66 (`FirstPerson=0`) | NormalWalkTurnRight752 | Yes, shared source leaf |
| Left walk69 direct; node70→71 at RightSpeed threshold0 | NormalWalkTurnLeft751 | Yes, shared source leaf |
| Node70→72 at RightSpeed threshold1 | SwordWL463 | No; literal source parameter is RightSpeed, not FirstPerson |
| Sprint74 | SwordSprint113 | Yes; gameplay support is separate |

Diagonal third-person leaves also use the Normal turning clips751–754. The first-person branches select distinct SwordRR455, SwordRBR716, SwordRBL712, SwordRL105 and SwordWR470; their captures/behavior are outside this stage. The left-walk duplicate point and unusual node70 parameter are source facts, not grounds to silently normalize the controller.

The ten new captured leaves are AttackIdle90, SwordWF133, SwordRF435, SwordSprint113, SwordJump756, SwordFall757, SwordLand758, SwordRunJump272, SwordRunFall284 and SwordRunLand279. The two backward441/708 leaves are now qualified in unified03. Other unsupported branches, including downhill755 and distinct first-person leaves, remain unavailable. NormalWB/RB are not source-equivalent replacements. The shared TP turning leaves can be reused only under the verified branch conditions. The current native locomotion smoother remains an explicit adapter rather than the Unity blend graph.

## Earlier backward capture preparation, now completed

The separate `reference_animator_bake.py --preset sword-backward` now selects only exact female SwordWB441 and SwordRB708, preserving the ten-leaf `torch-sword` preset and existing defaults. Source validation follows BeatWeight0 → Sword style(.94,.342) → directional points(0,−2)/(0,−5), checking identity, loop clocks and state speed1. The separate preset planned48 poses at30Hz; backward02 subsequently captured and validated all48. Installed-source inventory, separate managed compilation,22 wrapper/source tests and12 analyzer tests pass without skips. Local evidence is `rebuild/build/sword-backward-bake/result.json`. The preparation occurred after the earlier full optimized suite and had separate focused checks. The later completed backward02 capture and current33/42 stage above supersede the original preparation-only status. Installation remains outstanding.


## Current C actor and host integration groundwork

`pe_sandbox_actor` now loads an optional complete33-clip body plus Wood Sword1 drawn/stowed and Shield34 attachment group, checking all ordered clocks and exact female source leaves before GPU upload. Entirely absent wood groups preserve installed21-clip Knife behavior; partial or mismatched groups fail loading atomically. Item-specific `melee_assets_for_item`, `set_melee_item` and `running_ready_for_item` APIs retain the original Knife33 wrappers. Attack6–9 and running408 snapshots copy the authoritative combat clock and never blend an attack with locomotion.

The grounded third-person Sword adapter selects AttackIdle, SwordWF/RF, the newly captured SwordWB441/RB708 and the verified shared Normal turning leaves. One locomotion snapshot supplies normalized weights and phase-mapped authored times to the body, sword and passive shield. The read-only `PeSandboxBodyPose` exposes exactly those names, clocks and weights; the host's `hunt_player_pose` resolves the same snapshot against the42-clip defence actor, failing closed on missing names or clock mismatches. This does not introduce a second animation/contact clock or port Unity's blend graph.

`shield_ready_for_item` currently permits only unarmed/Sword presentation. `set_shield` binds an exact transient instance; every draw rechecks `PeGame.shield_instance_id` through the existing `pe_shield_equipped` ownership validator. Unsupported primary tools/Knife advertise unavailable while a shield is bound. Replacing or removing an instance cannot retain its old pose, reset clears the transient binding, and UI preview does not mutate the world snapshot. This is a passive forearm attachment, not a masked block pose or an inferred block collider/protection gate.

The Sword ground-readiness helper rejects sprint, airborne/jump/land states and remaining weighted air contributors. A safe grounded presentation reset is required after a prior jump until stationary/running air-family selection is integrated. It does **not** recover Unity `ForwardAngle` or first-person controller parameters; those need separate host admission rules. The actor does not claim a Normal clip as a missing Sword air/slope/FP pose. The current host helper stows Sword presentation when the grounded helper returns false; that compatibility behavior is not qualified Sword air movement. Normal host `starter_pair_available` remains false: ordinary starter Sword/Shield use stays disabled pending the complete block/contact/presentation path. The staged33/42 files remain separate from installed assets.

The actual source Wood Sword core is also tested with recovered blade contacts: five durability per accepted damage notification, no wear on a miss or guide-deduplicated target, per-target breakage/depletion affecting subsequent contacts, calculated overkill and rollback after a late failure. Shield/outfit wear and HP changes retain their own staged transaction. This is headless contact evidence, not a new physical hunt, block or balance result.

Verification is deliberately scoped:

- `sandbox-actor-test` passes optimized and under ASan/UBSan with explicit leak detection. It covers stale instances, resets, death, preview purity, exact action clocks, source backward/turn mapping, malformed groups and unsupported-state rejection. Its intentional simulated draw-failure diagnostic is part of the passing failure-path test.
- `test_sandbox_actor_assets.py` passes normally and sanitized with zero skips, using actual installed21 assets, standalone staged33 assets and a read-only mixed overlay. Real PEAC parsing, CPU skin calculations, matching body/weapon/shield contributors and source sword PEHB binding are checked; GPU upload/drawing are mocked, so this is not graphical coverage.
- `rebuild/build/sword-wear/result.json` records optimized and sanitizer build/run exits0. The corresponding logs explicitly report real source blade, shield and outfit contact/wear checks.
- `rebuild/build/starter-pair-next/integration-full.log` records a full test run reported exit0, with29 optional skips. It **predates the latest host/UI corrections**, so it is not a blanket latest-tree full-suite claim. The reviewed latest followup `make -j2 all sandbox-ui-test training-asset-clocks-test hunt-consume-test` exits0 in `integration-reviewed.log`; the earlier `integration-build.log` is preserved. `integration-ui-sanitize.log` records UI and tutorial asset-clock sanitizer checks. The current focused game test also passed under ASan/UBSan (`game-focused.log`, success is silent). These are separate followup checks, not a new full sweep.
- `sandbox-equipment-host-test` exits0 in `equipment-host-target.log`. The actual static host helpers pass strict optimized and ASan/UBSan/leak builds with zero warnings/skips (`rebuild/build/equipment-host/verification.json`), covering shared action/body pose dispatch, source clocks, old-save asset readiness and depleted Sword handling. Their actor/asset callbacks are mocked; the real geometry evidence remains the separate actor/asset/contact tests above.

No desktop input, native graphical test, asset installation or playable Sword/Shield claim was added by this integration groundwork. The next gate is a coherent qualified block pose/controller adapter, host activation with exact incoming/outgoing ownership and movement limits, then explicit installation and native graphical/regression checks.
