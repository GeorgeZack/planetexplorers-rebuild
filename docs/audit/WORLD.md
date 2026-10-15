# Terrain, world, rendering, construction, persistence and networking audit

Source-derived format/algorithm contracts plus native importer evidence. Source paths below are repository-relative. `V/` abbreviates `Assets/Terrain/Voxelform2/Voxelform/Source/Scripts/Voxel/`.

## Authoritative world representation

- `V/VFVoxel.cs`: two bytes per terrain sample, byte Volume followed by byte Type. Volume 0 is air, 255 full solid; surface threshold is **128**, not 127.5. `ToNormByte` truncates volume*255. Water source material IDs occupy 128–249; 250–255 reserved. The `IsBuilding >=64` convention is marked deprecated and must not become a universal material classifier.
- `V/VoxelTerrainConstants.cs`: default `_shift=5`, 32 cells per chunk axis, prefix halo 1 and postfix halo 2; a loaded dense chunk is **35×35×35×2=85,750 bytes**. World scale 1.0 m/sample. Source warns about mesh offset (-1,-1,-1). A disabled software compile option changes chunk size to 8; do not assume that option is active just because CPU extraction is used.
- `V/VFVoxelChunkDataHelper.cs:131`: dense index `((z+1)*35*35+(y+1)*35+(x+1))*2`; x varies fastest. Halos serve cross-boundary gradients, extraction and edits. Maintain all overlapping chunk halo copies when writing near chunk borders.
- `V/VFVoxelChunkData.cs`: uniform ('hollow') chunks store only a two-byte sample. Air, solid-by-type and water-plane have shared constant buffers. Before editing a uniform chunk, expand it. A chunk's version stamp guards asynchronous mesh requests against attaching stale output. Build steps are data-load start, data-load finish and GameObject creation. Port this lifecycle as explicit state plus generation counters.
- `V/VoxelTerrainConstants.cs`: authored story-world constants 18,432×2,944×18,432 meters, split into 6,144-meter x/z files (3×3). 4×4×4 chunks per compressed piece: 128 m each axis. `V/VFLODOctreeMan.cs:380`: LOD 0 through 4 inclusive.
- Adventure world configuration is different from story bounds. `Assets/Scripts/PuJi/Town/Tool/RandomMapConfig.cs`: map sizes 40km,20km,8km,4km,2km square; default seed 666, height 128, centered ±20,000 initial bounds. Store actual mode and configuration with a world; do not force all modes into story-world constants.

## Terrain meshing and rendering

`V/MarchingCubes/Cpu/MarchingCubesSW.cs` is a readable CPU reference, supported by `MarchingCubesSW_Tables.cs`. It traverses each chunk's 32³ cells at halo indices 1..32. Corner volume `<128` sets a case bit. Edge interpolation is `(128-v0)/(v1-v0)`. It outputs non-indexed triangles, interpolates negative density-gradient normals from neighbor samples, and packs normals/material information in UV/UV2 rather than ordinary mesh normal channels. A replacement shader must decode this original packing or, preferably for new C-owned meshes, receive explicit normal/material attributes. Preserve winding and coordinate conventions when moving between Unity and a C renderer.

`V/MarchingCubes/Cpu/SurfExtractorCpuMC.cs` is a worker-thread queue: prioritize fine LODs, discard invalid generation stamps, copy immutable input to a work buffer, create meshes on main thread, then schedule transition seams. The OpenCL counterpart is `V/MarchingCubes/Ocl/`, using an external OpenCL context; OpenCL acceleration is not required for a faithful CPU first implementation.

`V/TransVoxel/SurfExtractorTrans.cs`, `ported/TransvoxelExtractor.cs`, `Lengyel/Tables.cs` implement LOD transitions. Rendering only coarse chunks without seams will produce cracks. Initially one resolution avoids this; full replication needs the transition algorithm and reference fixtures. Material shading families are in `Assets/Terrain/Voxelform2/Voxelform/Source/Shaders/`, especially `VoxelBlend/` and triplanar shaders. Do not mistake a flat-color prototype for replicated art.

Water is a **separate volume field**, not terrain type replacement. `V/VoxelWater/VFVoxelWater.cs` defaults sea level to 97, water-source sample `{255,128}`, special surface volume 128, its own archive key, material, per-LOD sea mesh and fluid processor. The sea level is configurable (97 is a source default, not all adventure maps). `EulerianFluid.cs` covers dynamic volume propagation. First slice may use a static visual water plane with no swimming/flow; document the difference.

