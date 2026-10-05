# Sandbox terrain material recovery

October 5, 2026. This records **source-derived** bindings and exported installed assets. It does not establish an original-game graphical comparison, Adventure world-generation parity, or complete Unity lighting parity.

## Original palette and bindings

[TerrainSet1Mat](../../Assets/Terrain/Materials/TerrainSet1Mat.mat) points by GUID to [VoxelBlend_Def.shader](../../Assets/Terrain/Voxelform2/Voxelform/Source/Shaders/VoxelBlend/VoxelBlend_Def.shader), whose shader declaration is `VoxelMultiMaterial_Def`. The installed `PE_Client_Data/resources.assets` contains matching `TerrainSet1Mat` (path ID2408), six diffuse textures, six normal textures and `terrainNormSpec`. Its float properties match the original serialized material. Shader asset name `VoxelBlend_Def` differs from its shader declaration. The exporter resolves the external installed shader pointer rather than interpreting that path ID in the resources file.

The source [Voxel Terrain prefab](<../../Assets/Prefabs/Voxel Terrain.prefab>) component11400004 has `defMat` referencing this material. [VFVoxelTerrain](../../Assets/Terrain/Voxelform2/Voxelform/Source/Scripts/Voxel/VFVoxelTerrain.cs) assigns `_defMat` to chunk/transvoxel shared materials. The legacy prefab field spelling differs from the present C# field spelling; it is supporting asset provenance, not evidence of an observed current runtime scene.

The original [VoxelBlend.cginc](../../Assets/Terrain/Voxelform2/Voxelform/Source/Shaders/VoxelBlend/VoxelBlend.cginc) decodes three material IDs from mesh channels, subtracts one, and blends their sampled colors with interpolated one-hot vertex weights. `CalcBaseUV` gives this mapping:

| Source voxel IDs | Diffuse atlas | Grid | Repetition | Tile inset |
|---|---|---|---|---|
| 1–16 | terrainset1_d | 4×4 | 3m | .03125 |
| 17–32 | terrainset2_d | 4×4 | 3m | .03125 |
| 33–48 | terrainset3_d | 4×4 | 3m | .03125 |
| 49–64 | terrainset4_d | 4×4 | 3m | .03125 |
| 65–68 | terrainset5_d | 2×2 | 10m | .015625 |
| 69–84 | terrainset6_d | 4×4 | 3m | .03125 |

Tiles run left-to-right, then bottom-to-top. The inset comes from `_Indent=8` times the shader correction `1/256`, halved for the2×2 atlas. Sampling takes `base + (inset + fract(world/repetition)*(1-2*inset))*tileSize`. Diffuse atlases are2048×2048 except terrainset3_d (atlas2), which is1024×1024 in both source and installed assets; installed normals are1024×1024 DXT5nm. The normal/specular lookup is256×2.

For normal `n`, triplanar weights are `normalize(abs(n))`, then `max((weights-.2)*7,.01)`, then division by their sum. Plane coordinates are ZY for the X weight, ZX for Y and XY for Z. This prevents the horizontal texture from stretching onto excavated walls. The native OpenGL branch uses `0.73*log(0.04*distanceSquared)` for explicit mip selection; unlike D3D9's branch, forward `surf` calculates its lighting normal from the geometric world normal without applying the recovered bump vector. Deferred/prepass paths and full lighting remain separate concerns.

## Synthetic versus original world IDs

[PeWorld](../../rebuild/src/pe_world.h) distinguishes `source=0` synthetic palette from `source=1` imported original voxel IDs. Synthetic dirt1/stone2/grass3 are **not** the source palette. The presentation adapter selects representative original dirt1, stone17 and grass3 without changing voxel/save bytes or resource transactions. This selection is a native presentation choice, not recovered generator behavior.

