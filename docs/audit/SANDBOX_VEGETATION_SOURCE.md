# Wild vegetation source recovery

This audit qualifies four original wild plant identities and their recovered geometry without launching Unity or the installed game. It supports the [fixed sandbox examples](SANDBOX_VEGETATION.md); it does not establish procedural placement, randomized loot, animation timing or rendered visual acceptance.

## Identity and production

[VETreeProtos.prefab](../../Assets/PeLauncher/Resources/VETreeProtos.prefab) contains the `VoxelEditor.m_treePrototypeList` of512 references. [RSubTerrainMgr.Init](../../Assets/Scripts/ZhouXun/Random%20SubTerrain/RSubTerrainMgr.cs) consumes that list. A prototype index is not the number in a prefab filename. These are ordinary wild prefabs, independent of the tutorial gathering lesson and cultivated `Prefab/Item/Plant` objects.

| Prototype | Original prefab | NaturalRes row | Original fixed production trials | Native example |
| --- | --- | --- | --- | --- |
|15|`tree15_grasslandC`|1015|2; Shade Weed238 at100%|238×2|
|37|`tree08_grassland`|1037|3; Tulip235 at70%, otherwise1583|235×3|
|40|`tree09_grassland`|1040|4; Maria Sunflower239 at70%, otherwise1583|239×4|
|146|`tree16_grassland`|1146|3;1006 at50%, otherwise1583|Decorative; no harvesting|

The generated [source manifest](../../rebuild/generated/pe_vegetation_source.json) retains the literal database rows, including optional production groups. Prototype15 includes optional943;37 includes388/940/1583;40 includes388/944/1583. The fixed native examples intentionally give only the primary item and omit fallback and optional rolls. This does not reproduce wild loot probabilities. Prototype146 also has source type10 resource data; making it decorative is an explicit fixture policy.

[NaturalRes](../../Assets/Scripts/NaturalRes/NaturalRes.cs) converts each production group into cumulative thresholds. `DigTerrainManager.GetTreeResouce` in [DigTerrainManager.cs](../../Assets/Scripts/BuildNDig/DigTerrainManager.cs) chooses `fixed_num` when positive; otherwise it derives quantity from `self_num × width² × height × (1+bonus)`. Quantity is at least1. The primary loop truncates that quantity to an integer and samples each trial against cumulative thresholds. Optional production has an additional probability gate and a float loop bound; it is not equivalent to a guaranteed seed per plant. None of these random operations are executed by the native fixed examples.

The resource durability is255 for all four rows. [SkEntitySubTerrain](../../Assets/Scripts/SkillSystem/SkEntitySubTerrain.cs) and `DigTerrainManager.Fell` own source damage/removal and production. The resource `gather_skill=10007` is preserved as database evidence; it must not be substituted for the active `Action_Gather` skill20110003.

## Picking and action admission

[Action_Gather](../../Assets/Scripts/PeEntity/Cmpt/Motion/PEAction/Action_Gather.cs) requests nearby objects with3m and360degrees, excludes collider-bearing trees, and requires `NaturalRes(prototype+1000).m_type == 10`. The Adventure `RSubTerrainMgr.Picking` implementation clamps the requested distance to2m, clears the vertical component before measuring distance, and rejects only distances greater than the limit. Therefore its effective range is **inclusive2m in XZ**, within the separate floor-position±2 cell scan in all three axes. The apparent `Vector3.Distance(trans.position, trans.position)` guard in `UpdateOPTreeInfo` provides no additional restriction.

With a free mouse, the source intersects `PeCamera.mouseRay` with the selected raw mesh bounds multiplied by height scale and translated to the plant root. Captured-pointer mode skips this ray check. `RSubTerrainMgr.Init` records `MeshFilter.mesh.bounds`; no collider is attached to any of the four selected prefabs. Native plants use those bounds at unit scale for picking and do not create player, camera or tool walls. Native closest-hit arbitration, ray selection in all camera modes and geometry line-of-sight checks are explicit host policies.

The exact `ActionRelation` row32 has value2 for SwordAttack, Dig, Fell and Build. `ActionRelationData.LoadData` interprets2 as a required-false mask. `MotionMgr.CanDoAction` and `DoAction` check dependencies before executing or canceling other actions. Gathering must therefore **reject an active melee/tool action**, rather than interrupting it or harvesting concurrently. `Action_Gather.CanDoAction` also requires a valid transform, a selected plant and no aim state. The exporter guards the complete relation row and these source method bodies.