Vegetation has independent terrain systems under `Assets/Scripts/ZhouXun/Layered SubTerrain/` and grass systems; edits notify these systems to refresh vegetation. Terrain is also tied to item drops, durability, NPC pathfinding and physics via `Assets/Scripts/BuildNDig/DigTerrainManager.cs`; density removal alone reproduces only geometry.

## Procedural world replication

`V/VFDataRTGen.cs` (~large partial class), `VFDataRTGenTerrain.cs`, `VFDataRTGenGenVoxel.cs`, `VFDataRTGenCave.cs`, `VFDataRTGenMineral.cs`, `VFDataRTGenOptimise.cs`, `VFDataRTGenInterface.cs` are the generation authority. The pipeline produces 35×35 x/z column tiles with volume/type pairs through vertical height, terrain and water separately; terrain classification, density clamping, sea/river/lake/bridge features, caves, minerals, biome painting, town leveling and connection cuts, grass and trees are coupled. Biome enum in `Assets/Scripts/System/GameConfig.cs`: GrassLand=1, Forest, Desert, Redstone, Rainforest, Mountain, Swamp, Crater. Do not collapse this into one heightmap: caves and overhangs require density volumes.

Generation initializes multiple SimplexNoise instances from seed plus fixed offsets (`VFDataRTGen.cs:681`, `:898`), separately for terrain, caves, mines, rivers and biomes. The first terrain offset is biome ID. Exact replication requires **legacy .NET System.Random**, not C rand(), for permutations and world decisions. `Assets/Scripts/System/SimplexNoise.cs` resets p[0..255] to identity and swaps each i against `rand.Next(0,255)` (upper bound exclusive: index 255 is never randomly chosen), then duplicates the permutation. Its double constants F2/G2 originate from float Mathf.Sqrt; translating to double sqrt changes bits. Preserve operation precision/order before claiming deterministic seed compatibility. This SimplexNoise class mutates a static permutation scratch array, so parallel initialization needs isolation.

Map parameters include scene/game mode, terrain/vegetation biome, climate, size/height, river width/density, plain height, flatness, bridge height, ally count, mirror/rotation and picked line/level index. Cache hash includes these plus seed (`VFDataRTGen.cs:859`); a seed alone does not identify a world. Town generation seed is RandSeed + biome + size + riverDensity + riverWidth + plainHeight + flatness + bridgeMaxHeight, with offsets for biome/town/allies. Mine seed is RandSeed+1. Material painting loads Resources `RandomMapXML/tmpPaintVxMat` and associated XML with 79 as an argument (`:969`). Export those tables before claiming material parity.

A seed-stable hash-noise demo is a useful provisional C implementation but is explicitly **not** this generator. For matching worlds, first port the RNG/noise plus column classifier under golden-value tests, then cave/mineral/painting stages, then towns and vegetation.

## Binary formats and fixtures

All BinaryReader/BinaryWriter integer encodings here are little endian. C readers must decode explicitly, validate counts/length arithmetic and reject truncated input.

**Authored `.voxelform` files:** `V/VFDataReader.cs` gives complete reader and LZ4 bindings. File name `<prefix>_x<fileX>_y<fileZ>.voxelform`, or suffix `_<lod>.voxelform` for LOD>0. Header is a table of absolute u32 piece offsets, no observed magic/version field. Number of entries `(48>>lod)*(48>>lod)*(23>>lod)`. Final piece length uses file EOF. File-local piece index `((px%48)>>lod)*(48>>lod)*(23>>lod) + ((pz%48)>>lod)*(23>>lod) + (py>>lod)` (x,z,y ordering). A piece with length 2 directly stores a uniform volume/type pair. Other pieces are raw LZ4 blocks. Decompressed piece begins with **64 u32 offsets**, then chunk bodies. Chunk index is `16*((cx>>lod)&3)+4*((cy>>lod)&3)+((cz>>lod)&3)` (x,y,z ordering, unlike sample indexing). Last chunk ends at decompressed EOF. Body sizes accepted: 85,750 dense, or 2 uniform. Uncompressed maximum = 64*85,750+256 = 5,488,256 bytes. Map missing files are handled by source warnings; a modern reader should return an explicit error.

There are actual small fixtures: `TutorialMode/Mainland/map_x0_y0.voxelform` (~316 KiB), map LOD1–4 (~40 KiB down to 666 bytes), and water equivalents (LOD4 is 54 bytes: 36-byte nine-offset table + nine 2-byte uniform pieces). CustomGames includes several complete map/water sets. These assets make an importer milestone more faithful than inventing all world content. The initial source audit had not decoded these files; the implemented importer and measured fixture results are recorded below.

