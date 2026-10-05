# Knife33 collision and recorded attachment geometry

Knife33's source and installed attack geometry have been recovered, and the completed original-runtime capture supplies its actual mounted transform. [export_reference_knife_collision.py](../../rebuild/tools/export_reference_knife_collision.py) exports capsule samples and, optionally, the original rigid knife visual on the same PEAC timeline as the captured player. This is geometry and recorded playback: it neither observes attack-trigger activation nor authorizes contacts, damage or mission progress.

## Source and installed definitions

The item database maps33 to `Prefab/Item/Equip/Weapon/Sword/knife`. Source file `Assets/Resources/Prefab/Item/Equip/Weapon/Sword/knife.prefab` contains equipment component11433344, attack component11422180, and attack transform487040 on `knife/SingleHandSword`.

The installed resource with that same path resolves to GameObject118150 in `PE_Client_Data/resources.assets`; its `SingleHandSword` transform is218508 and PEAttackTrigger is345856. Read-only decoding confirms the capsule, attack form/layers, and broadphase box match the source. The installed runtime trigger occupies80 bytes:32-byte player MonoBehaviour base followed by mask/count and one serialized part. Source-only editor fields such as `TestHit` are absent. The exporter checks the full size, actual MonoScript class, referenced transform and all recovered geometry before accepting this layout.

| Property | Source and installed value |
| --- | --- |
| Custom attack parts | One |
| Attack form | 0, SharpMetal |
| Capsule radius | .30000001192092896m |
| Serialized `heigh` | 1.5m |
| Offset | `(0,0,−.10000000149011612)` |
| Axis | 2, positive local Z |
| Referenced transform | SingleHandSword |
| Attack mask | 34154496, bits11/13/16/19/25 |
| Source mask names | SceneStatic / Building / Unwalkable / GIEProductLayer / Damage |

`PECapsuleTrigger.ResetInfo` defines endpoints as `offset + radius×axis` and `offset + (heigh−radius)×axis`. The sphere centers are therefore approximately **`(0,0,.2)` and `(0,0,1.1)`**, with radius.3. The capsule is not centered on the weapon origin. Its contact volume intentionally extends well beyond the visible knife mesh; shortening it to the rendered blade would alter the source collision rules.

The separate source BoxCollider6583062 is a **broadphase trigger** on SingleHandSword: size `(.600000024,.600000024,2.5)`, center `(0,0,1)`, initially disabled. It gathers candidate colliders; it is not the final capsule contact test. `PEAttackTrigger.active` enables/disables this collider and updates capsule positions. Its Start initializes parts and sets active false.

## Attachment transform chain

`PEHoldAbleEquipment.InitEquipment` attaches the item to its serialized stowed bone, **mountWaist**. Drawing moves it to **mountMain**. The chain is `Action_Tool → BiologyViewCmpt.Reattach → BoneCollector.SwitchBone`; `SetParent(parent,false)` preserves local TRS while changing parents. The knife root and SingleHandSword child have identity local position/rotation/scale in the source and observed capture. The source names AxPutOn/AxPutOff are retained; a knife-specific animation name must not be invented.

The animated mount is under:

```text
Model/Bip01/Bip01 Pelvis/Bip01 Spine1/Bip01 Spine2/Bip01 Spine3/
Bip01 R Clavicle/Bip01 R UpperArm/Bip01 R Forearm/Bip01 R Hand/mountMain
```

Both player prefabs also contain ragdoll mounts. The exporter preserves only mount chains under the animated `Model`, and captured selection requires a direct chain ending `/mountMain/knife/SingleHandSword` or `/mountWaist/knife/SingleHandSword` (also allowing the explicit Unity `knife(Clone)` name). Multiple candidates, missing weapons, or changing model/weapon instance identity reject the capture rather than choosing an arbitrary transform.

| Animated mountMain | Local position relative to right hand | Local quaternion x,y,z,w |
| --- | --- | --- |
| Male, transform405738 | `(-.066296078,.030821456,-.010829544)` | `(.000149619,-.004830000,.003023380,.999983788)` |
| Female, transform400208 | `(-.052396659,.033852614,.000451355)` | `(.033969317,-.060731765,.000281908,.997575879)` |

The output retains full chains and scales; these offsets should not be multiplied into a recorded **weapon world matrix a second time**. When only a future animated hand pose is available, the hand→mount→knife→SingleHandSword local matrices must all be composed. Direct captured matrices already include them.

For each recorded frame:

```text
capsule_model = inverse(captured modelRoot_world) × captured SingleHandSword_world
knife_visual_model = inverse(captured modelRoot_world) × captured knife_root_world
endpoint_world = SingleHandSword_world × source_local_endpoint
endpoint_model = capsule_model × source_local_endpoint
```

Source radius is `abs(radius×SingleHandSword.lossyScale.x)`, always local X, even though the capsule axis is Z. The exporter derives X scale from the orthogonal recorded matrix basis and rejects shear above1e−4. It requires a uniform model-root scale within relative1e−4 before representing the world sphere as a scalar-radius model-space capsule; otherwise inverse-root conversion would create an ellipsoid. It stores both model-space and world-space endpoints/radii and the root matrix. Local-X radius must not be replaced by Z scale or a largest-axis heuristic.

