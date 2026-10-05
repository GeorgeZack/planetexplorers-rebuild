# Player mesh and humanoid-pose recovery

The original default male and female modular meshes can be recovered without Unity. `rebuild/tools/export_reference_player.py` produces a static, textured OBJ in the serialized Avatar default pose, alongside the complete selected skin and Avatar data. It does **not** evaluate player attack animation or provide a validated moving knife-contact capsule. The training avatar remains the existing provisional visual until a separate integration is made.

## Repeatable local conversion

Use the optional isolated UnityPy 1.25.4 environment described in [ASSET_RECOVERY.md](ASSET_RECOVERY.md):

```sh
/tmp/pe-asset-tools/bin/python rebuild/tools/export_reference_player.py
/tmp/pe-asset-tools/bin/python rebuild/tests/test_reference_player.py
```

`--game-dir PATH` selects an installed original game. `--output PATH` defaults to ignored `rebuild/build/reference-player`; `--sex male|female|both` defaults to both. Original assets are read-only. The exporter stages a complete successful conversion before replacing its generated files, refuses output inside the installed game, and leaves unrelated output files intact. It launches no game, renderer, or desktop input.

Per sex, the output is `player-SEX-default.obj`, its MTL/diffuse PNGs, `player-SEX-default-rig.json`, and `player-SEX-default-attack6-channels.json`. `player-manifest.json` records selected parts, counts, bounds, database row IDs, input SHA-256 hashes and limitations. Files stay local and are not vendored into the C rebuild.

The OBJ contains eight named parts with their original submesh/material slots. Coordinates preserve Unity XYZ with no scene placement. Texture orientation follows the earlier reference asset exporter. Diffuse-only rendering omits Unity skin/hair shader behavior, masks, runtime colors, blend shapes and clothing. Hair transparency is consequently approximate. This is the source `DefaultMale`/`DefaultFemale` **body-part selection**, not a claim to reproduce a particular saved/customized player or all appearance defaults.

## Exact asset identities

Unlike a name-only search through duplicate objects, `PE_Client_Data/mainData` object 13 (`ResourceManager`) resolves the original `Resources` paths. Its external file 2 is `resources.assets`; file 4 is `sharedassets1.assets`.

| Resource | GameObject in resources.assets | Avatar in sharedassets1.assets |
|---|---:|---:|
| `Model/PlayerModel/Male` | 43029 | 778 (`MaleAvatar`) |
| `Model/PlayerModel/Female` | 44340 | 777 (`FemaleAvatar`) |
| `Prefab/PlayerPrefab/MaleModel` | 46211 | — |
| `Prefab/PlayerPrefab/FemaleModel` | 47841 | — |

`Assets/AppearBuilder/CustomCharactor.cs`, `AvatarData.SetBodyParts`, selects head index 0 and hair index 1 for both default bodies. `CustomMetaData.cs` selects torso, legs, hands and feet from the sex-specific body prefab. The `CustomMetaData` SQLite table supplies head/hair paths; its numeric fields are strings and must be parsed accordingly. The exporter preserves the original table iteration order used by `ReadFullTable`.

| Part | Male mesh ID | Female mesh ID |
|---|---:|---:|
| torso_0 | 7386 | 7119 |
| legs_0 | 7140 | 7259 |
| hands_0 | 7281 | 5948 |
| feet_0 | 6143 | 7376 |
| default head | 6931 (`m_head_02`) | 6410 (`w_head_01`) |
| front hair | 7358 | 7283 |
| top hair | 7005 | 7142 |
| back hair | 7241 | 6160 |

Male head/hair row IDs are 15/48 (`m_head_02`, `m_hair_01`); female IDs are 1/28 (`w_head_01`, `w_hair_00`). Male output contains 3,480 vertices and 5,210 triangles; female contains 3,707 vertices and 5,280 triangles. Their default-pose Y bounds are approximately 0.000287–1.816550 m and −0.001101–1.659852 m respectively.

## Skin and Avatar reconstruction

The research JSON retains original mesh-local positions, normals, UVs, triangle indices, four weights/joint indices per vertex, skin bone IDs/names, inverse-bind matrices, material properties and all serialized Avatar fields. This avoids losing animation inputs when writing a static OBJ.

`Avatar.m_AvatarSkeleton.m_Node` defines a parent-first hierarchy. `m_DefaultPose.m_X` supplies its local position/quaternion/scale. `m_SkeletonNameIDArray` uses CRC32 of the bone name; `m_AvatarSkeleton.m_ID` and `m_TOS` describe full paths. The exporter validates unique name hashes, composes the parent transforms and maps each modular skin's bones to that common Avatar skeleton. Each palette is:

```text
avatarDefaultBoneWorld × meshInverseBind
```

Weighted vertices are therefore assembled in the shared Avatar root space, even when a body-part mesh's own vertices use a different local frame. Inverse-transpose bone normals are blended and normalized for the static preview. Stored default pose is not relabeled as an evaluated animation frame or universally as a mathematical zero-muscle pose.

The male head also references lowercase `Bip01 mouth`, which is absent from the selected Avatar name map. Its original transform is preserved, and its local transform chain is composed below the nearest Avatar-mapped ancestor. The JSON records that fallback and its complete source transform. This is distinct from the uppercase `Bip01 Mouth` present in other source hierarchies. Female selected parts require no such fallback. Runtime facial animation and customization are outside this prototype.

## Attack6: recovered values, unresolved retargeting

See [PLAYER_ANIMATION.md](PLAYER_ANIMATION.md) for controller transitions, override selection, animation events and source hand mounts. The prototype additionally decodes every scalar in the representative third-person male Attack6 clip 179 and female clip 231 directly from installed `sharedassets1.assets`.

| Data | Male clip 179 | Female clip 231 |
|---|---:|---:|
| Dense frames at 30 Hz | 38 | 34 |
| Dense scalar channels | 136 | 117 |
| Constant scalar channels | 103 | 103 |
| Direct Transform bindings | 33 | 28 |
| Humanoid bindings (`classID=95`, `customType=8`) | 127 | 127 |

Male also has one ordinary Animator scalar binding with attribute hash 1207992894, retained as unclassified rather than guessed. Dense/constant offsets account for this extra scalar. Translation/scale Transform bindings consume three values, quaternion bindings four; the humanoid/Animator bindings here consume one. Tests require exact scalar coverage and reject a binding crossing the dense/constant boundary.

The JSON preserves bindings, raw numeric attributes, resolved paths where available, and every sample. It explicitly declares `bone_poses_evaluated=false`. Direct Transform bindings cover helper bones, tips and accessory mounts. Neither representative clip provides a direct TRS binding for `Bip01 R Hand`; decoding only the ordinary Transform curves cannot recover a faithful knife swing.

Unity's humanoid format represents normalized muscle values relative to Avatar pre/post rotations, signed angular limits and a normalized body reference. Its retargeting includes twist distribution and optional hand/foot IK. These operations must be reproduced or sampled from a matching Unity runtime before claiming equivalent bone motion. Unity's own explanation is the primary reference: [Mecanim Humanoids](https://unity.com/blog/engine-platform/mecanim-humanoids). The managed [`HumanPoseHandler` bindings](https://github.com/Unity-Technologies/UnityCsReference/blob/master/Modules/Animation/ScriptBindings/HumanPoseHandler.bindings.cs) call native pose functions; they do not contain an independent offline implementation of that solver.

A small diagnostic computes `preQ × inverse(postQ)` for each human axis and compares it with the stored human-skeleton local rotation, allowing quaternion sign equivalence. Only 14 of 41 axis-bearing bones match within 1e-4 for either Avatar; maximum quaternion residual is approximately 0.684. This **does not disprove** a swing/twist formula: the stored pose need not correspond to all-zero muscle values. It establishes that equating these two poses is not a valid calibration test here. The exporter records every residual and makes no retargeting claim.

A general asset converter is not sufficient evidence of humanoid support. The inspected primary [AssetStudio ModelConverter implementation](https://github.com/Perfare/AssetStudio/blob/master/AssetStudioUtility/ModelConverter.cs), `ReadCurveData`, handles Transform and blend-shape bindings and skips the other scalar cases. Its resulting animation export would omit the essential human channels in these clips.

The next validation target is a matching-runtime set of local/world matrices for the hips, clavicle, upper arm, forearm, right hand and `mountMain`, sampled at known Attack6 times with documented Avatar, FP parameter, state speed and IK settings. Those samples can calibrate root/body normalization, muscle-axis mapping, asymmetric limits, swing/twist order and twist distribution. Only then should a recovered palette drive the original knife contact capsule. Exporting readable muscle values is useful groundwork, not a substitute for that validation.

## Validation status

The exporter runs entirely headlessly. Tests cover exact source default selections, invalid weights/joints, dense/constant channel partitioning, missing-input preservation, real mesh counts and bounds, unit output normals, all 127 humanoid channels, absent direct right-hand animation, and byte-identical repeat exports. No player graphics preview or humanoid animation comparison has been performed for this prototype.
