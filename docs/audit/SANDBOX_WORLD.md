# Sandbox world: source contracts

This audit holds the source contracts for world identity, modes, terrain, collision, mining, building, camera and the shared tree asset. Implemented behaviour and its binding rules are in [FEATURE_CONTRACTS](../FEATURE_CONTRACTS.md). The original fixtures are in [SANDBOX_ALIGNMENT](SANDBOX_ALIGNMENT.md). Formats and generation are in [WORLD](WORLD.md), terrain geometry parity in [TERRAIN_PARITY](TERRAIN_PARITY.md), and building in [BLOCKS](BLOCKS.md).

## Adventure and Build are distinct modes

- **Map configuration.** [RandomMapConfig.cs](../../Assets/Scripts/PuJi/Town/Tool/RandomMapConfig.cs) defines Adventure and Build scene modes and the game types Cooperation, VS and Survive. Defaults: seed 666, terrain height 128, map sizes 40, 20, 8, 4 and 2 km square, boundaries centred at ±20,000. The Story constants 18,432×2,944×18,432 are not the Adventure extent. Biome, rivers, plain height, flatness, bridges, rotation and mirroring, generation lines and allies are all part of world identity, and some of them change towns, minerals and vegetation even with the same seed.
- **Starting supplies.** [SingleGameInitData.cs](../../Assets/PeWorld/SingleGameInitData.cs): Adventure gets item 1289 and 500 money, plus 1743 when `openAllScripts` is set. Non-demo Build gets item 1291 and 500 money.
- **Build debit lock.** [SingleGame.cs](../../Assets/PeLauncher/SingleGame.cs) `SingleGameBuild.LoadBuild` sets `PlayerPackageCmpt.LockStackCount=true` (line 556); Adventure sets it false (line 480). `PEBuildingMan.OnCheckVoxelModify` still checks `pkg.GetItemCount` against the grouped rounded cost before calling `pkg.Destory`. With the lock set, [PlayerPackageCmpt.cs](../../Assets/Scripts/PeEntity/Cmpt/PlayerPackageCmpt.cs) lines 220–248 return success without reducing inventory. This is neither `PEBuildingMan.IsGod`, which skips the checks entirely, nor the multiplayer `PeGameMgr.unlimitedRes`. Keep the three separate, and keep the consumption policy owned by the mode.
- **Build clock.** [PlayerTypeLoader.cs](../../Assets/PeLauncher/PlayerTypeLoader.cs) `InitNew` sets Build to day 94.50 with `Timer.ElapseSpeed=0` (13:00 in a 26-hour day), and Adventure to day 94.35 at speed 12. Only the world clock freezes: `GameTime` still advances `PlayTime` at speed 1, and movement and action timers keep running. Do not emulate this with the pause flag.
- **Terrain path.** Build loads `LoadRandomTerrainWithoutTown`, Adventure the with-town path.
- **Observed in the original** ([SANDBOX_ALIGNMENT](SANDBOX_ALIGNMENT.md#live-single-player-build-comparison)): in Build, dirt stayed at 9999 after a successful placement and the clock stayed at 13:00. In Adventure, the same kind of placement took dirt from 6 to 5.

A useful original comparison must record: version, mode and game type, the seed string and its resolved integer, every map setting, the avatar, the starter package, source coordinates, camera mode and FOV, input duration and game time, and graphics settings.

## Terrain generation and coordinates

- **Generator.** The authority is `Assets/Terrain/Voxelform2/Voxelform/Source/Scripts/Voxel/VFDataRTGen*.cs`. It seeds separate terrain, cave, mine, river and biome noise families from fixed offsets, with a cache key derived from the full configuration. Town and mine seeds come from `RandomMapConfig`.
- **Noise.** [SimplexNoise.cs](../../Assets/Scripts/System/SimplexNoise.cs) uses legacy `System.Random`. Its permutation swaps each index against `Next(0,255)`, which excludes 255. Its double constants embed float `Mathf.Sqrt` results. Exact densities depend on that precision.
- **Chunks.** Original terrain uses 32-cell chunks of 35³ samples (a 1-sample prefix halo and a 2-sample postfix halo) with LOD 0–4.
- **Importer.** [pe_legacy.c](../../rebuild/src/pe_legacy.c) validates `.voxelform` offsets and LZ4 and expands uniform and dense chunks at LOD 0–4; cropping is LOD 0 only.
- **Coordinates.** A global-world adapter must define the coordinate conversion once for terrain, blocks, actors, rays and saves, using floor division and modulo for negatives.
- **Streaming rules.** Key chunks and freeze their inputs. Results carry chunk identity and revision, so a stale job cannot overwrite a later edit or a reused slot. Preserve edits outside the resident set. `.terra` caches are disposable, not save authority. Get one LOD right, with correct halos, before adding Transvoxel transitions.

## Collision and movement

- **Two surfaces.** Rays and heights intersect the marching-cubes triangles, while trilinear density defines a different implicit surface. A matching mining ray therefore does not prove feet, walls and ceilings collide with what is displayed.
- **Shared triangles.** [pe_scene_collision](../../rebuild/src/pe_scene_collision.h) provides a triangle BVH with raycast, strict capsule overlap and continuous sweep. Render geometry and collision come from one marching-cubes result. The BVH is a surface collider, not a solid-volume classifier, so embedded spawns and closed solid chunks need their own policy.
- **What is left to the native code.** Climbing, swimming, moving platforms, root motion, ragdoll and the full motor are separate work. Sources: `Assets/Scripts/PeEntity/CharacterMotor/PEMotor*.cs` and `Cmpt/Motion/Motion_Move*.cs`.
- **Consistency.** A geometry change must keep rendering, rays, movement, placement and save-spawn validation consistent ([AGENTS](../../AGENTS.md)).

## Mining and building

- **Dig target.** Retries thresholds 1, 11, … 131, validates six neighbouring solid samples, keeps supported low-density selection, and checks strict reach from player feet + (0, 1, 0), with negative-coordinate midpoint rounding. Keep it separate from visual marching-cubes rays and from Block45 picking. Edits must be authorized by the tool action clock ([tools](../FEATURE_CONTRACTS.md)), not by repeated input.
- **Block storage.** Keep `PeB45Cell` packed bytes and half-metre indices. Debit is `ceil(cells/4)` and refund `floor(cells/4)`.
- **Point brush.** Source `BSMath.RayCastDrawTarget` and `BSPointBrush` target occupancy boxes across the terrain and Block45 data sources. `BSMath.Upsilon` is 10,000, and there is no artificial 3 m limit. Terrain targets are offset −0.5, Block45 targets 0.
- **Source controls.** Left click adds, Alt+left subtracts, T rotates.
- **Not covered.** Extended primitives, selection brushes, ISO and creations are separate scopes.

## Camera

[ThirdPersonFollow](../../Assets/PeCamera/Scripts/ScriptModifier/ThirdPersonFollow.cs) takes sensitivity, damping and follow speed from the graph. Script defaults: distance slots 1.8–10, pitch slots −70…55°, zoom, and geometry-clamp recovery. [GeometryClamp](../../Assets/PeCamera/Scripts/ScriptModifier/GeometryClamp.cs) uses near-plane size, layer masks, ray, line and sphere tests, and camera rise. Copying one FOV or distance does not reproduce this; see [camera](../FEATURE_CONTRACTS.md).

## Saves

Original archive v5 and modified-terrain version 1606230 are unrelated to PECS. `.voxelform` import is a geometry import, not a save import. A streamed world must version its configuration and coordinate identity together with a chunk edit archive, plus an explicit migration from the full-volume saves. Saves never hold pointers, GPU or BVH handles, or jobs ([save format](../FEATURE_CONTRACTS.md)).

## Shared tree asset and collider

Presentation: [pe_sandbox_world_view.c](../../rebuild/src/pe_sandbox_world_view.c). Exporter: [export_sandbox_world_assets.py](../../rebuild/tools/export_sandbox_world_assets.py). It reads `rebuild/build/reference-assets/gathering` and writes only `.../sandbox-world`; `--check` regenerates and compares, and existing output is never overwritten.

- **Mesh.** `TrainingManager_New/HoloTree/orgtrees/orgtree`, installed `resources.assets` Mesh 6039: 3,150 vertices, 4,216 triangles, raw SHA256 `f4260d9f8fefcd07257ef753ac01d53dd7934b5671ebcb51ce37ad817395fea4`. The exporter applies the inverse recorded affine to positions and the inverse transpose to normals, which removes the tutorial translation (10.92083058, 1.58233070, 11.97933918) and the ≈−90° yaw. Winding is reversed when the transform mirrors. Mesh-local bounds are (−2.65711046, −0.24681623, −3.46337006) to (2.15026356, 7.78633261, 2.08653378); the negative bottom is part of the source pivot. Each tree is drawn at `PeTree.position` with unit scale.
- **Material.** `Grassland_tree_B` (Material 1654, diffuse Texture 5017), PNG copied byte for byte, tint ≈ 0.882352948. A shader discards texels below `_Cutoff=0.25`. The source `_BumpMap`, `_AO` 2.4, `_BaseLight` ≈0.35, `_Occlusion` 7.5, `_Shininess` 80, wind and shadows are not reproduced.
- **Depletion.** Read on every draw: `remaining==0` draws nothing, and there is no stump. Source: [SkEntitySubTerrain.cs](../../Assets/Scripts/SkillSystem/SkEntitySubTerrain.cs) calls `DigTerrainManager.RemoveTree` at HP ≤ 0.
- **Render ray.** `ray_tree` hits rendered triangles, including transparent card areas, with no alpha test. Tool interactions use `ray_trunk`.
- **Collider.** [tree11_grassland.prefab](../../Assets/Prefabs/tree-Ambient-Occlusion/tree11_grassland.prefab), SHA256 `a94f5931d8fa6cab466004968f0dc73f1eec41fd6c8919874f2f4ae9fc0f442b`, uses the same mesh GUID `71fbbe8530d15a4f8408390cd27df9d4` (Mesh 4300000) with an identity root. CapsuleCollider 13600000 is enabled, non-trigger and Y-directed: centre (−0.10000000149011612, 3, −0.20000000298023224), radius 1 m, height 7 m. The `tree03_01_grassland` capsule belongs to a different mesh (GUID `d792af9af527be4e956f68642c714431`) and must not be used.
- **Sidecar.** The 36-byte `tree-collider.pesc` is little-endian: `PESC`, u32 version 1, f32 centre XYZ, radius and height, u32 direction 1, u32 flags 1. The loader requires the exact length and values equal to the generated [pe_tree_collider.h](../../rebuild/generated/pe_tree_collider.h), produced by [export_tree_collision.py](../../rebuild/tools/export_tree_collision.py), which keeps the binary32 values as hex literals.
- **Interaction.** `Action_Fell.UpdateOPTreeInfo` → `PEUtil.RayCastTree` → a layer-filtered `Physics.Raycast`. The camera ray starts at `mouseRay.origin − 0.5×direction` with range 100 m; the body ray starts at `player.position + up`, forward, range 2 m. Both must name the same tree. Origins inside the capsule miss, per the [Unity 5.4 rule](https://docs.unity.cn/540/Documentation/ScriptReference/Physics.Raycast.html).
- **Near-tree clones.** [RSubTerrainMgr.cs](<../../Assets/Scripts/ZhouXun/Random SubTerrain/RSubTerrainMgr.cs>) keeps only the collider and places clones at `TreeInfo.m_pos` with identity rotation and scale (widthScale, heightScale, widthScale) on layer 21. The `UNITY_5` path stays non-trigger. Native trees use unit scale, an explicit assumption.
- **Layers.** `TagManager` names Player 10, AIPlayer 8 and NearTreePhysics 21. The physics matrix enables 10↔21 and 8↔21, and the movement masks in `Motion_Move_Human` and `Motion_Move_Motor` include layer 21.
- **Movement collision** ([pe_tree_collision.c](../../rebuild/src/pe_tree_collision.c)). The Minkowski capsule for a player of radius r and height h, in feet coordinates, has centre `tree.center − (0, h/2, 0)`, half-axis `(tree.height/2 − tree.radius) + (h/2 − r)` and radius `tree.radius + r`. Entry roots are solved analytically in double precision. Overlap is strict; sweeps ignore tangent and separating contact and keep exact range-end contact.
- **Snapshot.** `pe_sandbox_collision_update_with_trees` copies live capsules into the same transactional snapshot as terrain and blocks, compared field by field rather than by `PeWorld.revision`. Invalid tree data keeps the previous snapshot. `overlap_geometry` and `sweep_geometry`, which the camera uses, exclude trees. Fell's layer-21 rays are not occluded by terrain or blocks.
- **Saves.** Save v5 validates tree collision. A v4 Adventure load uses a bounded tree-escape migration, a compatibility policy rather than Unity behaviour. Legacy mode has no trees.

Still unresolved: species and prototype binding, per-instance width and height, and NaturalRes duration and yield rows per tree.

## Open comparison gates

- Terrain collision: displayed surface, hit point and support agree at diagonal walls, ceilings and all ramp rotations, and long frames cannot tunnel.
- Edit lifecycle near chunk edges: no stale result revives removed terrain.
- Source brush: negative coordinates, offsets, inside-origin suppression and failed-operation conservation.
- Deterministic generation: legacy RNG and noise goldens, then one fully specified map's columns, with caves, ore and water intermediates.
- Streaming: adjacent revisits, halo edits, unload and reload.
- Mode identity: the same seed with different settings stays distinct; Build stack lock and frozen clock persist; insufficient material still fails under the lock.
- Camera and controller: input traces at declared frame rates, from both programs.
