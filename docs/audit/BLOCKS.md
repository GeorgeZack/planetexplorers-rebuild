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

The frontend renders cells through `pe_b45_mesh`. V changes cube/ramp, T rotates, B changes single/four-cell brush, right mouse places, and X removes using the selected brush. Left-click mining on a construction cell removes one cell, so it follows the zero-refund single-cell rule. Fence/ladder items remain their earlier separate furniture representations.

Native save version 3 adds construction cells with packed type/material bytes and half-meter indices. Versions 1 and 2 remain readable, preserving their earlier one-meter proxies. Load validation rejects out-of-bounds or unsupported cells, duplicate occupied coordinates, player/furniture overlaps and sampled terrain intersections. Equipment state, durability, stamina, RNG state, pending craft deliveries and imported-world metadata continue to survive save/load. This format remains independent of original Unity saves.

Surface selection uses the nearest terrain, furniture or construction hit. A regression found that a farther terrain hit could overwrite placement height after furniture had already won the ray test. This is fixed: terrain height adjustment runs only when terrain is the selected surface. A test aims from a jumping-player position at furniture with terrain behind it, verifies the cell lands atop the furniture, and then verifies another cell lands atop that new cell, with the correct material charges.

## Validation and remaining scope

`rebuild/tests/test_block.c` verifies the original triangle positions, cube/ramp signed volumes 0.125/0.0625 m³, all four rotations, unit outward normals, ramp point occupancy, noncollision above the sloping half, exact sloped ray normals, translated/negative cells, invalid encodings, material mapping, and rounding for counts 0, 1, 3, 4, 5, 8 and the maximum 32-bit count. The isolated module passed strict C11 compilation, AddressSanitizer and UndefinedBehaviorSanitizer.

`rebuild/tests/test_game.c` additionally covers transactional grouped costs/refunds, mixed materials, duplicate requests, insufficient inventory, terrain/player/reach rejection, rotated cells, a constructed wall blocking movement, refund-capacity failure preserving cells, version 3 round trips and invalid save geometry. The furniture-top targeting regression passed the gameplay suite with ASan/UBSan. The complete build and graphical smoke checks were run again after integration.

This is a two-primitive implementation. Other original primitives, extended blocks, original face-mask neighbor culling, brush boolean operations, the full undo/redo system, building ISO import and multiplayer validation are not implemented. Terrain-versus-construction screening currently samples points; it is not an exact continuous intersection solver. Player movement uses an AABB with step-up behavior, not the original character controller. Primitive visual colors replace original material textures, and furniture collision still uses one-meter proxies. These limits must remain distinct from the implemented source geometry, encoding and cost rules.

## Appendix: tutorial752/753 receiver puzzle

Read-only source audit: `TrainingRoomLoader` instantiates `TrainingManager_New` without the room mesh's separate `(12,1.5,12)/−90°` override. Geometry below is therefore the prefab hierarchy's world coordinates. Prefab SHA-256 is `7a20d53f94a4f2531bd75e705c3dea9d0523e601d43749250e62134c7eb3369e`. Serialized `EmitlineTask` component11430344 references line group Transform464894 and receiver group GameObject100952. The optional asset exporter now verifies all50 beam hierarchy transforms, both receiver mesh hierarchies and collider hierarchies against installed `resources.assets` root120048. It also compares receiver collider sizes/centers/enabled/trigger flags and layer12. Custom MonoBehaviour mission fields remain repository-source evidence.

**752 is an actual mode-entry event.** `Quest_List` gives9000 dirt268 on acceptance, target4105, opening4074/300244 and plot437→300245. `InitInBuildScene` subscribes to `MainPlayerCmpt.onBuildMode`; the first `true` callback unsubscribes and completes752. `Action_Build.DoAction` sets its mask/animation and sends `Build_BuildMode(true)`; `MainPlayerCmpt` changes the UI/camera mode before emitting the callback. Opening a build menu or merely checking an already-true flag is not that event. Completion dialogue300246/4075/4076 accepts753.