**Modified terrain save:** `V/VFVoxelSave.cs` format: i32 version **1606230**, i32 chunkCount, chunkCount dense 85,750-byte blobs, then chunkCount index entries `{i32 x,y,z,lod; i64 offset}`; optional subsystem extension follows. Index offsets refer to temporary data file layout, whose initialization/import paths must be checked when writing a compatibility importer. Source refuses hollow chunks when saving. Separate terrain and water records. Do not write new game saves in this format without regression fixtures; new C schema should be versioned, bounded and atomic.

**Generated `.terra` cache:** `V/VFDataRTGenFileCache.cs`: i32 magic **16102000**, path derived from MD5 of UTF-8 concatenated world parameters, up to 10 retained cache files. Each record: 6 i32s x,z,lod,height,bitMask,dataLen (24-byte header). Flags town=1, volume=2, noise=4, height=8, gradient=16, material=32. Volume body iterates z then x, each column writes u16 terrain byte length + terrain bytes, u16 water length + water bytes. Optional grids follow: double noise, float height, float gradient, byte biome, each 35². Cache is disposable, not the savegame authority.

**Archive container:** `Assets/GameArchive/Archive.cs`: `.arc`, header i32 version=5, 16 GUID bytes, i64 total file length (called checksum in source but **not cryptographic or content checksum**). `ArchiveIndex.cs`: .NET string filename (7-bit byte length prefix + UTF-8), bool yird, i64 begin offset, i64 length. Multiple subsystem records and world/yird-specific indexes mean an entire game save is more than voxels. `ArchiveMgr.cs`, `PeRecordReader.cs`, `PeRecordWriter.cs`, `SwapSpace.cs` coordinate these records.

## Construction representation

Construction uses a distinct grid and extractor under `V/Block45/`. `Block45Constants.cs`: 0.5m voxels, 8 logical cells, 1 prefix and 1 postfix (10³*2=2,000 byte dense chunks), 4-meter physical chunk size, max LOD4, 256 material count. `Block45Kernel.cs:2455` defines B45Block with byte blockType, byte materialType. Primitive `blockType>>2`; rotation `blockType&3` (quarter-turns). Do not model all blocks as cubes: kernel primitive tables include slopes/shapes and neighbor face culling.

Extendable blocks: root byte0 `(63<<2)|rotation`, byte1 `((length-2)<<2)|extensionAxis`; child byte0 `0x80|(primitive<<2)|rotation`, byte1 `(material<<2)|extensionAxis`. Rotating an extended structure also changes root location and extension axis, not only its primitive rotation bits. `Block45OctDataSource.cs`, `Block45OctNode.cs`, `Block45Man.cs`, `Assets/Block45/Scripts/BuildingEditOps.cs` handle storage and edits.

Voxel-creation vehicles/weapons and ISO artifacts are another subsystem; do not present Block45 as a replacement for the creation editor. Full creation format audit belongs to its own source modules and milestone.

## Networking contracts and dependencies

`Assets/Scripts/GameNetwork/` is uLink RPC/client architecture with typed custom codecs (`CustomCodecRegister.cs`), packet IDs (`PacketType.cs`), network entities and game/lobby/file-transfer clients. ClientConfig defaults lobby/proxy ports 12534/12535. No live server compatibility has been established by this static audit; a new ENet/other transport implementation would need a new protocol or a documented wire adapter, not direct source-level RPC substitutions.

`ChunkManager.cs` caches edits for unloaded chunks, applies updates on chunk availability, and dispatches terrain/block data and destruction RPCs. Terrain destruction sends Vector3 pos, float power, float radius. Dig RPC contains IntVector3 pos, float durability decrement/radius/height, bool returnItem. Change-terrain RPC includes position, radius, target type and bytes. Authoritative delta blobs are interpreted by `DigTerrainManager.cs:1162`: i32 count; per entry position and BSVoxel via BufferHelper, with additional datasource i32 for mixed building updates. Exact BSVoxel codec must be ported from `BufferHelper.cs` before claiming wire compatibility. That DigTerrainManager file is UTF-16; naive tools can show null-separated characters.

External native terrain dependency: `lz4_dll` exports legacy LZ4 compress/uncompress functions. New importer can use a maintained liblz4 raw-block decompressor with strict output limits. Mesh acceleration uses OpenCL wrappers; base C rebuild should work without GPU compute. Original runtime depends on Unity physics, assets/materials, threading and object lifecycle, managed third-party networking. Replace these boundaries explicitly rather than attempting to call MonoBehaviour code.

## Native boundary

