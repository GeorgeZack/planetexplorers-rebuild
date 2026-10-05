# Archived starttown terrain recovery

The archive-derived unrotated `town1_8` building origin is **(-623,16,327)**. The bounded native crop uses origin **(-627,16,323)** and dimensions **64×32×128**. These are source-world voxel sample coordinates, not translated mesh vertices. The complete background remains procedural; the `.art` alone cannot establish the surrounding terrain or Eitai's support.

## Source-derived placement and material rules

[VArtifactUtil.GetArtifactUnit](../../Assets/Scripts/PuJi/Town/Tool/VArtifactUtil.cs) computes `isoStartPos` from the actual artifact header. Its `GetPosAfterRotation` uses that same start for building/NPC offsets. `OutputVoxels` subtracts half the header dimensions from the unit center. The installed header is X60/Y80/Z100; the database metadata is X55/Z100/Y14. Therefore the archived building-derived origin also equals the voxel origin at yaw0; **there is no2.5m X shift**. The center is(-593,16,377). Integer metadata55/2 gives inclusive unit query bounds X[-620,-566], Z[327,427], which differ from the authored voxel extent.

[VArtifactData.Import](../../Assets/Scripts/PuJi/Town/ArtifactData/VArtifactData.cs) reads a BinaryReader header, materials and zlib-compressed components/voxel/color records. Voxel keys encode `x | z<<10 | y<<20`; each little-endian ushort contains volume followed by local material slot. The installed artifact contains42,350 voxel records, including fractional surface densities. Preserve these byte volumes, rather than thresholding them into solid cells.

