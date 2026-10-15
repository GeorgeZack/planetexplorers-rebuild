# Shield layer capture

**Status.**

- **Observed:** the original's masked shield composition.
- **Reproduced:** three callback-free clone replays match its states, independent clocks, layer weights, clip contributions and parameters exactly. Bone positions still differ.
- **Not done:** no masked runtime loop or native block animation is qualified or installed, and a shield block clip sampled alone is no substitute.

Related: [starter pair](STARTER_SWORD_SHIELD.md), [Sword locomotion](TORCH_SWORD_RECOVERY.md), [isolated sampler](ANIMATOR_BAKE.md), [pose observer](../../rebuild/tools/REFERENCE_POSE.md).

## Source contract

`inventory_shield_layers.py` reads the installed `sharedassets1.assets` into the ignored `rebuild/build/shield-layer/plan.json`. The plan holds the asset SHA256, the controller779 graph, clips, events and behaviour references, and is marked not capture-ready and not export-ready. Its tests reject a full-body mask, a missing layer weight, Normal block standing in for Sword block, fixed-second transitions, changed MoveStyle thresholds and the wrong draw clip.

- **Mask layer.** `FemaleAnimator2` 779, layer 3 `LArmOnly`: override mode 0, default weight 1, body-mask words `(0,41947128,1)`, empty transform mask, no IK pass, no synced timing. The words are kept as source data, not turned into a bone list.
- **Block state.** `LArmOnly.SheildBlock` is a 1D `MoveStyle` blend: 0 → `NormalSheildBlock` 420, 1 → `SwordSheildBlock` 413 (1.6666717529 s, looping). State speed is 1 and neither clip has events. The transition from DoNothing requires `SheildBlock=true`, and release requires false. Durations 0.09999996423721313 and 0.10209385305643082 are normalized, with no exit time.
- **Base leaves.** `AttackIdle` 90, `SwordWF` 133, `SwordRF` 435. A valid frame needs the base and block layers in the same Animator on the same frame; spliced bones or a full-body 413 do not count.
- **Draw and stow.** `UpbodyOnly.SwordPutOn` and `SwordPutOff` use clips 270 and 275 at `InWater=0`, speed 1.2000000476837158, with `PutOnEquipment` at 0.16318383812904358 s and `PutOffEquipment` at 0.2606496214866638 s.
- **Action.** `Action_SheildHold` sets bool `SheildBlock`, mask `HoldShield`, attribute 35 `EnableShieldBlock=1` and buff 30200177. Without EquipmentHold it requests Sword movement style; ending clears all four. It needs a shield, costs no stamina, cannot run alongside SwordAttack or Sprint, and ends on attack. Default input is held LeftControl.

## Original observations

Observer runs used `reference_pose.py` and the strict analyzer `analyze_shield_pose.py`. A steady gait-plus-block frame requires all of:

- block state 439445811, with the Sword block contributing at least 0.9999;
- layer weight 1 and MoveStyle 1, each within 0.0001;
- `SheildBlock` true, no transition, a matching base gait, and no other clip-bearing override.

Mixed gaits stay mixed, and reports always keep `runtime_pose_export_ready=false`.

- **Live01 and live02** crashed before arming, with the known NVIDIA glcore signature (`mov (%rsi),%edi`, `rsi=0xc`, a `/memfd:/.glXXXXXX` mapping). Live02 used `-force-gfx-direct`, so single-threaded rendering does not prevent it. [Crash comparison](../reference/shield-observer-crash-20261006.json).
- **Live03** used Mesa llvmpipe and gave 187 frames over 119.424317 s, none of them usable: `FirstPerson=1`, `SheildBlock=false`, and `LArmOnly` stayed in DoNothing throughout. [Report](../reference/shield-observer-software-20261006.json).
- **User-operated Zink** ([report](../reference/shield-observer-zink-20261006.json)): 3,600 frames over 74.367614 s, SHA256 `25f1965e54ca2ebcb56479f1b2f72a68762736dc3f2b50c60573b269b340dc17`, on `zink Vulkan 1.4 (Quadro K1200)`.
  - 732 frames have `SheildBlock=true` and 451 are steady Sword block, all in third person.
  - The shield sits under animated `Bone L Forearm`.
  - Excluding five frames with another active override leaves 446, whose strongest base contributions are AttackIdle 156, SwordRF 138, SwordWF 80, turns 68 and SwordWB 4.
  - No 0.9999-pure gait-plus-block frame exists.
  - Quit returned 137. All 14 protected save files were unchanged.

| Base | Sample | Weight | Parameters |
|---|---:|---:|---|
| AttackIdle | 1611 | 0.999358356 | MoveStyle 0.999995649 |
| SwordWF | 2157 | 0.9700903 | ForwardSpeed 2.03722119, RightSpeed 0.00327215833 |
| SwordRF | 1686 | 0.9877594 | ForwardSpeed 4.966685, RightSpeed −0.00156909623 |

`Motion_Move_Human.UpdateAnimState` damps MoveStyle and MoveStyleH/V with `Lerp(…, 5*Time.deltaTime)` toward (cos 20°, sin 20°). The live point (0.939692557, 0.3420201) matches that target rather than the authored (0.94, 0.342), so small neighbouring contributions are expected. Unity's blendType 2 weighting has not been reconstructed.

Attack6 → 7 → 8 and three further Attack6 episodes appear on FullBody, with the block bool false at each entry. Block returns at samples 2828, 2890 and 2971. Seventeen frames show an attack tail with the block bool true; this does not prove concurrent actions, because PEAction state and held keys were not recorded.

## Callback-free clone composition

