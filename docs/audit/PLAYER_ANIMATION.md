# Player knife animation recovery

The installed Unity 5.2.4f1 player animation state machine and exact authored clip events are recoverable. They are now inventoried by [`inventory_player_animation.py`](../../rebuild/tools/inventory_player_animation.py). The original runtime now supplies a validated recording of evaluated female knife poses, and the C renderer can replay those sampled matrices on recovered body parts. General humanoid muscle/Avatar evaluation, Animator blending and root motion remain unfinished; this recording is a calibration reference, not a complete replacement Animator.

[TRAINING_COMBAT.md](TRAINING_COMBAT.md) contains the verified skill/damage/stamina rules and current implemented equipment/health status. [ASSET_RECOVERY.md](ASSET_RECOVERY.md) covers visual asset exports. The original tables below come from file inspection. The runtime-calibration section records the subsequent automated play/capture test separately.

## Reproduce the inventory

Use an isolated Python environment with UnityPy 1.25.4 (the tested version):

```sh
python rebuild/tools/inventory_player_animation.py
python rebuild/tools/inventory_player_animation.py --game-dir '/path/to/Planet Explorers' \
  --raw-dir rebuild/build/player-animation-clips
```

Default output is ignored `rebuild/build/player-animation.json`. It includes source SHA256, Unity version, controller/state/clip object IDs, male overrides, authored events, transition conditions, raw blend nodes, curve-format counts, both Avatar records, and source prefab mount ancestry/TRS. The optional directory adds selected raw clip type trees for later pose decoding. The tool refuses output inside the installed game directory, streams the serialized asset, and does not require loading the 1.9 GB `resources.assets` blob.

The actual run exported **13 states and 35 clips**, including stationary attack6–9, run/sprint/air/water knife variants, and hold/stow states. Syntax/help checks and extraction against the installed file passed. The report is an inventory, not a runtime pose sampler.

## Actual files and Animator mapping

The installed file is `PE_Client_Data/sharedassets1.assets`, approximately 85 MB, Unity 5.2.4f1. Its SHA256 for this installation is:

```
d9c65f820a2186116d9eb73d815fe07d9f46924ddfd103ee9510bf254f9a3d67
```

| Object type | Path ID | Name |
|---|---:|---|
| AnimatorController |779|FemaleAnimator2|
| AnimatorOverrideController |780|MaleAnimator|
| Avatar |777|FemaleAvatar|
| Avatar |778|MaleAvatar|

The male override references controller 779; it changes clips while retaining the common state-machine structure. All selected clip references are local to this file. `resources.assets` contains monster controllers and menu-only MaleAC/FamleAC, which must not be mistaken for the playable human controller.

The source `MaleModel.prefab` and `FemaleModel.prefab` reference external controller/avatar GUIDs whose target assets are absent from the checked-in source. The built controller preserves names such as `Assets/Resources/Model/PlayerModel/FemaleAttack2.FBXAttack6`; that original FBX is likewise absent from this source checkout. Missing editor assets therefore do not mean the shipped clips are missing.

`skEffect 8` drives triggers `attack6,attack7,attack8,attack9`. Their controller states are `FullBody.Attack.Attack6` through `Attack9`, on state machine 1. Incoming AnyState transitions use those exact lower-case trigger names. Attack6's entry has no exit-time requirement, offset 0, serialized transition duration 0.009999973 with `m_HasFixedDuration=false`; its outgoing transition has exit time 0.7, duration 0.15, and fixed duration enabled. Full transition records are retained in JSON.

Each state has speed 1 and a 1D blend on `FirstPerson` with thresholds 0 and 1. The clip mappings are:

| State | View | Female clip | Male override |
|---|---|---:|---:|
| Attack6 | TP | 231 | 179 |
| Attack6 | FP | 499 | 419 |
| Attack7 | TP | 240 | 192 |
| Attack7 | FP | 504 | 424 |
| Attack8 | TP | 247 | 203 |
| Attack8 | FP | 509 | 428 |
| Attack9 | TP | 254 | 213 |
| Attack9 | FP | 515 | 433 |

The TP leaf's serialized `m_Duration` is 1.0 and FP leaf's is 1.538461566. The subsequent female runtime capture measured FP authored clip time at .65 times Animator.speed, consistent with the inverse of that compiled duration. TP clip time follows Animator.speed. This calibration applies to stable stationary endpoints; transitions, partial camera blends and hit response still need separate evaluation.

## Authored event times

Times below are seconds **on the clip timeline**, rounded to six decimals. JSON preserves the decoded float values. TP and FP callbacks are mode-filtered by `Action_Sword.OnAnimEvent`; the other view's events must not fire an additional hit window.