Current ownership and acceptance are in [feature contracts](../FEATURE_CONTRACTS.md) and [PLAYTEST](../PLAYTEST.md). Native marching cubes replaces the initial tetrahedra prototype; source topology is checked across256 cases. Cropped gradients and native shaders remain explicit adapters. Noise/streaming/LOD/water, original archives/ISO and online compatibility remain separate gates.

BSVoxel delta entries are i32 XYZ plus two bytes Volume,Type (14 bytes), with count and optional datasource header. BufferHelper.ReadVFVoxel instead reads Type,Volume; select codecs by caller.

## Creation archive evidence

Additional inspection of `Assets/Scripts/ZhouXun/Voxel Creation/Scripts/Core/VCIsoData.cs` establishes an independent ISO serialization path. `Export` around line 741 writes the .NET length-prefixed string `VCISO`, then version, category, author/name/description/remarks, dimensions, icon length/bytes, material/decal/component/voxel/color counts. Material/decal slots carry GUIDs and optional embedded resource blobs. The compressed component/voxel/color payload uses `IonicZlib`; voxel entries store an integer coordinate key and a ushort packed voxel; color entries store integer keys and RGBA bytes. Import has version-specific branches, including author/remarks since `0x02020000` and a head signature check since `0x02020001`. This is a format outline, not a validated compatibility decoder; enforce bounded decompression and inspect each branch before implementing one.

`Creation/CreationDataIO.cs` computes CRC64 over resource bytes for identity and resolves both persistent creation and network-cache paths. In the non-Steam branch it validates this CRC before import. `CreationDataCalc.cs`, `CreationAttr.cs`, and `Mediator/CreationDataBuild.cs` govern calculated properties and construction into game objects. The `VCEditor` subtree contains the editing tools/UI; `Components/Parts` and `WhiteCat/PE-VC` contain functional part and controller behavior. Replication must distinguish the editable design, its exported resource, the resulting inventory item, and the live vehicle/weapon instance. No original ISO has been round-trip tested in this audit.

Additional codec distinction: `BSVoxel` edit blobs use an i32 count and then 14-byte entries (`i32 x,y,z` plus `Volume,Type`); mixed building updates prefix the datasource i32. `BufferHelper.ReadVFVoxel` separately reads `Type,Volume`. A compatibility importer must select the specific codec used at each call site rather than assuming one global voxel byte ordering. The creation exporter currently declares `ISO_VERSION = 0x02030001`, sixteen material slots and four decal slots; configured extensions are `.vciso`, `.vcres`, and `.~vcres`.


## Implemented authored-terrain importer

The C implementation is in [`pe_legacy.h`](../../rebuild/src/pe_legacy.h) and [`pe_legacy.c`](../../rebuild/src/pe_legacy.c), with [`test_legacy.c`](../../rebuild/tests/test_legacy.c). It uses system `liblz4` and `LZ4_decompress_safe` for raw compressed blocks. It does not depend on the original `lz4_dll` binary.

The reader follows the source ordering: file pieces X,Z,Y; chunks within a piece X,Y,Z; samples x-fast. `pe_legacy_sample` takes base-resolution file-local coordinates aligned to `1 << lod`. `pe_legacy_chunk` takes chunk coordinates in the selected LOD grid and expands uniform chunks to the full **85,750 bytes**, including the one-sample prefix and two-sample postfix halo. LOD 0–4 reads are supported. Reported extents respect integer truncation of the vertical piece count: LOD 4 covers height 2,048, while LOD 0 covers height 2,944.

Opening validates every piece offset, requires the first offset to match the table length, rejects invalid or oversized spans, and caps file length at `UINT32_MAX`, consistent with the original 32-bit offsets. A piece is read and decompressed on demand; all 64 chunk offsets and body lengths are checked before data is exposed. Memory holds an offset table, a compressed-piece buffer, and at most a **5,488,256-byte** decoded piece. The handle caches one piece and is intended for one caller at a time.

`pe_legacy_crop` imports a complete **64×32×64** region transactionally: failures leave the destination unchanged. It is restricted to LOD 0 because `PeWorld` currently has no sample-spacing field. Crops preserve the original volume and material bytes, set `source = 1`, `seed = 0`, `revision = 1`, and record the file-local origin. Raw materials must be interpreted through the original content tables, rather than the generated terrain's small material enum.

### Measured source fixtures

