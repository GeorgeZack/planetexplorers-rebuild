# Humanoid channel and Avatar index mapping

October6, 2026. Read-only installed-binary, serialized-asset and existing-capture evidence. No game launch, engine initialization, desktop input or source/save mutation was used for this mapping. It identifies the127 scalar channels in the inspected player clips; it does not establish complete retargeting, shield composition or root/goal solving.

## Primary inputs

The inspected Linux executable is `PE_Client.x86_64`, SHA256 `1eb5ad6ff19dddb960a71453603ed7c67403403186c9dfeadc798a6021c4d2a6`, ELF Build ID `651e01fe3d5be6ff003977729e62e91ebd3390e6`. Addresses below are virtual addresses in this exact stripped binary. Inspected text/read-only-data offsets equal virtual address minus `0x400000`; BSS addresses identify initialized runtime tables and are not file offsets. Static inspection used `objdump -d` and byte/table reads, without executing the initializer.

Installed `PE_Client_Data/sharedassets1.assets` has SHA256 `d9c65f820a2186116d9eb73d815fe07d9f46924ddfd103ee9510bf254f9a3d67`. Female Avatar777 and the actual player clips come from this file. The existing exporter records full input provenance in ignored `rebuild/build/reference-player/player-manifest.json` and `rebuild/build/reference-player-starter/player-manifest.json`.

Reusable local inputs, already present before this audit:

| Input | SHA256 / purpose |
|---|---|
| `rebuild/build/reference-player-starter/player-female-starter-rig.json` | `58f2e013f5c5dc3724ea3e22c41f4ea425f457c3530d3568502452e33d378bff`; full Avatar, skeletons, axes, limits, skin and inverse-bind data |
| `rebuild/build/reference-player-starter/player-female-starter-attack6-channels.json` | `f81ea5a5455557618b74379a5d2c44aaa2e6181f4fea7a3af8f393c3bc0024c1`; decoded scalar bindings and samples |
| `rebuild/build/pose-analysis/female-starter-tp-attack6-v2.json` | `71cd28df8742b63348c46bc8a231c605e7ee2c89ae4a0507fa4700fa944877df`; captured `human_traits`, including92 named muscles and54 bone-to-muscle-axis records |