| Gender/view | Stage (clip ID) | Hit starts | Hit ends | Action ends |
|---|---|---:|---:|---:|
| Female TP | Attack6 (231) | 0.068724 | 0.299463 | 0.709126 |
| Male TP | Attack6 (179) | 0.123690 | 0.342917 | 0.910350 |
| Female FP | Attack6 (499) | 0.053574 | 0.329773 | 0.791521 |
| Male FP | Attack6 (419) | 0.230302 | 0.392564 | 0.963092 |
| Female TP | Attack7 (240) | 0.173136 | 0.464701 | 0.836943 |
| Male TP | Attack7 (192) | 0.130770 | 0.342847 | 1.010363 |
| Female FP | Attack7 (504) | 0.104828 | 0.437214 | 0.878696 |
| Male FP | Attack7 (424) | 0.237297 | 0.563597 | 0.955234 |
| Female TP | Attack8 (247) | 0.138959 | 0.425486 | 0.873210 |
| Male TP | Attack8 (203) | 0.129094 | 0.398491 | 0.660396 |
| Female FP | Attack8 (509) | 0.109243 | 0.417865 | 0.884263 |
| Male FP | Attack8 (428) | 0.225617 | 0.347108 | 1.104955 |
| Female TP | Attack9 (254) | 0.038513 | 0.253554 | 0.534951 |
| Male TP | Attack9 (213) | 0.418563 | 0.732314 | 1.231955 |
| Female FP | Attack9 (515) | 0.058486 | 0.217390 | 0.535519 |
| Male FP | Attack9 (433) | 0.426989 | 0.655806 | 1.192856 |

The inventory also records effect and combo-window events. Ordering differs across clips: for example, female TP Attack6 starts its hit at 0.068724 before `WeightInputStart` at 0.156946, whereas male TP Attack6 opens combo input at 0.089618 before its hit at 0.123690. Do not impose a universal ordering derived from one gender or assume animation events fall exactly on 30 Hz sample boundaries.

Knife source prefab playback uses animation speed 1.37, slowed by factors 0.9 at stamina<=50 and<=20; `Action_Sword` additionally uses a hit-response animation-speed curve. Its EndAttack coroutine guarantees at least two engine frames after activation. EndAction has the source 0.3 s stale-combo guard. Animation state transitions, camera blend, skill continuation, root-motion changes and hit response can all change wall-clock completion; the table is not a fixed cooldown table.

## Hand mounting and hit geometry

The source prefab ancestry is recoverable independently from humanoid animation evaluation. Normal player `mountMain` is under:

```
Model/Bip01/Bip01 Pelvis/Bip01 Spine1/Bip01 Spine2/Bip01 Spine3/
Bip01 R Clavicle/Bip01 R UpperArm/Bip01 R Forearm/Bip01 R Hand/mountMain
```

Both player prefabs also contain a duplicate mount under the ragdoll skeleton. A port must use the currently animated model hierarchy, not whichever duplicate name was encountered first.

| Mount offset relative to right hand | X | Y | Z |
|---|---:|---:|---:|
| Male mountMain |-.066296078|.030821456|-.010829544|
| Female mountMain |-.052396659|.033852614|.000451355|

The JSON retains full quaternion rotations, scales, all ancestor TRS, and stowed mountWaist chains. The knife child `knife/SingleHandSword` has identity local transform. It carries the source capsule radius 0.3, height 1.5, local Z axis and offset (0,0,-0.1). Thus exact combat geometry requires the dynamic right-hand transform and correct mounted knife transform during the authored window; a static forward capsule would still be an approximation.

Both built Avatar records resolve right-hand path hash 2132383290 to the expected right-hand hierarchy. They contain skeleton/default poses, humanoid mapping arrays, root-motion skeleton and Avatar human data. These are included in the report for a future retargeting implementation.

## Pose format and port feasibility

The selected clips have zero ordinary position/rotation/scale curves. Their content is inside `m_MuscleClip.m_Clip.data`: dense sampled values, constants and a streamed curve container. Representative Attack6 records:

| Clip | Dense frames | Dense channels | Dense rate | Constant values | Direct transform bindings | Humanoid bindings |
|---|---:|---:|---:|---:|---:|---:|
| Male TP 179 |38|136|30|102|33|127|
| Female TP 231 |34|117|30|103|28|127|
| Male FP 419 |40|135|30|102|33|127|
| Female FP 499 |34|120|30|110|31|127|

These four have zero streamed curves, but the other selected clips must be handled according to their own inventories. Direct transform bindings use classID 4; the main humanoid channels use classID 95/customType 8. A plain quaternion-per-bone decoder is insufficient: interpreting humanoid channels as skeletal TRS would corrupt the pose. The Avatar's human/skeleton mapping and coordinate/retargeting rules must be evaluated, along with root motion, for a faithful hand trajectory.

A practical staged port can now use verified clip-event schedules and source stamina/action rules while keeping actual attack contact unavailable. The next substantive recovery step is a validated humanoid pose sampler: decode packed channel order, apply Avatar muscle limits/bone mapping, compose skeleton transforms, attach mountMain, and compare sampled hand/root trajectories against the original runtime. After that, add the state-machine blend/transition and hit-response behavior. Recovering the event times removes a major uncertainty, but does not justify a fabricated static hit volume or a weapon-metadata timer.

## Standalone action core

`rebuild/src/pe_combat.c/h` now implements the stationary knife skill's four-stage mechanical schedule for both genders and both camera views. Its compact table contains the 16 exact clip event sequences above, with installed-file SHA256 provenance. `tests/test_combat.c` validates all four schedule combinations, contact boundaries, combo progression, stamina, target deduplication, invalid inputs and the source damage expression. Strict C11, ASan, UBSan and leak checks passed for this standalone suite.