## What the capture establishes

The root agent completed `knife-reference-01.jsonl` under `~/.local/share/planet-explorers-pose-reference/pose-captures/`, with a clean stop record. Inspection began only after completion. The566 end-of-frame samples use FemaleAvatar and model path `EntityRoot/Tutorial_20000/FemaleModel(Clone)/Model`. The same knife transform appears in all frames: **27 under mountWaist,539 under mountMain**. Its maximum normalized basis shear is approximately1.12e−6, and recovered world radius ranges from .299999972 to .300000247m, consistent with floating-point scale variation around.3.

The probe captures complete model-descendant transforms, including the item, and Animator states, clip weights and clocks. It does **not** capture PEAttackTrigger.active, BoxCollider.enabled, the action's fixed firstPersonAttack flag, or actual delivered animation-event callbacks. `Transform.activeInHierarchy=true` only establishes that the item is present in the active hierarchy; a stowed knife is also active. Accordingly every exported sample has `attack_active:null`, and the recording declares `attack_activation_observed=false` and `collision_authorized=false`.

The source activation path is `Action_Sword.OnAnimEvent`: StartAttack selects `SingleHandSword`, enables its trigger and skips the skill's pre-wait. EndAttack starts a coroutine that delays deactivation until at least two engine frames after activation, then resets trigger hit information and skips the skill main wait. TP/FP-prefixed callbacks are filtered against the action's firstPersonAttack flag; the opposite view must not contribute another hit window. Animator blending, state transitions and this minimum-frame delay prevent treating recorded wall time or a visible arm swing as proof that a trigger was active. The exact authored event schedules and headless event module are documented in [player animation](PLAYER_ANIMATION.md) and [training combat](TRAINING_COMBAT.md).

## Contact filtering still required

`PEAttackTrigger.OnTriggerEnter` first checks its attack mask and rejects missing/self SkEntity candidates. Enabled defender parts then undergo the custom capsule check. `PEUtil.CanDamage` supplies damage permission; attacker death also suppresses processing. Trigger-level hit/effected lists and skill-guide entity deduplication serve different purposes and must both be respected.

The source loops attack parts, then defence parts, selecting a new `mostCross` only when its `distance` is larger. Capsule/capsule collision currently sets **distance=0 for every successful contact**, so ties keep the **first overlapping defence part in serialized order**. For the recovered hare that order begins with Head1, damageScale2. A nearest-part or deepest-penetration selector would not reproduce this code path.

The source also uses previous/current endpoints and parent-center arc correction to sample swept motion. Its geometric test is strict overlap, excluding exact tangency. The existing native [hitbox module](HITBOX_RECOVERY.md) supports instantaneous capsule overlap but not that history sweep. In particular, the original sweep uses the previous sample when its computed subdivision count is one; replacing that with only the current capsule changes temporal semantics. End-of-frame pose samples are not a record of every internal trigger update. These gaps must be resolved or explicitly scoped before claiming matching melee hits.

No new runtime attack, collider, target-damage or kill-completion hook is added by this exporter.

## Export commands and recorded visual contract

Generate the geometry record and verify the installed capsule:

```sh
/tmp/pe-asset-tools/bin/python rebuild/tools/export_reference_knife_collision.py \
  --installed-game '/home/admin/.local/share/Steam/steamapps/common/Planet Explorers' \
  --capture '/home/admin/.local/share/planet-explorers-pose-reference/pose-captures/knife-reference-01.jsonl'
```

Default output is ignored `rebuild/build/reference-assets/knife-collision.json`. The source section contains capsule and broadphase fields, attachment/mount chains, source hashes and layer names. The recording stores sample/frame IDs, Unity and real-time clocks, attachment category, observed transform activity, unknown attack activity, world/model geometry and transform matrices. The parser bounds input bytes, line size, frame count, geometry and hierarchy, requires monotonic clocks and a completed capture, and refuses output over the input or within an explicitly supplied game installation.

To export the rigid knife on the same sample interval as the third-person body recording, first obtain the ordinary `training-knife` asset export, then run:

```sh
/tmp/pe-asset-tools/bin/python rebuild/tools/export_reference_knife_collision.py \
  --capture '/home/admin/.local/share/planet-explorers-pose-reference/pose-captures/knife-reference-01.jsonl' \
  --scene-dir rebuild/build/pose-analysis/knife-tp-attack6 \
  --first-sample 48 --last-sample 68 --fps 30
```

The exporter checks the recovered OBJ and diffuse files against their asset manifest. A strict bounded OBJ reader preserves vertices, normals, UVs, triangle winding and separate material slots. There are two PEAC parts:84 vertices/276 indices and67 vertices/183 indices, each with one rigid joint, identity inverse bind and a `captured` nonlooping clip. Their palette is the recorded knife-root matrix in model space. UVs retain the same native PEAC convention as the player; the renderer applies its usual V flip.

