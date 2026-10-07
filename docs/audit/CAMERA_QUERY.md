# Camera query backend

Implemented in C on 2026-10-07. This is a native physics adapter for the source-qualified [GeometryClamp evaluator](CAMERA_GEOMETRY.md); it does not establish Unity PhysX cooking, renderer or physical camera acceptance. It resolves the query prerequisites identified in [the geometry gate](CAMERA_GEOMETRY_GATE.md). Host projection and display ordering are qualified separately by the host integration checks.

## Evidence and ownership

`export_camera_query.py` reads the existing recovered route manifest and the original installed asset bundles offline with UnityPy. It verifies each manifest bundle SHA256, collider type, enabled/nontrigger status and owning GameObject. It reads the actual GameObject layer and MeshCollider convex flag. Generated `pe_camera_query_source.json` retains the bundle hashes, source IDs and the original `WhiteCat/PE-VC/Layer.cs` hash; the exporter guards the supported named layer numbers. No original game or Unity editor launch is involved.

The complete route contains 16 source collider components across 12 building instances, with 4,343 exported collision triangles. The older starting crop contains six of those components and 920 triangles. The recovered components are on Building13 or Unwalkable16; all are admitted by source mask8468481. This mask also admits Default0, SceneStatic11, VFVoxelTerrain12 and ProxyPlayer23. Trees14/21 are excluded. These are the colliders present in the qualified export, not a claim that every collider in a procedural world has been recovered.

`pe_scene_load_obj` now retains `entity_<id>_component_<id>_sub_<id>` identities. Submeshes belonging to the same source collider share a group; separate components remain separate. The existing triangle IDs, object statistics, triangle geometry, motor sweep and ordinary ray implementations are retained. New group data belongs to the immutable `PeSceneCollision` and is destroyed with it.

`pe_sandbox_collision_scene_count/scene` provide read-only borrowed terrain, construction and static scenes from a ready snapshot. Trees and synthetic motor boundary planes are absent from this visitor. The caller must refresh the ordinary snapshot after terrain/construction/world changes; the query adapter neither rebuilds nor owns it. No new saved state exists.

## Explicit native query policy

`pe_camera_query` is the `PeCameraGeometryQueryFn` callback. Its context borrows one current `PeSandboxCollision`. It accepts only the qualified camera mask and ignored triggers. Unsupported static identities, invalid requests, an unready snapshot or allocation failure return false without changing the output. A successful miss returns `{false, 0}`. The adapter continues validating later scenes after an early hit, so an unknown collider cannot be hidden behind an earlier successful result.

- Surface overlap is inclusive sphere-to-triangle contact, including touching. The broadphase AABB is never treated as a filled concave building. Source boxes, source convex MeshColliders and native construction cubes/ramps/proxy boxes additionally have closed convex interiors. Concave static meshes and terrain remain surfaces; a building room is not filled by its overall bounds. Nearby Unity documentation about overlap bounds does not establish identical behavior in the installed Unity5.2 player.
- Sphere casts first identify initial contact/interior groups and exclude those entire groups, then find later eligible obstacles. They do not discard the whole scene. A ray beginning inside a qualified convex solid similarly excludes that solid. Rays and lines use double-sided exported triangles; casts use bounded conservative advancement against those same surfaces, with a 0.00001m contact tolerance. These are native numerical/query policies, not PhysX equivalence.
- Static groups are original component identities. Native terrain uses one existing Marching Cubes mesh region per group. Native construction uses one B45 cell or proxy box per group, including each cell's complete cube/ramp surface. This grouping is not claimed to reproduce Unity terrain chunks or construction collider cooking.
- Synthetic cropped-world motor walls/floors do not block the camera. Actual exported/generated terrain surfaces do. Queries may begin or end outside the crop, within the existing finite coordinate bound of one million units. There is no invented outside-crop terrain, infinite wall or sky collision.
- All queries use the existing BVHs. Distant region roots reject before scratch allocation or triangle traversal. The installed route has at most16 groups in any scene, so both policy and initial-contact scratch use bounded stack storage without per-query heap allocation. Scenes with more than64 groups use a checked temporary allocation. Convex containment scans only after that group's bounds contain the query origin; ordinary sphere surface/cast tests use BVH candidates.

The pure evaluator retains its own finite callback validation, transactional preview/commit and one-step rise clock. Backend preview queries do not advance gameplay, mutate a snapshot or commit a camera clock. Player movement retains its existing initial-contact behavior, separately tested against the camera's initial-group exclusion.

## Verification

`test_camera_query.c` covers source submesh grouping, inclusive touching, initial-group exclusion followed by a later hit, disabled groups, line endpoints/zero length, malformed request rollback, convex interiors, tree exclusion, crop-wall differences, a later unknown static group after a valid earlier overlap, immutable snapshots and repeated evaluator previews. Optional command-line OBJ inputs additionally exercise ray, overlap and cast requests against every actual installed static collider group.

Focused optimized and ASan/UBSan checks with leak detection pass for the starting and route collider sets. The existing scene and sandbox collision regression suites pass with zero failures, preserving movement and perception behavior. `test_camera_query_export.py` passes five tests with UnityPy and zero skips: exact regeneration/layers, changed bundle hash, wrong owner, wrong collider type and missing collider rejection. The ordinary interpreter may skip these installed-asset tests; that is not source verification. Exporter `--check` passes with the qualified UnityPy interpreter.

The ignored CPU benchmark is `rebuild/build/camera-query-check/bench.c`, with output `result.log`. It loads the untouched `build/vegetation-check/examples.pesave`, builds its actual416×64×416 route snapshot (387,048 triangles), then performs300 repeated16:9/FOV60/near0.8 geometry previews and300 activity evaluations at each of five anchors. Setup is timed separately: this run loaded in2.65s and built the snapshot in2.58s. Saved-player, welcome, hunt-return and hunt-travel geometry averaged0.085–0.087ms with five queries each; the supplier obstruction averaged0.614ms with29queries. The separate66-ray activity calculation through this stricter backend averaged1.44–1.82ms. Combined sampled averages were below2.44ms; these are local CPU measurements, not an enforced frame-time bound (activity scheduler outliers reached8.02ms). The host retains its existing simpler, tree-excluding activity backend, so these66-ray timings are a conservative comparison rather than the selected host activity cost. All result fields were finite; game bytes, voxel bytes and snapshot generation/rebuild counts remained unchanged.

Useful focused commands from repository root:

```sh
rebuild/build/unity-tools/bin/python rebuild/tools/export_camera_query.py --check
rebuild/build/unity-tools/bin/python rebuild/tests/test_camera_query_export.py
```

Host integration owns Makefile targets and host/renderer checks. No desktop or graphics launch was used for this backend qualification.