[VAMaterial.Import/CalcGUID](../../Assets/Scripts/PuJi/Town/ArtifactData/VAMaterial.cs) derives runtime `m_Guid` by rounding serialized specular power. Both populated slots agree with the outer serialized GUID in this asset: slot0→240, slot1→45. [VFDataRTGen's town overlay](../../Assets/Terrain/Voxelform2/Voxelform/Source/Scripts/Voxel/VFDataRTGen.cs) maps GUID240 through the current column's biome `mapType` and `VArtifactUtil.triplaner`: grassland4, forest8, desert14, redstone26, rainforest9, hill8, swamp2, crater20. GUID45 remains45. Assuming every column is grassland is insufficient evidence for a complete crop.

The town overlay only operates inside the unit's inclusive query bounds. For a column containing artifact samples it clears background at/above worldY26, adds and saturates artifact volumes below town maximumY30, fills through the lowest artifact sample down to worldY0, and later restores recorded town material IDs over the ordinary terrain material pass. Outside those columns, generated background remains. Noise, roads, biome transitions and saved edits cannot be recovered by laying the bare artifact over an invented plane.

## Offline artifact diagnostic

[export_adventure_terrain.py](../../rebuild/tools/export_adventure_terrain.py) decodes the installed file and exports a diagnostic with explicit biome selection:

```sh
python3 rebuild/tools/export_adventure_terrain.py --map-type 1
python3 rebuild/tools/export_adventure_terrain.py --map-type 1 --check
python3 rebuild/tests/test_adventure_terrain_export.py
```

The output under ignored `build/reference-assets/adventure-settlement/town-artifact-only.pevx` explicitly records the chosen biome assumption, material GUIDs, preserved input hash, archive-manifest hash and missing generator operations. It must not be mistaken for the full gameplay terrain. The exported header signature is retained as metadata, not independently authenticated. The Python checks include installed-artifact decoding, material/partial-volume checks, malformed readers and crop ordering; an absent installed asset is a skip, not recovered evidence.

## Bounded original-runtime capture

[reference_terrain.py](../../rebuild/tools/reference_terrain.py) uses the compiler and main-thread installation shim documented in [REFERENCE_POSE.md](../../rebuild/tools/REFERENCE_POSE.md). Its default action is an offline build. The optional observation component [ReferenceTerrainProbe.cs](../../rebuild/tools/ReferenceTerrainProbe.cs) reads existing LOD0 `VFVoxelTerrain` chunk arrays at end of frame; it never calls generation, voxel writes, input, teleport or save APIs. It adds a transient hidden component and incurs allocation/IO overhead.

An operator must create a fresh profile clone containing the preserved Adventure checkpoint before launch, following [reference runtime rules](../REFERENCE_RUNTIME.md). Do not use the normal profile or overwrite the archived checkpoints. With concrete fresh paths:

```sh
python3 rebuild/tools/reference_terrain.py
python3 rebuild/tools/reference_terrain.py --launch \
  --user-data /absolute/fresh-profile \
  --output /absolute/fresh-profile/terrain-captures/starttown.pevx
```

Load the archived Adventure and wait at the settlement until terrain is visible. Create the printed `.arm` path. The probe requires all intersecting LOD0 chunks to exist with actual data; missing chunks are not silently converted to air. It copies raw arrays, handles the source two-byte uniform-chunk representation, and compares entire crops across two consecutive end-of-frame samples before writing. The probe stops after120 seconds armed,1800 seconds waiting, a printed `.stop` file, or error. These bounds are checked on Unity frames; a suspended/unfocused game must resume frames to observe them. The pose launch flags are inherited, but the terrain probe's fixed bounds govern its behavior.

The create-new binary output is524,320 bytes. The adjacent `.pevx.json` records Unity version, frame, two-frame stability and loaded chunk count. The launch manifest records the isolated profile and command. The snapshot captures resolved terrain materials including ordinary generated terrain and whatever edits are already present in that checkpoint. It does not capture water, vegetation or building meshes. A stable two-frame sample is bounded runtime evidence, not proof of the entire procedural generator's deterministic reproduction.

The shared PEVX v1 format is little-endian: magic`PEVX`, version`uint32=1`, origin`int32[3]`, dimensions`uint32[3]`, followed by interleaved `volume:uint8,type:uint8` pairs in X-fastest, thenY, thenZ order. Header size32 bytes; payload size`2*sx*sy*sz`. Volume≥128 is the source isosurface classification; retain the raw byte for interpolation. `read_pevx` validates magic/version/length and returns the bounded sample data.

Offline probe compilation succeeded against the installed Unity5.2 assemblies. Compilation alone does not establish a complete crop. The original capture completed later and is recorded below; graphical C acceptance remains a separate operator check.

## October 5 original capture and installation

The operator launched a cloned isolated profile at `/home/admin/.local/share/planet-explorers-settlement-reference`, loaded the Adventure checkpoint, and armed the probe. The original Unity5.2.4f1 runtime produced `terrain-captures/starttown.pevx`: frame7550,30 loaded chunks, byte-identical complete crops on two consecutive end-of-frame samples. Its524,320-byte SHA256 is `99e0006dd2866b17f741988f90eb815ca3c589b14c8daf43dc62dcfdc67ab387`.

All30 original sandbox-profile archive files in the pre-launch `source-save-hashes.json` still matched their saved SHA256 values after capture. This verifies the original reference profile, separately from ordinary writes permitted in the disposable clone.

```sh
python3 rebuild/tools/export_adventure_terrain.py --capture \
  /home/admin/.local/share/planet-explorers-settlement-reference/terrain-captures/starttown.pevx
# Repeat with --check to verify the installed bytes and original-save hashes.
```

`--capture` requires the exact bounds, runtime metadata, two-frame stability and30 loaded chunks. It checks a supplied metadata SHA256 when present, computes the binary SHA256, verifies the original profile against the capture profile's baseline hash file, and creates only `terrain.pevx`, `terrain.pevx.json`, and `terrain-manifest.json` in the ignored settlement asset directory. Existing differing destination files fail closed; unrelated recovered assets are untouched. `--source-profile` selects a different original profile for an explicitly corresponding baseline. The four Python tests cover invalid metadata/hash/original-save changes as well as installed-artifact and binary checks.

The observed resolved material IDs are0,1,3,4,17,22,45,66,68,71. Material0 occurs in air; occupied voxels use the other nine IDs. Around Eitai's archived XZ(-598.52,341.36), the nearest checked integer column(-599,341) has its last solid sample at worldY22. The welcome/start pad near(-606,348) and store near(-583,356) has its last solid sample atY27. This establishes why a flat pad or artifact-only terrain cannot support both archived NPC positions correctly.

Comparing the10,269 authored town samples at worldY26..29, within source query bounds, against the source overlay including the lowest-sample fill rule yields10,268 exact volume/material matches. The one difference is world(-597,26,345): artifact-derived volume119/type4 versus observed air0/type0. A prior saved edit is a plausible explanation, not established by this capture; the installed fixture preserves the actual observed byte pair. No voxel was fabricated to force agreement.

The separate headless C fixture [test_settlement.c](../../rebuild/tests/test_settlement.c) checks loader byte ordering, all header fields, short headers/payloads, trailing data and failure atomicity. When both terrain and static collision assets exist it also exercises Adventure initialization, safe player support, exact archived NPC XZ anchors with vertical settling, and PECS save/load. Missing assets produce an explicit skip. Its actual-asset section runs in a child process to retain the parent's immutable default geometry/root configuration for other tests.
