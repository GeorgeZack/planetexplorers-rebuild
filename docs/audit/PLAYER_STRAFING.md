# Player directional locomotion

This audit records source-derived normal movement, the implemented cardinal clip selector, and the remaining host/capture boundary. It extends the earlier [six-clip locomotion audit](PLAYER_LOCOMOTION.md). A clip name or a passing selector test does not establish original-game blend or visual parity.

## Source identity and authored clips

The installed `PE_Client_Data/sharedassets1.assets` has SHA256 `d9c65f820a2186116d9eb73d815fe07d9f46924ddfd103ee9510bf254f9a3d67`. `FemaleAnimator2` is controller object779; `MaleAnimator` override object780 supplies the corresponding male clips. The IDs below are local object IDs in that file. All six added leaves are looping, sampled at30fps, and have compiled leaf duration multiplier1. Native initialization must use the actual exported duration for its actor, not these rounded values.

| Clip | Female ID | Male ID | Female seconds | Male seconds |
| --- | ---: | ---: | ---: | ---: |
| NormalWL | 517 | 483 | 1.1000022888183594 | 1.0666656494140625 |
| NormalWR | 524 | 554 | 1.1000022888183594 | 1.0666656494140625 |
| NormalWB | 547 | 680 | .800000011920929 | 1.066680908203125 |
| NormalRL | 447 | 538 | .6666717529296875 | .6666717529296875 |
| NormalRR | 510 | 546 | .6666717529296875 | .6666793823242188 |
| NormalRB | 696 | 692 | .6666717529296875 | .6666793823242188 |

Existing forward clips are NormalWF531/560 and NormalRF539/566, female/male respectively. NormalIdle is684/688. These are Normal-style leaves: using them with a drawn sword remains the existing equipment-style approximation.

The ignored `rebuild/build/player-strafing-source.json` records the inspected normal nodes2–40, their parameters, thresholds, children, source hash and clip metadata. `rebuild/build/audit-strafe-source.py` generated that local inspection using the UnityPy environment; it is an audit scratch tool, not a required runtime asset or committed exporter.

## Blend graph and facing

`Base Layer.GroundLocomotion.Move` runs at speed1. The Normal branch lies under BeatWeight0 and MoveStyleH/V point(0,0). Its node2 is serialized blendType2 with RightSpeed and ForwardSpeed. The compiled pair/neighbor arrays have not been validated as a native weight algorithm.

| Local speed anchor (right, forward) | Child behavior |
| --- | --- |
| (0,0) | NormalIdle |
| (0,2) | ForwardAngle −25→NormalRunForwardDown, −15→NormalWF |
| (0,5) | ForwardAngle −45→NormalRunForwardDown, −30→NormalRF |
| (−2,0) | FirstPerson0→NormalWalkTurnLeft,1→NormalWL |
| (2,0) | **RightSpeed**0→NormalWalkTurnRight,1→NormalWR |
| (0,−2) | NormalWB |
| (−5,0) | FirstPerson0→NormalRunTurnLeft,1→NormalRL |
| (5,0) | FirstPerson0→NormalRunTurnRight,1→NormalRR |
| (0,−5) | NormalRB |
| (−3.5,3.5), (3.5,3.5) | FirstPerson0→matching RunTurn;1→NormalRFL/RFR |
| (−3.5,−3.5), (3.5,−3.5) | FirstPerson0→matching RunTurn;1→NormalRBL/RBR |
| (−1.42,1.42), (1.42,1.42) | NormalWalkTurnLeft/Right directly |
| (0,8) | NormalSprint |
| (−5.65,5.65), (5.65,5.65) | NormalRunTurnLeft/Right directly |

The right-walk parameter is literally RightSpeed in the inspected graph, unlike left-walk's FirstPerson. Do not silently replace it. There are no separate backward-diagonal walk anchors in this Normal branch.

The additional female/male leaves are NormalRFL736/736, RFR737/737, RBL700/695, RBR704/699, WalkTurnLeft751/751, WalkTurnRight752/752, RunTurnLeft753/753, RunTurnRight754/754, RunForwardDown755/755 and Sprint137/117. The installed runtime contains duplicate NormalSprint names with equal durations; the existing sampler intentionally rejects ambiguous name matches. The known object ID alone does not make a name-only capture safe.