The installed Unity 5.2 metadata ([inspector](../../rebuild/tools/inspect_unity_animation_api.c)) has `AnimationMixerPlayable` but no `AvatarMask`, `AnimationLayerMixerPlayable`, `HumanPoseHandler` or masked-layer setter. Attaching the original controller to a clone would run its 140 state-machine behaviours, and disabling `fireEvents` does not stop them.

- **Bundle.** [`export_shield_controller.py`](../../rebuild/tools/export_shield_controller.py) builds a separate bundle of controller779 and its 452 unique clips. It removes all 140 behaviours and both range arrays before any load and strips 372 clip events, leaving 454 objects. External or unexpected references fail closed. Clip bytes before the event vector, and the graph bytes, are preserved; NaN-bearing blend tables are compared by exact bits. `--check` rederives the export.
- **IK-disabled variant.** `--disable-ik` changes only graph offsets 40 and 48232 from 1 to 0 (Base Layer `IKPass` and Base Move `IKOnFeet`). It is a clone adapter, not live IK. The no-IK bundle is 8,140,097 bytes, SHA256 `4cbfd1e4929c6218b2357225334e08de789e93c24d50abb091aee8e63c138c91`.
- **Clone.** [`ReferenceShieldComposition.cs`](../../rebuild/tools/ReferenceShieldComposition.cs) builds new Transform and Animator objects only, borrows the female Avatar, and loads only the hash-bound bundle. Events and root motion are off and speed is 0. Components are checked before and after evaluation. Everything is retained until process exit.
- **Replay.** [Launcher](../../rebuild/tools/reference_shield_composition.py) and [probe](../../rebuild/tools/ReferenceShieldCompositionProbe.cs) replay samples 1611, 1686 and 2157: every non-trigger parameter plus the independent Base Move and LArmOnly clocks, with no rounding.

Runtime attempts, each in a fresh female tutorial ([record](../reference/shield-composition-smoke-20261006.json)). Every attempt passed the 120-update cleanup check and quit with 137.

| Attempt | Result |
|---|---|
| smoke03 | The guard rejected the `LoadAllAssets` count; 0 poses. |
| smoke04 | Count 1, the named controller, 608 references to 452 event-free clips. A nonfinite value was rejected before the first frame. |
| smoke05 | Localized: `Base Layer state=-1436569151.length=Infinity`, Unity's effective duration at Animator speed 0. |
| smoke06 | Positive Infinity is allowed only at speed exactly 0 (recorded as `frozen-effective-duration`). All 3 samples recorded. |

**smoke06 match.** States, clocks, layer weights, every clip contribution and every parameter match with zero error, and no unexpected layers were active. This is masked evaluation with independent base and block phases.

| Item | SHA256 |
|---|---|
| Clone capture | `632847800fb07cb94a10291f94611a529bbdd1527896065acece0d3957d4da3c` |
| Replay plan | `d400ae16aa8303f01a4df5e9c11f7b897998b7d527d96b5c210c7d4e7584d4fa` |
| Comparison | `d42946942752ccfa22606b9bc63bc57c194116ccb51957b78d5ff3868d53d9e8` |

Also: [regions](../reference/shield-composition-regions-20261006.json), [responsive menu](../reference/shield-composition-responsive.png).

Model-space maxima ([`analyze_shield_composition_regions.py`](../../rebuild/tools/analyze_shield_composition_regions.py)):

| Region | Idle 1611 | Run 1686 | Walk 2157 |
|---|---:|---:|---:|
| Shield arm | 0.870 mm / 0.148° | 16.900 mm / 3.299° | 4.657 mm / 0.985° |
| Shield forearm | 0.800 mm / 0.148° | 9.718 mm / 2.148° | 2.497 mm / 0.574° |
| Shield hand | 0.292 mm / 0.148° | 2.994 mm / 2.148° | 0.997 mm / 0.574° |
| Legs and feet | 47.235 mm / 6.952° | 57.691 mm / 8.715° | 9.117 mm / 4.150° |
| Neck and head | 1.050 mm / 0.128° | 28.860 mm / 3.299° | 7.785 mm / 0.985° |

What the residuals show:

- The head and left-hand local matrices are exactly equal, so their model-space error is inherited from ancestors.
- Local differences: spine 0.110° / 3.004° / 0.993°, leg bend up to 13.022° / 16.964° / 4.118°, left forearm 0.039° / 3.868° / 1.262°. A single whole-body correction is therefore wrong.
- Live IK and postprocessing are a candidate cause, not a proven one. Source [`IKCmpt`](../../Assets/Scripts/PeEntity/Cmpt/IKCmpt.cs) and the grounder updates in `Motion_Move_Human` act on these regions, but no paired IK-on/off clone was run.
- Proportions: local scale differences are below 4.9e−7 and local translation is within 0.068 / 1.120 / 0.069 mm. No global scale fix is supported.

## Prepared, not run

[`reference_shield_qualification.py`](../../rebuild/tools/reference_shield_qualification.py) with [`ReferenceShieldQualificationProbe.cs`](../../rebuild/tools/ReferenceShieldQualificationProbe.cs) and [`ShieldQualificationClone.cs`](../../rebuild/tools/ShieldQualificationClone.cs), prepared in `build/shield-qualification/repeatability-01`.

- **Repeatability.** Two fresh clones, each with a hierarchy snapshot taken before any Animator exists. Eight evaluations: clone 0 in the order idle, run, walk, idle; clone 1 in the order walk, idle, run, walk.
- **Optional source-IK variant.** The same bundle with only offsets 40 and 48232 restored, so it can be compared with the no-IK one. It has not been evaluated.
- **Grid.** 48 regular, 27 seam and 12 withheld points across the three configurations. It is gated on reanalyzing a completed repeatability capture, and requires exactly repeated poses.

Remaining before any installation: interpolation, phase coverage, IK causality, block entry and release, and attacks.
