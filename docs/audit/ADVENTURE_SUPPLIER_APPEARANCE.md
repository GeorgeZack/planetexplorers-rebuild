# Archived Adventure supplier appearance

The native scene `rebuild/build/reference-assets/adventure-supplier/recording.peas` recovers **Eitai Takayama, entity20016, RandomNpc prototype22**, from the preserved Adventure manual-slot1 archive. It is a particular saved supplier appearance, not a renamed tutorial NPC or a general Adventure character generator. Runtime C loading needs only the exported PEAS/PEAC/PNG files; it does not read Unity assets or the reference save.

## Identity and source selection

The read-only input is `~/.local/share/planet-explorers-sandbox-reference/PlanetExplorers/GameSave/save0`. `index.arc` declares bounded ranges into `world.arc`. Both carry version5, a shared archive GUID and their complete byte length. `ArchiveKeyCreature` stores entity components; `ArchiveKeyItemAsset` resolves equipped instance IDs. The exporter resolves the source database in SQLite read-only mode.

`Assets/PeWorld/NpcEntityCreator.cs` creates Adventure role NPCs through `AdRandNPCMission` and `RandomNpcDb`. Database role2 selects RandomNpc22, the unique archived Eitai entity20016. This establishes the supplier role from source/prototype identity; the exported scene does not claim that every newly generated Adventure world names its supplier Eitai. The preserved entity location is `(-598.5214233398438,22.978425979614258,341.3612365722656)`, identity rotation. A bounded replacement world must place the supplier explicitly; this saved coordinate does not establish its procedural terrain or arrival geometry.

`EntityInfoCmpt` supplies the name. `AvatarCmpt` stores `Prefab/PlayerPrefab/FemaleModel`, 27 base morph values, eye/lip/skin/hair colors and eight nude-avatar slots. `EquipmentCmpt` contains instances66–72:

| Instance | Item | Recovered appearance |
| --- | --- | --- |
| 66 | 97 | Female bronze torso |
| 67 | 133 | Female bronze legs |
| 68 | 75 | Female bronze helmet |
| 69 | 172 | Female bronze gloves |
| 70 | 194 | Female bronze boots |
| 71 | 33 | Knife, source stowed waist mount |
| 72 | 36 | Copper shield, source left forearm attachment |

`CustomCharactor.AvatarData.GetSlot` and `AvatarCmpt.AddPart` determine replacement slots. Helmet75 replaces HairT, retaining the saved hair08 front/back parts. Head08 and the five armor parts supply the other body slots. The exporter checks the reference profile's `HideHeadgear=False` setting and hashes it; a different setting is rejected instead of silently changing this selection. Material submeshes produce nine body/armor parts, two knife parts and one shield part.

## Morphs, skinning and colors

`Assets/AppearBuilder/AppearBuilder.cs: BakeToMesh` selects positive/negative morph channels in pairs, scales by the absolute saved weight and skips channels absent from a mesh. The exporter supports the actual single-frame, full-weight100 channels. The selected head has14 applicable signed parameters, torso6 and legs4. These meshes have sparse position deltas; their source channels do not supply normal deltas. Gloves/boots have no applicable blend shapes, so their saved hand/foot values do not fabricate deformation.

For morphed parts, source `BakeMesh` also skins the resource renderer's current pose before transferring the mesh and original inverse bind matrices to the target Avatar. The exporter applies that resource-local palette, including the small approximately0.001248 maximum identity residual in head/torso resources. It retains original vertex weights, material slots and inverse binds. No substitution of a default or starter body occurs.

`AppearBuilder.SetColor` sets hair `_Color`, head `_Color`, eye `_SkinColor`, and non-helmet body `_SkinColor`. The installed materials' source diffuse and mask textures are recovered. The Avatar Standard `Albedo` function in `Assets/Scripts/WuYiqiu/Resources/Shaders/Avatar Standard/PEUnityStandardInput.cginc` supplies the tint/mask expression. The exporter bakes that diffuse expression into PNG, preserving texture alpha. Masks with a different resolution use bilinear resizing. The saved lip color is recorded but is not applied by the inspected `SetColor` function.