Source gathering sets the Gather mask, triggers the `Gather` animation and starts skill20110003 on its `Gather` event. `GatherEnd` completes the action; cancellation resets the mask and animation. The native fixture instead transfers its fixed yield and depletes the plant immediately, atomically. Its animation, skill delay, resource damage and original random production remain unported. It uses action-input ownership so successful collection does not pause movement or other actors.

## Offline mesh and material recovery

[export_sandbox_vegetation_assets.py](../../rebuild/tools/export_sandbox_vegetation_assets.py) reads the installed `ResourceManager` entry for `vetreeprotos`, recovers its512-entry MonoBehaviour with the original `VoxelEditor` schema, and follows each selected original index to its MeshFilter and MeshRenderer. It verifies source prefab GUIDs, object/mesh names, absence of colliders and single-submesh/material structure. The custom typetree must consume the full serialized component; it does not search for approximate byte patterns.

| Prototype | Export | Triangles |
| --- | --- | --- |
|15|`plant-15.obj`|522|
|37|`plant-37.obj`|165|
|40|`plant-40.obj`|647|
|146|`plant-146.obj`|2282|

All four use original material `Grassland_tree_A`, shader `ats SH-Tree Leaves`, cutoff0.6499999761581421, RGB tint0.8823529481887817 and alpha1. The recovered shader declares `Cull Off` in all five passes. Their shared diffuse texture is installed `resources.assets` object5597. The ForwardBase pass has `Cull Off`, `ColorMask RGB`, no blend or depth-write override, and explicit fragment discard below `_Cutoff`: it is opaque cutout, not translucent alpha blending. Native drawing retains the source alpha cutoff and two-sided leaves, then writes opaque surviving fragments. The ForwardAdd pass separately uses additive `Blend One One` and `ZWrite Off`; the native renderer does not reproduce the complete Unity lighting, wind or shader passes.

Geometry retains raw Unity XYZ vertex positions, normals and UVs. Bounds come from the source mesh AABB. Unit root transforms are deliberate fixture adapters; serialized prefab transforms are recorded in the asset manifest, not silently claimed as original instance transforms. No actual wild placement or instance scale has been recovered. Existing cuttable tree examples reuse the prior `Grassland_tree_B` asset and8-Wood/255HP adapter; its `tree11_grassland` filename is not prototype11.

Assets are staged in a fresh directory and installed at ignored `rebuild/build/reference-assets/sandbox-vegetation`. The exporter refuses to overwrite an existing output directory or write into the installed game. Its manifest records source prefab/meta hashes, installed object IDs and raw mesh/material/texture/shader hashes, original transforms, bounds and output hashes. The earlier output preceding the shared OBJ/MTL preamble correction is preserved at `sandbox-vegetation-before-validator-preamble`.

## Regeneration and checks

```sh
# Requires an absent output directory; preserve an existing export first.
rebuild/build/unity-tools/bin/python rebuild/tools/export_sandbox_vegetation_assets.py
python3 rebuild/tools/export_vegetation_source.py
python3 rebuild/tools/export_vegetation_source.py --check \
  --check-assets rebuild/build/reference-assets/sandbox-vegetation
python3 -m unittest discover -s rebuild/tests -p test_vegetation_export.py -v
```

[export_vegetation_source.py](../../rebuild/tools/export_vegetation_source.py) uses the ordinary Python interpreter. It guards original prefab hashes, complete source method/class bodies and exact database rows, then produces gameplay/visual headers and the source manifest. Optional asset validation compares the installed export against independently pinned qualified hashes, not merely against its own manifest.

Executed: direct offline recovery of all four models succeeded; metadata and asset checks passed; all13 focused Python tests passed with0skips. Negative tests alter source distance/type/rounding, prefab identity, database production/action masks and recovered OBJ data and verify rejection. The asset-dependent tests would skip when local recovered outputs are absent; they did run here. This evidence establishes source and export contracts, not physical gameplay or on-screen appearance. The parent implementation audit and PLAYTEST record runtime validation separately.
