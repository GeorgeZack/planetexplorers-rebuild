# Native tutorial mound and collision snapshots

`pe_training_terrain.c/h` implement the exact726 authored samples from `TerrainDigTask.CreatTerrian`, with no generated ground or invented filler. This supplies real mutable terrain and mesh collision for lesson751; tool action clocks and inventory receipts belong to the separate mining host.

## Source data and coordinates

`tools/export_training_terrain.py` uses the existing bounded C# literal/loop/SafeWrite interpreter to generate `generated/pe_training_mound.h`. It independently interprets `DestroyTerrian` and requires precisely the same coordinates, all cleared to density/material0. Unknown source statements, changed counts or mismatching cleanup fail generation. The generated header records the original C# SHA-256.

```sh
python3 rebuild/tools/export_training_terrain.py --check
python3 rebuild/tests/test_training_terrain_export.py
```

`pe_training_terrain_init` initializes a64×32×64 `PeWorld` to air and applies all726 samples, using origin`(-8,-4,-8)`, source1, raw material1 and revision1. Seed751 is an explicit lesson identity tag, not a terrain-generator seed. Authored absolute bounds are x−2..15,y0..3,z2..21; the chosen origin retains an air halo on every side and includes the negative-X samples. Layer counts are360/288/59/19. This bounded volume supplies the scripted mound only; it does not claim to recover unrelated original world terrain.

Source prefab `(12,1.5,12)` and its yaw are **not** applied to these samples: source SafeWrite coordinates are already absolute. Mesh vertices remain volume-local. Render and BVH construction add origin exactly once. World/local helpers reject foreign lesson identity or nonfinite positions with NaN. The room-space ray/dig wrappers translate hit positions back to room coordinates while keeping selected sample x/y/z as **local volume indices** for edits and resource receipts. Dig-target `position` is generally the sample-box hit surface, not the snapped sample center; tools must use selected x/y/z.

`pe_training_terrain_apply` overlays only authored coordinates. `clear` sets only those coordinates to air, preserving unrelated samples and never restoring an invented previous floor. Both advance revision once only when contents change; repeated identical calls succeed without another revision. Foreign identity and revision overflow reject atomically. Initialization intentionally resets the owned volume; callers wishing to preserve other volume contents use apply/clear instead.

## Same-mesh collision

`pe_scene_from_mesh(const PeMeshData*,origin,error,capacity)` copies a triangle list into the existing headless scene BVH. It applies origin to the same rendered marching-cubes positions, preserving an independent immutable collision snapshot. It needs no temporary OBJ, filesystem I/O or graphics context. Normals/materials are not collision inputs. Vertex count is bounded to one million, triangle lists must be divisible by3, positions and translated coordinates must be finite and within the existing million-meter limit. Nonempty all-degenerate meshes fail. An empty mesh succeeds as a valid empty scene, whose ray and capsule queries report no collision.

`pe_training_terrain_collision` is a convenience builder that creates the current MC mesh, constructs its translated BVH and replaces an initialized caller-owned snapshot pointer only after success. Failure preserves the old snapshot. A renderer that already built the mesh should call `pe_scene_from_mesh` directly to avoid duplicate meshing. Rebuild on actual volume revision changes; snapshots retain copied geometry rather than a live pointer to the world.

The training integration supplies this scene as borrowed optional movement collision and chooses the earliest room/tree/mound sweep. It must refresh that pointer after replacing a snapshot. Capsule overlap and continuous sweeps use exact mesh triangles through the existing BVH algorithms; the provisional sandbox cylinder-density sampler is not used for the mound.

Spawning a mound around an already intersecting player still requires a deliberate safe-position/depenetration policy. Triangle overlap detects surface intersection; it is not an arbitrary closed-solid point-containment test. Continuous motion prevents entry from outside, but does not automatically recover any initial position buried inside freshly inserted terrain. A bounded clear/sweep search on751 entry can handle that case without inventing a flat collider or changing authored voxel positions.

## Verification

`tests/test_training_terrain.c`, entry`pe_test_training_terrain`, passes strict C11 warnings-as-errors plus ASan/UBSan/leak checks. Fixtures verify exactly726 nonair material1 samples, all four layer counts, irregular boundary densities, negative-coordinate translations, same-MC ray/BVH agreement, real capsule overlap/sweep on the mound, source-box dig targeting, no-op revisions, unrelated sample preservation, empty collision after clearing, foreign identity rejection and overflow rollback.

Additional `test_scene_collision.c` fixtures verify translated mesh construction, copied-buffer ownership, finite/count/index-list bounds, all-degenerate rejection and valid empty-scene behavior. The existing actual56446-triangle room collision fixture also passes the standalone sanitizer run. Four Python tests verify the committed generated source fixture, create/clear coordinate agreement for mining and construction anchors, and changed-schema rejection. These tests are headless; the separately completed12000-frame mining route is recorded below.

## Construction-anchor extension

The same lesson volume can now overlay or clear the32 authored `EmitlineTask` samples via `pe_training_terrain_build_anchors(world,clear)`. They are generated from source create/destroy methods with equal-coordinate/zero-cleanup validation: floor x17..20,y1,z10..13 and wall x10..13,y2..5,z9, volume255/material1. Existing samples outside those coordinates survive. The same origin, revision, MC rendering and BVH contracts apply; these anchors must not be replaced with preplaced Block45 cubes. See [construction adapter](TRAINING_BUILD.md) for source grid targeting and transactional native cells.

The native12000-frame mining route subsequently passed through752 with five actual shovel hit events, three removed samples,9006 dirt and the second shovel's independent durability retained. This verifies the integrated mining route, not later construction or full original terrain parity.

The construction frontend now uses these32 anchors through the same render/MC-BVH cache and source-style occupied-grid target query. Receiver and placed-cell triangles form a separate combined collision snapshot; movement selects the earliest room, terrain or construction contact. Entry recovery now considers both newly enabled anchor and receiver geometry. The integrated headless suite passes, including the formerly clear-of-anchor but overlapping-receiver placement case. The updated15000-frame graphical construction route exited successfully at mission756 after19 placements and one removal, with receiver score50 and zero remaining construction cells or terrain triangles. Final dirt8987 matches the source single-cell debit/no-refund rule; wood11, two shovels and their separate durability3528/3553 were retained. The [native construction screenshot](../reference/rebuilt-training-building.png) records the integrated scene. This verifies the native automated route and cleanup, not original Unity building parity or later creation/ISO behavior.