This is **diffuse recovery**, not shader equivalence: original specular, normal maps, lighting, gamma conversion, continuous mask filtering, hair cutoff and material render state are not reproduced by these PNGs. There has not yet been a live `BakeMesh` comparison against Eitai. Base weights are saved; transient abnormal-condition submorphs and other unsaved runtime overrides are not reconstructed.

Knife initialization in `PEHoldAbleEquipment.InitEquipment` attaches the source `m_PutOffBone` (`mountWaist` for item33). `PESheild.InitEquipment` attaches item36 to `Bone L Forearm`; the installed shield hierarchy's local transforms are baked once. `BoneCollector.SetModelParent` keeps local transforms. These establish the exported idle attachment defaults, not the saved transient held/hidden/combat action state.

## Idle motion and collision

The body uses the existing closed `female-clips-02.jsonl` isolated Unity bake: matching FemaleAvatar and source `NormalIdle`. Only that clip is exported. All41 required bones map to captured hierarchy paths, including attachment mounts. This reuses original humanoid clip evaluation; it is not an observation of Eitai's live Animator, NPC IK, gestures or controller transitions.

The multipart scene contains one looping NormalIdle clip,30Hz,62 samples, using PEACv2's exact terminal timestamp. Its12 parts have8,800 exported vertices. The native headless loader and CPU skinner evaluated all744 part-poses with finite positions/normals; combined bounds are approximately `(-0.46377,-0.00243,-0.42120)` to `(0.24894,1.66725,0.39373)` in model-local Unity XYZ meters. The duplicate head material submesh carries its own vertex records, as required by the existing multipart format.

`export_adventure_npc_collider.source_capsule()` supplies the identical physical PESC and source metadata used by the encounter module. CapsuleCollider13600000 in `FemaleModel/Model` is enabled, nontrigger, layer10, Y-axis, center `(0,.875,0)`, radius `.3700000047683716`, height1.75. The manifest separately records source Defence13690738 and the source-derived outer-collider growth. Visual mesh bounds do not replace interaction collider bounds or establish movement blocking policy.

## Reproduction and evidence

The optional exporter uses the existing isolated UnityPy environment; it never launches Unity or sends input. A **fresh output destination** is required. Preserve the save and existing recovered assets.

```sh
/tmp/pe-asset-tools/bin/python rebuild/tools/export_adventure_supplier.py \
  --output rebuild/build/adventure-supplier-new
python3 -m unittest discover -s rebuild/tests -p test_adventure_supplier.py
```

Defaults select the preserved Adventure save, installed native game and closed female-clips02 capture. `--save-dir`, `--game-dir` and `--bake` can name equivalent validated inputs. The verified training-knife visual/collision fixtures must already exist under `rebuild/build/reference-assets`.

`read_reference_adventure.py` validates archive versions, GUIDs, file/record bounds, finite weights/colors, entity/prototype identity, component versions, slot count and equipped item relationships. Tests cover exact selection, read-only behavior, malformed/truncated/GUID-mismatched archives, nonfinite appearance data, signed morph pairs, sparse-delta bounds and tint/alpha semantics. Seven tests passed. Two independent offline exports produced byte-identical content for all25 runtime scene/actor/texture/collider files. Input archive, settings and database SHA256 values remained unchanged. The native headless checks passed; subsequent graphical verification is recorded in the integrated checkpoint below.

Full source hashes, selected resources/materials/morphs, collider metadata and saved identity are retained in `supplier-manifest.json` and `source/supplier-rig.json`. The closed bake validation report is `source/bake-report.json`. Original resource files, save archives and shaders remain unchanged.

Integrated C desktop verification subsequently passed: source menu and all five dialogue pages, visible saved appearance/portrait, cancellation without items, final grant once, and save/reload. See [PLAYTEST](../PLAYTEST.md); this does not imply a new original-game observation or full town parity.