[Action_HumanMove.cs](../../Assets/Scripts/PeEntity/Cmpt/Motion/PEAction/Action_HumanMove.cs) constructs desired movement in world space. When `m_AutoRotate` is enabled, the character rotates toward that direction, projected horizontally on land. It computes local desired speed with `Quaternion.Inverse(trans.rotation) * wantDirInWorld * m_MoveSpeed`. RightSpeed receives localX; ForwardSpeed receives localZ multiplied by `netMoveSpeedScale`. Ordinary parameter updates interpolate with `AnimLerpSpeed * deltaTime`, where AnimLerpSpeed is5; aiming uses the immediate factor1.

[Motion_Move_Human.UpdateMoveSubState](../../Assets/Scripts/PeEntity/Cmpt/Motion/Motion_Move_Human.cs) sets Normal and Sword `m_AutoRotate = !firstPersonCtrl`, apart from competing abnormal states. Rifle, handgun, bow, shotgun, carry, grenade and drill have separate no-auto-rotate handling. [MainPlayerCmpt](../../Assets/Scripts/PeEntity/Cmpt/MainPlayerCmpt.cs) enables first-person control only with the configured first-person flag, outside Build, free-look and Drive. It aligns body facing with camera direction for first-person control or aimed shooting.

Thus ordinary Normal third-person movement rotates the body toward travel and can show turning poses while rotation catches up. Persistent sideways movement with the body locked to camera yaw is a bounded native presentation policy. Neither a captured pointer nor a free pointer alone establishes the source FirstPerson parameter. The native correction below separates body yaw from camera yaw; full source parity still requires the actual turn/blend behavior.

## Source rotation rates and two distinct smoothing stages

Read-only UnityPy decoding of repository `FemaleModel.prefab` finds MoveParam component11431752; `MaleModel.prefab` uses11418252. Both serialize normal move rotation rate6, sprint rotation rate1 and fast-sprint rotation scale75, overriding C# defaults5/5/30. Female prefab SHA256 is `1e518e5af9afd09d89e16a3a2dd776abd42846143ff6b40e409fdf4bb08ae441`; male is `fe19a5d5019c8f2273fe562aa536015ddecb2eae44ba2169d864c4517150ae58`. These are repository-prefab values, not a newly observed live component configuration.

Normal `Action_Move.Update` slerps the body forward vector toward desired horizontal movement by `rotateSpeedScale * m_MoveRotateSpeed * deltaTime`. If the target angle exceeds150 degrees, it first turns toward the body's right vector. On flat ground this corresponds to a clamped fraction of the signed yaw difference, with that special turn direction. A stationary character does not turn toward its idle camera in this ordinary auto-rotate path. First-person/aim control has separate direct camera alignment.

There are two different source smoothing stages. Desired movement direction uses the authored `m_SpeedToLerpF` curve, whose speed/value keys are0/5,3.5/5,5/3,8/2, with serialized tangents. Animator RightSpeed/ForwardSpeed then lerp toward desired body-local movement with factor5×deltaTime (aiming factor1). `DoAction` initializes those Animator parameters immediately. This is parameter smoothing before the authored blend tree, not a fixed-duration pose crossfade. Full source slope compensation, stop IK and sprint handling are not implemented by a normal-heading correction.

## Direction selection and blended presentation

[pe_player_locomotion](../../rebuild/src/pe_player_locomotion.h) retains enum indices0–5: IDLE, WALK, RUN, JUMP, FALL, LAND. It appends WALK_LEFT, WALK_RIGHT, WALK_BACK, RUN_LEFT, RUN_RIGHT, RUN_BACK at6–11. Initialization requires twelve finite positive loaded clip durations, each in[.001,120]seconds, and rejects missing/invalid durations atomically.

The input adds `bool directional` and `float local_right, local_forward`. These components are measured planar velocity in authored body space: +X right and +Z forward. They are ignored when directional is false, preserving existing forward-only callers. When enabled, both must be finite and within±100m/s. World velocity remains authoritative for speed and the .05m/s moving threshold; local components choose direction only.

The target-state selector chooses the dominant local axis. Exact diagonal ties choose forward/back; negative forward selects the back clip, negative right selects left. There is no inferred source diagonal weight algorithm or directional hysteresis. Walking means walk enabled without sprint; otherwise movement selects running. Sprint keeps the existing run fallback. Normalized phase is preserved across all eight walk/run clips. Every walking clip uses actual horizontal speed/2, every running clip speed/5, clamped to.25–2 authored rate. Cyclic clips wrap at their own loaded duration. The legacy selected-state clock retains its transition behavior; rendering now reads the separate continuous pose snapshot described below.