| Fixture | Observed data or useful crop |
| --- | --- |
| `TutorialMode/Mainland/map_x0_y0.voxelform` | 322,811 bytes; 52,992 pieces, of which 52,990 are uniform. Two compressed pieces, indices 0 and 3312. |
| Tutorial piece 0 | One dense chunk with 606 solid logical samples, bounded by x=6..25, y=0..3, z=2..21; raw solid material 1. |
| Tutorial piece 3312 | A dense chunk exists but contains no logical samples with volume at least 128. |
| Tutorial crop | Origin `(0,0,0)`. Surface discovery returns `(16,1,16)`. The default world center `(32,32)` is empty, so a fixed center spawn is unsuitable. |
| Tutorial sample column `(12,12)` | Volumes at y=0..3 are 137, 167, 177, 125; the interpolated surface is approximately y=2.9423. |
| Dragon Desert | `CustomGames/Dragon Desert/Worlds/Dragon Desert/map_x0_y0.voxelform`, 2,862,304 bytes. Surface anchor `(16,116,16)`; crop origin `(0,108,0)` produces an authored surface near local y=8–9. This crop is decoded and meshed in tests. |
| Cave on the island | `CustomGames/Story Demo/Worlds/Cave on the island/map_x0_y0.voxelform`, 36,137,572 bytes. Surface discovery returns `(16,217,16)`; `(0,209,0)` is a candidate crop origin. The surface search was executed; this crop is not one of the importer's fixed integration assertions. |

`TutorialMode/Mainland/WorldEntity.xml` places `scene_TrainingRoom` at `(12,1.5,12)`. Importing terrain does not instantiate that room. The cave world's entity XML similarly references base structures around `(385,99,407)` and other locations; these remain distinct scene objects.

`pe_legacy_find_surface` is a deterministic spawn/crop hint. It scans dense chunks for a supported solid-to-air transition, preferring central samples, with immediate horizontal support and two air samples above. It can miss unusual surfaces and surfaces represented only by uniform chunk boundaries. The frontend must still validate a complete crop and find capsule clearance; the hint is not a proof that the entire area is walkable. Automatic crop origins can be formed from the anchor with enough ground below and headroom above, then clamped to file extents.

### Validation and remaining limits

Independent strict C11 compilation with `-Wall -Wextra -Wpedantic -Werror` and AddressSanitizer/UndefinedBehaviorSanitizer passed for the importer test suite. Tests cover all ten tutorial map/water LOD files, the tutorial volume-177/material-1 sample, the Dragon Desert crop and mesh, raw material IDs 201 and 233, all chunk-index axes, LOD alignment, crops crossing chunk and piece boundaries, malformed/repeated offsets, bad LZ4, chunk offsets into metadata, truncated files, invalid LODs, and output preservation on failure. `PE_SOURCE_ROOT` selects the repository fixture root; tests enter through `pe_test_legacy()`.

The renderer now uses the original marching-cubes tables and source corner/edge ordering in `pe_mesh.c`, replacing the first slice's provisional tetrahedra. Interior gradients use the source's discrete negative density differences at neighboring samples, interpolated along each edge and normalized for the C renderer. The common source `/255` factor cancels during normalization. Crop-edge gradients are clamped because the bounded `PeWorld` does not retain samples outside its crop; shading at those edges is therefore not an exact original halo reconstruction. Source shader packing/full appearance remains outside importer qualification; recovered native terrain materials are documented in [terrain materials](SANDBOX_TERRAIN_MATERIALS.md).

Water-file decoding is byte-level support only. The original special `{128,type}` sea-plane interpretation, separate water field, flow, underwater rendering, and swimming are not implemented by this terrain importer. World entities, vegetation, streaming, LOD transitions, full story-world assembly, and original save/ISO compatibility remain separate work. The generated world's hash-noise terrain remains provisional and does not reproduce the legacy random generator.

A direct fresh terrain import contains no authored trees or starting tools. Inventory can be carried from an existing C save when importing another world; importing geometry alone does not establish a complete survival progression path. The packaged native Linux original V1.1.4 now runs locally with CPU extraction and antialiasing disabled; its tutorial scene, character, room, dialogue, minimap, and hotbar have been visually verified. This does not establish terrain or gameplay parity. No source Unity editor build has run. See the [reference runtime investigation](../REFERENCE_RUNTIME.md) for the controlled launch test, diagnostic evidence, historical distribution probes, and remaining editor dependencies.

## Tutorial geometry integration

[Training terrain](TRAINING_TERRAIN.md) owns the726-sample mound and32-sample construction anchors. [Training build](TRAINING_BUILD.md) owns shared half-meter B45 placement, dirt transactions, matching render/BVH and receiver cleanup. [PLAYTEST](../PLAYTEST.md#tutorial-native-graphical-fixtures) owns native12,000/15,000-frame observations. These do not qualify original world streaming, water or creation behavior.