**753 grants no items** and has target4106, opening300249, plot438→300247/300248, and successor756. `InitScene` creates32 smooth-terrain samples with density255/material1: `(x17..20,y1,z10..13)` and `(x10..13,y2..5,z9)`. These are density samples meshed with the existing terrain kernel, not half-meter construction cubes. It activates50 beam scripts and two receivers.

| Beam set |25 emitter positions, meters|World right/direction|Query origin|
|---|---|---|---|
|Upright receiver|x=10.5..12.5, y=2.5..4.5, both step.5; z=13.5|−Z|same x/y, z=28.5|
|Flat receiver|x=17.5..19.5, z=10.5..12.5, both step.5; y=5.5|−Y|same x/z, y=20.5|

Serialized quaternion reconstruction includes small floating-point deviations (up to several micrometers) from these nominal coordinates; preserve full matrices in recovered data when matching exact source transforms. `EmitlineRayCaster.Update` uses `Physics.Raycast(P−right×15,right,20,1<<12)`. The query thus starts15m behind the visible emitter and must not be shortened to only the visible four-meter beam. Only layer12 (`VFVoxelTerrain`) participates; Block45 chunk colliders use that layer. Room/player colliders on other layers must not become occluders merely because they are in the native collision scene.

Receiver0, `col_horizontal` Transform483900, is physically upright: center approximately `(11.5,3.5,9.5)`, world size `(3,3,.2)`. Receiver1, `col_vertical` Transform433338, is physically flat: center approximately `(18.5,1.5,11.5)`, world size `(3,.2,3)`. Both are enabled, nontrigger BoxColliders with local size `(3,.2,3)`, local center0 and layer12. Names alone are misleading; use the transforms.

A nearest hit on **either receiver** is red and contributes−100. Any other layer12 hit **or a ray miss** is green and contributes+1. `EmitlineTask.Update` resets score; ray scripts contribute; `LateUpdate` completes once if score>0. With50 active beams, this requires all50 to avoid a receiver. There is no placed-block count, required shape, material-specific test or sustained hold timer. The source relies on Update ordering for reset/accumulation; a native evaluator should explicitly evaluate one complete beam snapshot per tick. It must test nearest-hit ordering, one remaining red beam, misses and irrelevant layers.

Visual assets use `Emitline`, `ReceiverTrue` and `ReceiverFalse` materials, line geometry in local coordinates, and per-receiver hologram material/render-camera effects. `EmitlineAppearance` fades over.3s, sets line tint alpha to progress×.4 and indicator tint from progress. Receiver appearance components11423338/11441388 use fadeTime.3, part1time.5 and minwidth0; their rendered geometry scales in two stages while independent receiver colliders remain present. A simple native line/red-X/green-ring treatment can preserve puzzle feedback, but must be identified as a rendering approximation until the original shader/camera path is recovered.

The smallest faithful construction step is an isolated training collection of existing `PeB45Cell` cube/ramp geometry, placement/removal rays and player collision, with material0 consuming the granted dirt under the existing per-operation ceil(cells/4)/floor(cells/4) rules. Evaluate these actual triangles/solids and the32-sample terrain against the two receiver slabs on every beam tick. Success must arise from those geometric results. Other original brushes, ISO editing and full construction animation can remain explicit later scope.

On753 completion `TrainingTaskManager` calls `CloseMission_buildPoint` and `CloseMission_buildCreateISO`: hide lines/receivers, clear recorded Block45 write coordinates, clear the32 terrain samples, and end the Build action. The source subscribes globally to `BSBlock45Data.voxelWrite`, records every edited coordinate and writes air on cleanup; it does not refund materials or restore previous cells. An isolated training construction collection avoids accidentally applying this destructive cleanup to unrelated sandbox blocks. The source completion bridge proceeds to756; that later lesson needs its own audit rather than being treated as an already supported generic build objective.

### Optional receiver visual recovery

`rebuild/tools/export_training_build_assets.py` uses optional UnityPy1.25.4 and creates a fresh output directory without modifying original assets:

```sh
/tmp/pe-asset-tools/bin/python rebuild/tools/export_training_build_assets.py
python3 rebuild/tests/test_training_build_assets.py
```