Jump/fall/land timing and accepted jump-edge semantics remain unchanged. Air and idle transitions reset the selected timeline; a moving landing selects the appropriate directional gait. Zero delta is a no-op after validation; invalid input leaves the state unchanged. No root displacement, physics or combat permission is introduced. The added presentation clocks do not dispatch gameplay events.

The selector now exposes `PePlayerLocomotionPose`, twelve normalized weights and authored times. Each step exponentially approaches the selected cardinal state with time constant.12s. This is a native presentation adapter, not the source5×deltaTime parameter lerp or the original two-dimensional blend solver. Reset starts fully at Idle; zero delta is a no-op. All eight gait contributors share a continuously advancing normalized phase driven by the selected/last gait at its actual-speed rate. Rapid reversals therefore cannot restart a still-visible foot cycle. Outgoing idle/air timelines keep advancing or clamp at their authored endpoint; genuinely new Jump/Land actions restart those one-shot timelines while weights remain continuous.

Actual measured world velocity is transformed using the authoritative body yaw in radians:

```c
local_right = vx * cos(body_yaw) - vz * sin(body_yaw);
local_forward = vx * sin(body_yaw) + vz * cos(body_yaw);
```

When body yaw equals `pi - game.yaw`, these reduce to the earlier camera-derived reflected-sandbox formula. Independent free-mode facing must use the current body transform instead. The sandbox now stores transient body yaw outside saved `PeGame`; `game.yaw` still owns camera orbit and camera-relative input. Free third person turns toward actual travel with the repository's normal rate6 and greater-than150-degree right-turn convention. Captured modes align body with camera, and initialization/load resets seed facing from camera. The actual-displacement target, fixed simulation updates and using this normal rate during the existing sprint fallback remain explicit adapters relative to source desired movement and separate sprint logic.

Both hosts use the blended pose for body and all matching held/stowed attachments. Blended joint palettes/points are sampled from compatible source actors with shared weights/times; the source hierarchy/Animator itself is not evaluated. Sandbox Torch light/flame points use the same blended mount and body yaw. Tool and melee actions override locomotion with their own unblended authoritative clock, align facing at action admission, and retain heading precedence. Camera/pointer placement queries keep their camera-derived target ray. Player defence receives the exact named blend and body yaw; rendering must never blend while incoming damage samples only one selected cardinal clip.

## Asset and host integration requirements

At the start of this audit, player body, attachments and defence had no captured Normal side/back set. The capture/export increment must recover all six leaves on the same rig and preserve source durations/loops. Body and every enabled held/stowed attachment must select the same clip and authoritative time. Incoming damage must query the matching posed defence geometry; a new body strafe pose paired with forward defence capsules would disagree with rendering.

The shared selector is integrated into the sandbox actor and tutorial host. Complete directional assets are installed; the physical sandbox evidence below is recorded separately in [PLAYTEST](../PLAYTEST.md). Both sandbox and tutorial free third person now turn into travel, while captured modes use camera-facing direction; active melee, dig and tree actions retain heading precedence. Native pose-weight smoothing is implemented, while original parameter/blend graphs, equipment movement styles, aim/FP layers, source sprint turning and root motion remain explicit gaps.

## Verification

Focused strict C11 optimized and AddressSanitizer/UndefinedBehaviorSanitizer builds both pass, with leak detection enabled and no skipped fixtures. The existing jump/land/ledge and long-session tests remain. New tests cover every added direction, phase continuity across unequal durations and gait changes, exact diagonal boundaries, actual-speed walk/run rates, sprint fallback, blocked movement selecting idle, directional moving landing, default-disabled legacy behavior, zero-delta stability and atomic rejection of invalid directional values or any missing duration. These pure-module checks were conducted without desktop input; the subsequent host physical check is recorded below.

## Integration review

The directional stage exporter validates every body/attachment part against the same ordered clip headers and the defence actor against that matching prefix. Its complete capture additionally retains the four authored walk/run turn leaves, for20 captured body clips and29 defence clips after the nine existing tool-action clips. The cardinal selector still uses twelve locomotion entries; retaining extra turn leaves does not implement their source blend branches.

