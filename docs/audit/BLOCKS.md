# Block45 construction replication

Verified against the supplied source, without executing Unity. Source prefix below is `Assets/Terrain/Voxelform2/Voxelform/Source/Scripts/Voxel/Block45/`.

## Representation and coordinates

- `Block45Constants.cs:1,30-33`: active `USE_HALF_METER_VOXEL`, scale **0.5m**, inverse scale **2**. `_shift=3` means 8 cells per chunk axis; one prefix and one postfix yields 10³ stored cells. Physical interior chunk edge is 4m. These are separate from 1m terrain density samples.
- `Cpu/Block45Kernel.cs:2455-2493`: `B45Block` is exactly two data bytes: first `(primitive<<2)|rotation`, second material ID. Primitive zero means empty; low two bits specify four Y-axis rotations. Shape1 cube bytes4..7; shape2 ramp bytes8..11. Extendable shapes use high-bit encoding and primitive63 root; exclude them from first implementation rather than decoding them as ordinary solids.
- `Building System/Scripts/BSDataSource.cs:88-100`: Block45 data-source offset is zero. Cell `(x,y,z)` occupies world box `[x/2,(x+1)/2] × ...`; negative coordinates must floor world×2. Do not carry terrain's -0.5m data-source offset into Block45.
- Kernel positions are integer local cell origin plus unit corner positions and are scaled externally by chunk transform. A native direct emitter can multiply both position and shape coordinates by0.5 once.

## Exact cube and ramp

`Cpu/Block45Kernel.cs:2263` corner IDs:

|ID|Unit position|
|---|---|
|0|(0,0,0)|
|1|(0,0,1)|
|2|(1,0,1)|
|3|(1,0,0)|
|4|(0,1,0)|
|5|(0,1,1)|
|6|(1,1,1)|
|7|(1,1,0)|

Cube shape1 external triangles (`EdgeFaceVertTable`, source1690 onwards): `501,540,430,473,723,762,612,651,310,321,467,456` (each group three corner IDs). 12 triangles, full0.125m³ volume.

Ramp shape2 external triangles: `501,540,430,512,310,321`; interior sloping face from `InteriorFaceVertTable:1000`: `423,452`. Eight triangles, volume0.0625m³. Solid region at rotation0 is `0<=x,y,z<=1` and **x+y<=1**; high side is x0, low edge x1. These source indices have outward cross-product normals, so they can be used with the native renderer's CCW culling.

`rotateCornerVertId:919` adds rotation modulo4 separately in bottom/top groups. Equivalent coordinate mapping from `rotate90:871`:

- r0 `(x,y,z)`
- r1 `(z,y,1-x)`
- r2 `(1-x,y,1-z)`
- r3 `(1-z,y,x)`

Ramp high sides therefore x0,z1,x1,z0. Normals transform by the linear part only. Implement ray clipping against six cube planes plus the ramp's x+y<=1 plane in inverse-rotated local space. AABB overlap is exact for this ramp: first overlap unit bounds, then clipped AABB minimum x + minimum y must be below1. This avoids the old full-cube ramp proxy. Emit independent cells initially; legacy face-mask neighbor culling is a later optimization, not necessary for solid geometry correctness.

## Resource mapping and transactional costs

`Building System/Scripts/BSBlockMatMap.cs:28-43` reads **blocktype** (`matid`,`itemid`). This is distinct from smooth-terrain `buildblock` and from mining `resource`:

`0→268 Dirt,1→269 Marble,2→270 Limestone,3→271 Iron,4→272 Steel,5→273 Copper,6→274 Silver,7→275 Gold,8→276 Aluminum,10→278 Sand,11→279 Wood,12→280 Stone,13→281 Diamond,14→282 Glass Small,15→1401`. Material9 has no mapping. Original kernel's MaterialGroups accepts0..15, but missing resource9 should be rejected by the first gameplay construction path unless a god-mode policy is explicit.

`Building System/Scripts/PEBuildingMan.cs:301-406` groups ordinary added cells by item ID, then uses divisor `1 << BSBlock45Data.s_ScaleInverted`, which is **4**, not8. Each resource debit is **ceil(number of cells /4)**. Removal lines448-505 groups old nonempty cells and refunds **floor(number of cells /4)**. It counts cells independent of geometric filled volume: ramp and cube each count one. Per-operation rounding is observable: adding four cells together costs1, adding four individually costs4; removing four together refunds1, removing individually refunds0. Do not introduce persistent quarter-item credit without labeling the behavior change. Extendable root/member exceptions and replacement/undo grouping need separate follow-up tests.