The integration contract deliberately separates **authored clip seconds** from **scaled game seconds** (`Time.time` action age):

1. Initialize `PeCombat`, then call `pe_combat_begin` only after the host verifies the weapon is held, the player is alive, skill cooldown permits it, and competing actions allow it. Beginning charges five stamina times the reduction attribute, clamped at zero; there is no low-stamina rejection.
2. Call `pe_combat_advance(action, clip_delta, wall_delta, &stamina)` once per actual engine update. The host supplies the evaluated Animator clip clock; the legacy `wall_delta` argument is the scaled `Time.time` delta, not `realtimeSinceStartup` or an unscaled wall-clock delta. `pe_combat_animation_speed` returns the verified knife stamina speed multiplier, while `pe_combat_stationary_clip_rate(view, animator_speed)` supplies the now-calibrated TP/FP endpoint factor. Neither helper evaluates transitions or the hit-response speed curve.
3. Submit combo requests only through `pe_combat_queue_combo`. Requests outside the current authored input window are ignored. An accepted request advances at EndAttack, after the source minimum two updates since hit activation, charges the next swing, and returns `PE_COMBAT_NEXT_STAGE`. The host starts the next animation at zero; leftover old-clip delta is discarded. A queue supports the first three transitions of this bounded four-stage skill; continuous held-button restart and a queue beyond stage four are not simulated.
4. Contact is accepted only while the event-driven window is open. `pe_combat_record_contact` takes a stable target entity ID **after** the host performs spatial and damage-permission checks. `SkInst.cs:297–309` explicitly deduplicates target entities per guide, clearing the list at line 377; the native module preserves that behavior per stage. The native bounded list holds 64 distinct entities and rejects overflow without replacing earlier records.
5. The host suppresses stamina recovery while `action.active`. The module neither regenerates stamina nor handles movement/root motion. EndAction applies the source 0.3-second scaled-game-time guard; stale events are ignored, not deferred until that time. The host can stop explicitly when animation/action cancellation occurs.

`pe_combat_damage` implements only the verified numeric expression, including shield/critical/contact multipliers, and ignores tiny/nonpositive contact scales using the original Linux epsilon. It leaves HP, XP, permissions, RNG, hit response and death handling to the host. Its nonnegative attack/defense and normalized shield-protection inputs are validated; zero attack returns zero rather than producing an undefined 0/0.

[Training melee](TRAINING_MELEE.md) connects this core to actual posed geometry and death progression. [Sandbox melee](SANDBOX_RUNNING_KNIFE.md) owns its broader ordinary/running/sprint integration. This stationary module alone supplies neither spatial permission nor full skill-runtime/Animator behavior; no fabricated hit/death shortcut is exposed.

## Original-runtime clock and pose calibration, 2026-10-03

A fresh isolated native Unity 5.2.4f1 session reached mission 746 through ordinary controls, equipped the supplied knife, and recorded 566 end-of-frame poses over 45.60962 seconds. The capture includes all four female stationary combo clips in both views. The capture ended cleanly by stop file; no successful enemy damage or kill was observed in this sequence. Tool/provenance details are in [REFERENCE_POSE.md](../../rebuild/tools/REFERENCE_POSE.md), with strict validation and export in [POSE_CAPTURE_ANALYSIS.md](POSE_CAPTURE_ANALYSIS.md).

Stable, nontransition, dominant-clip observations measured the following approximate rates in authored clip seconds per scaled game second:

| Family | Animator.speed | Measured clip rate | Endpoint factor |
|---|---:|---:|---:|
| Female TP Attack6–9 |1.37|1.3697–1.3703|1|
| Female FP SwordAttack1–4 |1.37|.8904–.8905|.65|
| Female TP Attack8, slowed |1.233|1.2331|1|
| Female FP, slowed |1.233|approximately .8014|.65|

`AnimSpeed`, the Animator parameter, remained 1; the Animator object's actual `speed` changed. These are distinct fields. The C core now exposes the bounded stationary endpoint conversion and tests its consequence: after .25 scaled game seconds at Animator.speed 1.37, the female TP first swing has closed its contact window while the FP first swing remains open. This does not prove that every runtime damage callback occurred exactly at the mathematical boundary. The male controller shares the source blend configuration, but male runtime poses were not captured.

The capture also reveals that `MainPlayerCmpt`'s component transform remains at `(27,2,11)` while the displayed model moves. World placement must use the captured model/Animator root. Exported matrices remove that model root, preserving a shared coordinate system for all body parts. A 180-frame C graphics smoke played nine body/material parts successfully; the initial export contains the default unclothed body, so it verifies skeletal playback rather than the tutorial outfit. The subsequent starter export reconstructs the 67-entry runtime palette order; an eleven-part C preview now includes the original outfit and captured knife attachment. See the [playtest](../PLAYTEST.md). The recording's roughly 12Hz source samples and linear interpolation do not establish 60Hz Unity pose equivalence.