The sandbox actor converts measured world displacement to authored local axes and validates the twelve body/attachment clocks before upload. Preview selects NormalIdle with one shared body/tool time and leaves the world locomotion state unchanged. Combat preparation now requires all25 runtime-visible defence names, including each new side/back clip; the extra four turn clips are not currently selected. An older19-clip defence group is deliberately rejected before an encounter runs. The exporter supplies exact body/defence clip-header agreement; controller preparation has no body asset argument and does not independently compare cross-asset clocks.

Review identified and corrected tutorial camera-facing locomotion updates overwriting active melee, dig-ray or tree-action heading: the new locomotion-facing block is bypassed while any such action is active. Static checks do not establish original third-person turn parity.

After installation, the focused hunt-controller suite passes optimized and strict ASan/UBSan builds with leak detection enabled and no skipped fixtures. Each of the six individually missing directional clips rejects preparation atomically. Existing source sight/hearing and obstruction checks, actual forearm damage, death cancellation, invalid-input rollback and four real blade-contact deaths in the explicit low-HP test arrangement also pass. The retaliation fixture records two contacts and HP5000→4915.852 after153 fixed ticks. This is headless fixture evidence, not ordinary four-kill gameplay or a physical strafe observation. Logs are ignored under `rebuild/build/hunt-directional-focused.log` and `hunt-directional-sanitize.log`.

## Subsequent native physical observation