`knife-recording.peas` contains the two actor/texture rows; `knife-recording-manifest.json` records hashes, selected samples, timebase and limitations. To produce a combined scene reproducibly, add `--body-scene rebuild/build/pose-analysis/female-starter-tp-attack6/recording.peas` and use a **fresh** `--scene-dir`. Every copied body actor must have exactly the same binary clip name, frame count, FPS, duration and loop flag as the knife. The exporter bounds paths/files/total bytes, rejects duplicate actors, filename collisions and path traversal, snapshots source bytes, and writes `recording.peas` with body rows followed by both knife rows. It never modifies the body directory. Matching clip metadata establishes a shared clock, not proof that an arbitrary supplied body came from the same capture; the known body command must use samples48–68 from the same recording.

The actual reproducible combined output is `rebuild/build/pose-analysis/female-starter-knife-tp-attack6-exported/recording.peas`, containing11 parts. Updated v2 exports use the separate `female-starter-knife-tp-attack6-v2`, `female-starter-idle-v2` and `knife-idle-v2` directories; existing active directories were not overwritten. Root independently rendered the earlier manually assembled equivalent11-part scene for38 frames and visually confirmed that the knife is properly gripped by the right hand at playback .583s, with no obvious mount offset (`/tmp/pe-clothed-knife.png`). This verifies recorded visual alignment, not trigger activation.

For the captured stowed pose, use `--scene-dir rebuild/build/pose-analysis/knife-idle --first-sample 0 --last-sample 20 --fps 30`. This yields two parts,57 frames, duration1.86535001s after f32 serialization. The source interval remains mounted at the waist. It supports drawing time0 in the optional training view; it is not a general put-on/put-off animation controller.

For samples48–68 at30Hz, both knife parts have **48 frames and duration1.545169999999871s** before f32 serialization (native duration1.54516995). This matches `analyze_reference_pose.py`: take recorded realtime relative to the first selected sample, resample componentwise affine matrices, and clamp the terminal guard frame to the last pose. It deliberately preserves the body recording's clock, rather than substituting an authored Attack6 clock. Resampling sparse captured poses does not reconstruct unseen original motion or generalized humanoid retargeting. The original diffuse-only visual bundle and its manifest remain the visual provenance; original shaders are not reproduced.

## Native collision fixture

Visual export also writes `knife-recording-0.pehb`, anchored to the exact geometry/skin/index/inverse-bind fingerprint of `knife-recording-0.peac`. It contains one `SingleHandSword` record: kindATTACK, groupRIGHT=1, joint0, source form0. Material0 and damageScale1 are unused attack-record placeholders, not newly recovered defence metadata. The four source capsule points are transformed from SingleHandSword-local into knife-root bind space. Export rejects intervals where this relative transform varies by more than1e−5; the actual recording passes.

The existing [PEHB loader and pose API](HITBOX_RECOVERY.md) can load this pair and call `pe_hitbox_pose(boxes,actor,0,0,seconds,modelRootPosition,yaw,uniformScale,&capsule)`. The same sampled joint0 palette drives the original knife visual and all four capsule points, including the radius vector. No new trajectory format or second motion clock is required. Geometry comes from the recorded female model-root coordinate frame; placement must use that same model root, rather than adding the hand mount a second time. The fixture does not encode activation windows and cannot itself permit damage.

The original v1 recording exposed a terminal guard mismatch: uniform `time*fps` timestamps stopped approximately0.000564m short of this recorded final knife pose. New recorded exports use **PEAC v2**, explicitly timestamping the final palette at duration while retaining uniform interior samples. Native vertex and joint-point interpolation use the shortened final interval, and nonlooping playback reaches the exact terminal palette. V1 actors—including original hare exports—retain their previous uniform sampling behavior. Scene composition rejects mixed v1/v2 timing. The version is excluded from the geometry/skeleton fingerprint, so identical bind geometry keeps its PEHB anchor. This fix does not reconstruct unseen motion between sparse capture samples or establish trigger activation.

## Validation

```sh
python3 rebuild/tests/test_reference_knife_collision.py
```

Seven headless tests pass: noncentral capsule endpoints and local-X scale, uniform-root conversion, rejection of nonfinite/singular/sheared/nonuniform-root inputs, captured geometry with explicitly unknown activation, missing/duplicate weapon and incomplete/nonmonotonic capture rejection, strict OBJ indices/material splitting, one-joint PEAC matrix/timing layout, exact-geometry PEHB fingerprinting and rigid-relative-transform checks, combined-scene snapshot preservation, and mismatched-clock/path rejection. Actual extraction completed for all566 poses. Installed/source capsule and box comparison passed. The native headless scene loader accepted the11-part combined attack scene and two-part stowed scene. The native PEHB loader accepted the exact paired knife actor; all48 posed capsule frames were checked against independently resampled captured blade transforms, first revealing the v1 terminal discrepancy and then verifying the explicit v2 fix described above. No graphics were launched by this audit agent. Root's combined visual alignment check passed as described above; it provides no evidence of active contacts or successful hits.