Default output is `rebuild/build/reference-assets/building`. `receiver-upright.obj` and `receiver-flat.obj` each preserve the installed built-in cube10202 (24vertices/12triangles) under the exact authored world transform. Draw them at world origin with unit scale. Their source `Receiver` material has no diffuse texture: the exported white `_Color` is only a diffuse fallback for a procedural shader, not visual parity. The two source org-receiver nodes are also the objects misleadingly named `recieverBase`; there is no separate machinery base mesh.

`effect-true.obj` and `effect-false.obj` recover installed mesh6654 used by the source mesh-mode ParticleSystemRenderer, with separate original circle/X texture materials. These OBJ files remain in **particle mesh local space**, unlike the world-space receiver files. `manifest.json` includes the particle child TRS, InitialModule/shape/rotation data, renderer fields and all50 line matrices; the consumer must implement particle placement/orientation or clearly use an approximate marker treatment. There is no static emitter-body mesh. The line texture is exported separately as well. No particle simulation, additive shader, hologram camera, fade animation or frontend graphics validation is asserted by this export.

Every selected mesh/material/texture has its installed object file/path ID and raw-object SHA-256 recorded; the repository prefab has a whole-file SHA-256. Three headless tests cover normalized source queries and their15m offset, guarded atomic publication/fresh outputs, and the optional real50-beam/four-mesh/material fixture. Source and installed transforms match within1e-5, while OBJ geometry retains the original full serialized transform calculation.

A second independent export to a fresh temporary directory produced byte-identical hashes for all12 published files (OBJ/MTL/PNG/manifest). This verifies repeatability for the installed fixture, not shader or graphics parity.

### Native visual integration review

The installed marker materials2264/2265 reference `resources.assets` Shader9035, named `AngryBots/FX/Additive`. Its retained compiled shader text explicitly specifies `ZWrite Off`, `Cull Off`, `Blend One One`, and fragment output `texture(_MainTex) * _TintColor`; there is **no alpha cutoff**. `EmitlineAppearance.SetNewColor` supplies completed marker tint `(.5,.5,.5,.5)`. The frontend now uses a dedicated fragment shader multiplying the recovered texture by `vec4(.5)`, additive `ONE/ONE` blending, disabled depth writes and disabled culling. It restores ordinary alpha blending, depth writes and culling after each marker draw. The earlier shared foliage cutoff `.25` has been removed; it was not a source threshold. Source marker orientation is wall180°Y or floor90°X in the native axis convention, with front normals pointing−Z/−Y; original `Cull Off` is necessary to view the same source mesh from the opposite side. Depth testing stays enabled. Marker shader creation depends on marker availability, independently of gathering assets. Every used marker material must have a nonzero, nondefault recovered diffuse texture or startup fails, preventing silent untextured marker quads.

The simple native line remains an approximation of original textured LineRenderer geometry. For ordinary hits beyond the visual emitter, placing a marker `.01m` before the hit matches source. However, the source uses unsigned distance from emitter even when the query hits within its15m pre-emitter segment, and uses the default hit point on a miss. The native frontend now mirrors that unsigned-distance formula, including pre-emitter hits and the default zero hit point on misses. These endpoint artifacts do not change the independently evaluated layer12 success predicate. Receiver procedural/hologram shading, textured line width/glow, fade timing and the particle lifecycle remain unported; their presence must not be inferred from the recovered marker shader.

The initial building exporter used descriptive file headers that the strict native OBJ/MTL loader rejected. Exporter and current outputs were corrected to the established canonical headers; three Python tests now check the contract, and a headless C invocation of the actual `pe_reference_validate` accepted all four meshes (12/12/2/2triangles). Full coordinate-space and shader limitations remain in the manifest instead of overloading loader header syntax. This review does not claim a completed graphics smoke; that run is owned by the parent integration task.


### Native construction playthrough result

The updated 15,000-frame graphical run passed with exit 0 at mission 756. It performed 19 actual additions and one removal, reached receiver score 50, conserved 8,987 dirt and cleared both lesson construction and anchor terrain. [Construction capture](../reference/rebuilt-training-building.png). This verifies the integrated C path; original-client construction parity remains unobserved.