The [October6 playtest](../PLAYTEST.md#october-6--directional-player-assets-and-native-strafe-check) exercised A/D/S running and, after Z, A/D/S walking with an existing Knife33 drawn in the free-camera sandbox. Native PID1657905 reported mask4037, covering all six new directional clips plus idle and forward run;20021 frames completed with no attacks or contact events. The [right-running image](../reference/strafe-native-run-right.png) shows the body and attachment clearly. Other backward frames were obscured and are not used as clear pose evidence. The [bounded record](../reference/strafe-native-result.json) separates selected-clip telemetry from still-image observations. Normal menu exit returned0.

The fixture already contained staged hunt completion. No kills, quest progression, moving attacks or directional incoming collisions were demonstrated by this check. Source normal third-person auto-rotation, turn blending, aim/FP layers and exact continuous pose transitions remain separate work.

## Tutorial coordinate review

Tutorial uses an unreflected Raylib camera, unlike sandbox. Its movement side input is D−A and captured body yaw is `theta = pi - camera_yaw`. The implemented inverse body transform is `local_right = vx*cos(theta) - vz*sin(theta)` and `local_forward = vx*sin(theta) + vz*cos(theta)`. Substituting tutorial movement gives local_right=−side and local_forward=forward. Thus a tutorial D press selects the authored left leaf, and A selects the authored right leaf, under the existing mirrored tutorial presentation; sandbox's reflected projection and A−D side basis give the opposite authored-name mapping. Both conversions agree with their rendered body transform. Reversing only the tutorial clip choice would disagree with that transform. This derivation establishes coordinate consistency, not recovery of source handedness across all tutorial rendering/rays. The subsequent physical check below exercises the resulting native side poses.

For tutorial free third person, body yaw converging toward `atan2(vx,vz)` gives local_right approximately0 and local_forward equal to positive horizontal speed once aligned. The original directional increment turned immediately; the current correction uses the normal-rate heading adapter described above. Active melee/dig/tree heading bypasses that locomotion update. The full source desired-movement filtering and Animator turn branches remain unimplemented.

The subsequent15000-frame tutorial building smoke exits0 at tutorial mission756 (gathering ledger752) with all nine NPCs,19 additions/one removal/score50 and eight melee starts/24 contacts/hareHP0. Its locomotion mask63 covers only the original six clips. It is a regression check of the new installed asset set and retained action-facing behavior, not a directional tutorial physical check. Local log: `rebuild/build/strafe-training.log`.

Final full optimized/content/23-CLI verification and full ASan/UBSan with `detect_leaks=1` pass for this directional increment. The sanitizer run exits0 with zero core failures; logs are `rebuild/build/strafe-verification.log` and `strafe-sanitize.log`. Optional skips in ordinary Python suites remain skips, not passed asset checks.

The later captured-camera tutorial physical check (PID1689786) exercised D/A/S run, then Z and D/A/S walk. Its4314-frame log reports mask4033, idle plus all six directional clips, with no melee/mining actions or contacts. [D running](../reference/strafe-tutorial-d-run.png) and [A walking](../reference/strafe-tutorial-a-walk.png) provide clear views; the [result record](../reference/strafe-tutorial-result.json) retains inputs and hashes. This closes the native tutorial directional-input coverage gap. It does not establish source turn/blend/handedness parity; tutorial free-mode movement and captured-mode direction remain distinct policies. Pause→Main menu followed by the main-menu Quit closed and reaped the application with exit0.


## Facing/smoothing correction after user feedback

The prior mask4037/4033 physical checks established that the new directional clips were exercised; they did not establish satisfactory free-mode facing or smooth transitions. User feedback identified the sandbox's camera-locked body and abrupt direction changes. The new implementation separates body heading from camera and applies the shared.12s continuous pose-weight adapter above. These changes do not enable moving attacks or install the staged running-attack group.

Incoming contact now accepts an optional bounded `PeActorBlendSample` array in `PeHuntCombatPlayer` and `PeHareDamageTarget`. A zero count preserves legacy single-clip behavior; nonzero uses the blend and ignores the legacy clip/time. Every entry, including zero-weight entries, validates before any sensor/tick/HP/dedupe mutation. Capsules are reconstructed from four blended posed control points before radius construction; averaging already-built radii would disagree with transformed geometry. An active blade action clears the locomotion blend and selects the exact Attack6–9 defence pose.

Focused optimized and ASan/UBSan tests with leak detection pass for locomotion, low-level blends and incoming damage. Incoming tests demonstrate a true blended hit between two individually missing target poses, atomic invalid-blend rejection, real-route retaliation/death with no usable legacy pose, and identical incoming results during active blade overrides. Its direct sanitizer runner exercises the real-route test in the normally exiting process, enabling the leak scan rather than relying on the usual test wrapper's `_exit` child. The latest correction also passes full optimized build/tests and core ASan/UBSan with `detect_leaks=1`; the ordinary Python run has19 optional skips, not passes. [New physical checks](../PLAYTEST.md#october-6--free-mode-facing-and-smooth-movement-transitions) establish sandbox free-mode A/D/S turning into travel and retaining stopped heading, plus captured sandbox/tutorial transitions inspected in CPU recordings. The [bounded result](../reference/movement-facing-blend-result.json) preserves their inputs and limitations. A separate new15,000-frame tutorial smoke exits0 at mission756 with camera mask3, locomotion mask3647, nine NPCs, five mining swings/three removals,19 construction additions/one removal/score50 and eight melee starts/24 contacts/hareHP0. Logs are `movement-verification.log`, `movement-sanitize.log` and `movement-training-smoke.log` under `rebuild/build`. These checks do not establish Unity blend/turn parity, camera-quality acceptance, exhaustive equipment/action-facing coverage or moving attacks; the earlier physical reports retain their dated scope.


## Subsequent grounded running Knife scope

The [October6 running increment](SANDBOX_RUNNING_KNIFE.md) now installs RunSwordAttack408 in the body/held/stowed Knife group and matching player defence, plus the separately qualified root06 curve. It changes the Adventure sandbox attack policy, not the tutorial locomotion/action policy. Grounded female third-person Knife33 can select running20110014 from actual4.5<speed<=7; initial stage motion uses the same body heading convention and shared swept collision before blade/defence contact. Its exact action pose overrides the12-clip locomotion blend. Attack7–9 continuation motion remains the existing zero-horizontal adapter.

The earlier directional and facing/smoothing observations above retain their dated scope. Original live03 now confirms the running branch and coarse1.37 clip-rate behavior; it does not qualify full Unity motion/transition timing. Current optimized core and full ASan/UBSan with leak detection pass, plus44 focused Python tests with0 skips. Native physical checks verify three running starts and a visible lunge; a separate one-running-start/continuation hunt session records six aggregate outgoing/eight incoming contacts and death, with0kills. Both sessions exit0; contacts are not isolated to408. See [bounded native evidence](../reference/running-knife-native-result.json). Full Animator blending, weapon-specific locomotion, sprint/air/FP attacks and general root-motion parity remain separate work.

The running asset installation initially exposed a tutorial startup mismatch between21 body clips and20 attachment clips. The loader now permits only the trailing body `RunSwordAttack`, retains all shared clock checks and exact action-pair matching, and passes focused optimized/sanitizer and seven installed scene-pair checks. The corrected15,000-frame tutorial regression exits0 at756 with nine NPCs, locomotion mask3647, eight stationary melee starts/24 contacts/hareHP0 and construction score50. This supersedes that failed startup attempt without extending tutorial attack scope. See the [running verification](SANDBOX_RUNNING_KNIFE.md#verification-and-next-checks).