The implemented cube/ramp module exposes these rounding helpers. Gameplay groups each request by material, validates all placements and inventory debits before mutation, and commits the batch atomically. The single-cell mode costs one resource and refunds zero; the four-cell brush costs one resource for a uniform material and refunds one when all four cells are removed together. Furniture placement remains separate: fence/ladder prefab items are not converted into Block45 material cells.

## Implemented native module

`rebuild/src/pe_block.h` and `pe_block.c` implement the source-backed cube/ramp subset. `PeB45Cell { int x,y,z; uint8_t type,material; }` stores half-meter cell indices and the original packed bytes. The module uses `PeVec3` and `PeVertex` without a gameplay or renderer dependency.

- `pe_b45_valid` accepts cube/ramp encodings and mapped materials; empty, extended, other primitives and material 9 are rejected.
- `pe_b45_material_item` and `pe_b45_item_material` map the original `blocktype` table in both directions; unknowns return -1.
- `pe_b45_build_cost` and `pe_b45_remove_refund` perform overflow-safe per-operation rounding.
- `pe_b45_mesh` emits outward-wound unculled world-space triangles: cube 36 vertices, ramp 24. Each vertex includes its normal and original material.
- `pe_b45_contains` includes boundaries. `pe_b45_overlaps_aabb` uses the actual ramp solid and excludes mere face contact, with a small floating-point tolerance.
- `pe_b45_raycast` normalizes its direction and returns world-space distance, point and normal; rays starting inside return the exit surface.

## Gameplay and save integration

`rebuild/src/pe_game.c` keeps a separate construction-cell collection alongside the earlier one-meter furniture/compatibility proxies. `pe_game_build_cells` and `pe_game_remove_cells` apply atomic batches, resource checks, reach/bounds checks, occupancy checks and grouped rounding. `pe_game_place_shape` and `pe_game_remove_shape` select nearby surfaces and implement the single-cell or aligned 2×2 four-cell brush. Shapes retain all four source rotations. Player AABB collision and construction ray targeting use the same cube/ramp geometry as rendering; ramps therefore no longer collide as full cubes.

Frontend controls are in [build/run](../../rebuild/README.md#sandbox-controls). The construction prefix introduced in PECSv3 remains in the current append-only save. Validation rejects unsupported/out-of-bounds/duplicate cells and invalid overlaps; legacy v1/v2 proxies retain compatibility. This is not original Unity archive compatibility.

Surface selection uses the nearest terrain, furniture or construction hit. A regression found that a farther terrain hit could overwrite placement height after furniture had already won the ray test. This is fixed: terrain height adjustment runs only when terrain is the selected surface. A test aims from a jumping-player position at furniture with terrain behind it, verifies the cell lands atop the furniture, and then verifies another cell lands atop that new cell, with the correct material charges.

## Validation and remaining scope

Regression proof: [test_block.c](../../rebuild/tests/test_block.c).

`rebuild/tests/test_game.c` additionally covers transactional grouped costs/refunds, mixed materials, duplicate requests, insufficient inventory, terrain/player/reach rejection, rotated cells, a constructed wall blocking movement, refund-capacity failure preserving cells, version 3 round trips and invalid save geometry. The furniture-top targeting regression passed the gameplay suite with ASan/UBSan. The complete build and graphical smoke checks were run again after integration.

This is a two-primitive implementation. Other original primitives, extended blocks, original face-mask neighbor culling, brush boolean operations, general history beyond the bounded [Adventure journal](CONSTRUCTION_HISTORY.md), building ISO import and multiplayer validation are not implemented. Terrain-versus-construction screening currently samples points; it is not an exact continuous intersection solver. The core AABB/step-up checks and the host capsule sweep remain native adapters, not the original character controller. Primitive visual colors replace original material textures, and furniture collision still uses one-meter proxies. These limits must remain distinct from the implemented source geometry, encoding and cost rules.

## Tutorial receiver integration

[Training building](TRAINING_BUILDING.md) and [training build](TRAINING_BUILD.md) own the752/753 receiver puzzle, source50-ray scoring, visual recovery and native geometry/cleanup. [PLAYTEST](../PLAYTEST.md#tutorial-native-graphical-fixtures) owns the15,000-frame result. This core module does not enable Tutorial undo/redo or the creation editor. Adventure's five-action journal is [construction history](CONSTRUCTION_HISTORY.md).