The source database `resource` row1 is loose soil (`松土`, dirt268), row17 is stone (`石头`, stone280), and row3 is soil (`土`, dirt268) with a grassy diffuse tile. [Grassland paint XML](../../Assets/Resources/RandomMapXML/tmpPaintVxMat.xml) uses types3/4 on height17–90 and slope0–130. This supports selecting grass3; it does not reproduce the original slope/height/biome distribution. Source material2 is soil with a different surface, so mapping synthetic stone2 directly to original2 would be incorrect. Imported IDs1–84 keep their exact atlas slots, including the unnamed `?` resource rows. Unsupported IDs require an explicit fallback, never modulo aliasing.

## Reproducible assets and checks

[export_terrain_assets.py](../../rebuild/tools/export_terrain_assets.py) reads original source/material GUIDs and installed Texture2D assets, validates binding names, dimensions and floats, and creates `rebuild/build/reference-assets/terrain`. It preserves installed DXT compression appearance when decoding diffuse textures; normals retain the installed channel encoding. PNGs are vertically flipped from UnityPy's conventional top-down images so native raylib UV `v=0` samples the original bottom row. No extra renderer V flip is required.

Outputs are `diffuse0.png` through `diffuse5.png`, matching `normal0.png` through `normal5.png`, `normspec.png`, and `manifest.json`. The manifest includes source hashes/GUIDs, installed resources hash/path IDs, PNG hashes, all84 slots, original resource rows, shader settings and explicit synthetic aliases. The exporter also generates [pe_terrain_materials.h](../../rebuild/generated/pe_terrain_materials.h). Generated outputs must be regenerated rather than hand-edited.

```sh
/tmp/pe-asset-tools/bin/python rebuild/tools/export_terrain_assets.py
/tmp/pe-asset-tools/bin/python rebuild/tools/export_terrain_assets.py --check
/tmp/pe-asset-tools/bin/python -m unittest discover -s rebuild/tests -p test_terrain_assets.py
```

The [exporter tests](../../rebuild/tests/test_terrain_assets.py) cover all atlas boundaries,84 unique slots, strict invalid-ID rejection, synthetic/source separation, output provenance hashes and installed/source coarse orientation agreement. The full `--check` separately regenerates all runtime outputs. Tests requiring Pillow/recovered assets report skips explicitly; a skip does not verify those assets. See the current playtest record for actual C graphics evidence and limitations.

## Native rendering and verification

The sandbox renderer now loads the six recovered diffuse atlases once and lends them to all four terrain-region models. Exact atlas dimensions are checked; failed texture/shader creation releases staged GPU resources. CPU mesh positions, normals, winding, collision and voxel/save IDs are unchanged. Each triangle carries its three source IDs and interpolated one-hot material weights. The GLSL path implements source atlas addressing, inset/repetition, XY/ZY/ZX projection, normal-based projection weights and the OpenGL distance mip rule. Texture coordinates include original imported crop origin, so region-local rebuilds do not reset texture phase. Unknown IDs render magenta. Geometry edits rebuild the existing dirty regions with new material attributes.

The source OpenGL `surf` branch computes lighting from geometric normals and does not apply its sampled normal perturbation. The native renderer likewise uses geometric normals, but retains the existing simple ambient/directional lighting. Unity's full forward-lighting, shadows, fog, specular and color-space pipeline are not ported. PNG diffuse content is recovered, not newly generated art. Construction blocks and water keep their previous renderers.

Verification passed: both new C suites (all84 slot boundaries, source/synthetic separation, invalid-ID behavior and GPU failure/ownership cases), focused ASan/UBSan/leak checks, five actual-asset exporter tests,15-file regeneration, full ordinary `test`, `check-content` and23CLI checks. Unrelated optional asset tests in the ordinary system-Python suite still report dependency skips; they are not counted as asset validation. Physical shovel and pickaxe checks passed2/2 and1/1 swings/resource changes. A controlled crater spanning all four render regions shows grass/soil/stone after F9/F10, and a60-frame original Dragon Desert import shows its sand/soil materials. [Cutaway](../reference/sandbox/rebuild-terrain-layers-reloaded.png), [import](../reference/sandbox/rebuild-terrain-imported.png), [results](../reference/sandbox/rebuild-terrain-result.json). All owned C windows exited normally; no original save or asset was modified.