The traits originate in preserved `~/.local/share/planet-explorers-pose-reference/pose-captures/knife-reference-01.jsonl`, SHA256 `dc94b9c0ae1c2dffc99bffba2681c68b6ecf011c147bb8c3757d2de4bd3ba40e`. [ReferencePoseProbe.WriteHumanTraits](../../rebuild/tools/ReferencePoseProbe.cs#L175) read the original `HumanTrait.MuscleName`, default limits, `BoneName` and `MuscleFromBone(bone,axis)` APIs. These are observed engine metadata, not labels inferred from bone names.

## Attribute mapping

The inspected clips' humanoid bindings have `classID=95`, `customType=8`, `path=0`, width1, and exactly the attribute set7..133. Attributes identify channels; their position in the serialized binding vector is not the muscle index.

| Attributes | Property |
|---|---|
|7..9|`RootT.x/y/z`|
|10..13|`RootQ.x/y/z/w`|
|14..16 /17..20|`LeftFootT.xyz` / `LeftFootQ.xyzw`|
|21..23 /24..27|`RightFootT.xyz` / `RightFootQ.xyzw`|
|28..30 /31..34|`LeftHandT.xyz` / `LeftHandQ.xyzw`|
|35..37 /38..41|`RightHandT.xyz` / `RightHandQ.xyzw`|
|42..93|52 body muscles, captured HumanTrait indices0..51|
|94..113|20 left-finger muscles, captured indices52..71|
|114..133|20 right-finger muscles, captured indices72..91|

For all92 muscles, **attribute =42 + HumanTrait muscle index**. Finger order is Thumb, Index, Middle, Ring, Little; within each finger it is `1 Stretched`, `Spread`, `2 Stretched`, `3 Stretched`. The registry's finger property spelling uses `LeftHand`/`RightHand` prefixes, while HumanTrait display names use spaced words. Their indices establish the join; comparing display strings alone is unnecessary.

The same native registry contains `MotionT.xyz`/`MotionQ.xyzw` at0..6. Those attributes are absent from these selected127-channel clips. Their absence must not be filled by copying RootT/Q or interpreted as proof that the clip has no evaluated root motion. Additional registry properties outside133 are outside this bounded mapping.

Native proof chain:

- Initializer `0xc1adc0` constructs128-byte property-name slots at BSS `0x1d682c0`. Initial stores construct Motion0..6 and Root7..13; the RootT.x store at `0xc1b24d` addresses `0x1d68640`, exactly base+7×128.
- Goal loop `0xc1b836` starts at attribute14 and advances seven slots until42. The table at `0x16fedc0`, stride16 bytes, holds `(5,1,3,5)`, `(6,2,4,6)`, `(17,13,15,17)`, `(18,14,16,18)`. Each first integer is passed to name getter `0xc35510`, whose pointer table `0x16ffbc0` names bones5/6/17/18 LeftFoot/RightFoot/LeftHand/RightHand. The initializer appends T.xyz and Q.xyzw.
- Body-muscle loop `0xc1be30` sets destination index42+muscle and copies52 names from getter `0xc35520`, pointer table `0x16ffc80`.
- Finger construction starts at94, increments four slots per finger and stops the left loop at114 (`0xc1c590`); the right loop stops at134 (`0xc1ccc2`). Getter `0xc33490`, table `0x16ff8a0`, supplies the five finger names. Getter `0xc334b0`, table `0x16ff900`, supplies the four suffixes in the order above.
- Registry setup `0xc1df00` associates each slot index with its name CRC; lookup `0xc1e040` addresses the same128-byte name slots. The table is not a guessed ordering based on the127-channel count.

Representative inspection:

```sh
objdump -d --start-address=0xc1b836 --stop-address=0xc1be75 BINARY
objdump -d --start-address=0xc1df00 --stop-address=0xc1e050 BINARY
objdump -d --start-address=0xc33490 --stop-address=0xc334bb BINARY
```

## Packed data and limits

[packed_channels](../../rebuild/tools/export_reference_player.py#L94) already decodes the selected dense/constant layouts. It labels these channels `humanoid_scalar_unretargeted`; ordinary Transform bindings remain separate. No new inventory tool is needed.

| Installed clip | Dense scalars | Constant scalars | Direct Transform bindings | Humanoid bindings |
|---|---:|---:|---:|---:|
|SwordSheildBlock413|114|116|31|127|
|NormalSheildBlock420|114|116|31|127|
|AttackIdle90|119|111|31|127|
|SwordWF133|118|109|30|127|
|SwordRF435|90|37|0|127|

All five have zero streamed curves. Counts of Transform *bindings* differ from scalar counts because a position/scale binding has three components and a quaternion has four. Dense and constant scalars occupy separate packed ranges. For example Attack6's constant attributes56/57 and several finger attributes appear after its animated bindings; indexing muscles directly by binding position would select the wrong data.

**Do not clamp normalized muscle samples to[-1,1].** The preserved Attack6 channels contain attribute127 approximately−4.07887. Negative values and excursions beyond nominal normalized limits are actual source data. The arithmetic helper's explicit native work/input bounds are separate from a gameplay clamp. Captured HumanTrait default limits are degrees; serialized Avatar axes limits are radians. Use the actual Avatar's limits for its joint calculation, not a second conversion of those stored values or a blanket replacement with HumanTrait defaults.

## Distinct index spaces

The female rig contains77 Avatar skeleton nodes,44 human skeleton nodes,41 axes records,24 entries in `m_HumanBoneIndex`, and separate15-entry hand maps. `m_HasTDoF=false` for this input. None of those counts equals the127 scalar-channel count.

A correct join preserves each namespace:

1. The captured HumanTrait bone index supplies three muscle indices in **axis order X,Y,Z**; `-1` means no muscle on that axis.
2. For the first24 body bones, `m_HumanBoneIndex[trait_index]` gives a **human skeleton index**. Finger bones instead use `m_LeftHand/RightHand.data.m_HandBoneIndex`; do not index past the24-entry body array.
3. `m_HumanSkeletonIndexArray[human_index]` gives an **Avatar skeleton index**. Validate its inverse `m_HumanSkeletonReverseIndexArray[avatar_index]`.
4. `m_AvatarSkeleton.data.m_ID[avatar_index]` is a path hash; resolve it through Avatar `m_TOS`. `m_SkeletonNameIDArray` is a separate name-hash array and must not substitute for full-path identity.
5. Join the exact relative path to the capture's `skeleton.nodes`. The capture's `humanBones` values are **indices into that capture's nodes**, not HumanTrait indices or Transform instance IDs.
6. `human.m_Skeleton.data.m_Node[human_index].m_AxesId` selects `m_AxesArray`. `m_PreQ`, `m_PostQ`, `m_Sgn`, limits and axis type belong to that record. Keep signed local scale from the appropriate Avatar pose distinct from rotation.

Examples from Avatar777:

| HumanTrait bone | Muscle indices X/Y/Z | Attributes X/Y/Z | Human index | Avatar index | Axes index |
|---|---|---|---:|---:|---:|
|Spine|2,1,0|44,43,42|12|15|9|
|Chest|5,4,3|47,46,45|13|16|10|
|LeftShoulder|−1,35,34|−1,77,76|31|40|28|
|LeftUpperArm|38,37,36|80,79,78|32|41|29|
|LeftLowerArm|40,−1,39|82,−1,81|33|42|30|
|LeftHand|−1,42,41|−1,84,83|34|43|31|

For this rig the left hand map is `[35,36,37,38,39,40,-1,-1,-1,41,42,43,-1,-1,-1]`; the right is `[22,23,24,25,26,27,-1,-1,-1,28,29,30,-1,-1,-1]`. Missing middle/little fingers remain missing; their channel values do not authorize invented bones. The mapped left forearm path ends `Bip01 Spine3/Bip01 L Clavicle/Bip01 L UpperArm/Bip01 L Forearm`; its animated shield mount remains a separate descendant binding.

## Existing pose comparisons and remaining proof

Closed unlayered bakes already provide independent evaluated transforms, without another launch:

- `~/.local/share/planet-explorers-bake-reference/pose-captures/female-clips-06.jsonl`:345 poses across Normal locomotion/jump/fall/land and Attack6–9; exported under `rebuild/build/animator-bake/player-female-06/all`.
- `female-sword-01.jsonl` in that directory:304 poses across ten Sword leaves; exported under `rebuild/build/sword-body-01/all`.
- `female-sword-backward-02.jsonl`:48 poses across SwordWB/RB; exported under `rebuild/build/sword-backward-body-02/assets/all`.

Their manifests bind the same starter rig hash above and preserve capture/launch hashes. [The strict bake analyzer](../../rebuild/tools/analyze_reference_bake.py) verifies closure, hierarchy, clocks and source identities. A new comparison must also link current installed asset hashes to the capture's launch plan; a matching clip name and length alone are insufficient provenance.

The separately recorded [272,217-case native arithmetic qualification](../reference/unity5-human-rotation-arithmetic.json) concerns `GetZYRoll` only. It does not establish that `preQ * GetZYRoll(muscles) * inverse(postQ)` alone reproduces every joint. [probe_humanoid_rotation.py](../../rebuild/tools/probe_humanoid_rotation.py) compares that candidate's local3×3 bases against existing bakes, retaining general-retargeting and shield-composition flags as false. Its diagnostic dense-channel interpolation, candidate pre/post composition, signed scale, and omitted root/goal/limb solving are separate qualifications from the helper's arithmetic replay.

The first retained diagnostic, `rebuild/build/humanoid-proof/bake06-comparison.json`, compares345 poses and40 mapped non-root bones. Across all ten clips,28 bones have maximum local-basis component error at most`1.0602976319384894e-5`. The12 upper/lower arm, hand, upper/lower leg and foot bones differ substantially, up to`1.3540701319675792` for the right forearm. Hips are omitted because they are a body/root transform; absent eye/finger bindings remain omitted. This is a useful bounded partial match, not a whole-skeleton success. Every compared axes record has source type1; other types are rejected. The limb concentration motivates inspecting source goal solving/twist distribution, but does not by itself prove which missing operation causes each residual. Subsequent tool hardening adds explicit launch/source and arithmetic-library provenance checks; the initial diagnostic must not be promoted to an independently qualified complete-retargeting result.

The earlier [zero_muscle_probe](../../rebuild/tools/export_reference_player.py#L135) is deliberately not an oracle for zero-muscle poses: Avatar default/T-pose is not established as zero muscles. Likewise, the scalar root/goal names recovered here do not establish their coordinate-frame transforms, human scale application, limb goal solve, twist distribution or blend order. Those are the remaining algorithmic work before a complete offline retargeter or independently advancing masked shield pose can be claimed. Existing [shield composition evidence](SHIELD_LAYER_CAPTURE.md) remains useful comparison data, not a substitute for that proof.


The subsequent [697-pose comparison](HUMANOID_ROTATION_PROOF.md) adds source-backed loop-pose correction and verifies launch/Avatar/helper provenance. It supersedes the first diagnostic metrics above as the current partial-rotation result; all12 limb bones remain unresolved.
