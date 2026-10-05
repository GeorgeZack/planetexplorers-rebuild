# Tutorial terrain parity

This audit compares the supplied Unity source, installed native V1.1.4 data, and current C terrain code. Original desktop input and tutorial observation are controlled separately by the root agent; the measurements here used read-only file inspection and headless C queries. A rendered tutorial room is not evidence that its floor is voxel terrain.

## Exact reference data and coordinates

All ten `TutorialMode/Mainland/map_x0_y0*.voxelform` and `water_x0_y0*.voxelform` files in the installed Steam build are byte-identical to the repository copies. `subter.dat` is also identical. `WorldEntity.xml` differs in text formatting but describes the same single entity: doodad ID 7, prototype 141, `scene_TrainingRoom`, position **(12,1.5,12)**, rotation quaternion **(0,0.7071068,0,-0.7071068)**. That is a separate authored scene asset, not geometry reconstructed by importing the voxel file.

The useful exact static terrain crop is origin **(0,0,0)**, size **64×32×64**, LOD 0. File-local coordinates and crop-local coordinates therefore coincide. Reproduce the import from the repository root with:

```sh
rebuild/build/planet_explorers \
  --terrain TutorialMode/Mainland/map_x0_y0.voxelform --origin 0,0,0
```

This command imports terrain only; it does not instantiate the training room, NPCs, scripted exercises, or props. The map's supported patch is away from the bounded world's default center, so spawn selection must use the imported surface.

| Static fixture | Measured result |
| --- | --- |
| LOD 0 map bytes | 322,811 |
| Map SHA-256 | `538a9f4833f5a6fd902bcddf61541501626c687af0a8ffdaaaf0b7434c45050b` |
| Crop raw voxel SHA-256 | `d490bba7ebc6a864f815fb6fe2e8b7db2913442e2441cacc75c5ca72df2b2c6d` |
| Crop hash serialization | Exactly 64×32×64 pairs of `volume,material`, x-fast, then y, then z; excludes world metadata |
| Samples with volume ≥128 | 606 |
| Samples with nonzero volume | 928 |
| Solid bounds | x=6..25, y=0..3, z=2..21 |
| Solid material | Raw material ID 1 |
| Current source-table mesh | 2,898 vertices / 966 nondegenerate triangles |
| Surface at x=12,z=12 | y=2.94230771 |
| Surface at x=16,z=16 | y=1.81506848 |
| Column x=12,z=12, y=0..3 | volumes 137,167,177,125 |

`subter.dat` is 41,472 bytes, SHA-256 `fae7e6d417ee6f407d9b467fd1b2e450263e710e3f5e3f27058a668063fe29f5`. Its 5,184 eight-byte header records are all little-endian `(offset=41472, uncompressed_length=0)`, with no payload after the header. This agrees with the offset/length reader in `Assets/Scripts/ZhouXun/Layered SubTerrain/LSubTerrIO.cs`: there are no serialized tutorial vegetation records in this file. Trees or exercise vegetation seen later must be sourced elsewhere, such as scene objects or scripted exercise creation; they cannot be recovered from this empty payload.

## The digging exercise changes the world

[`TerrainDigTask.cs`](../../Assets/Scripts/Jaluca/TrainingField/TerrainDigTask.cs), `CreatTerrian`, writes a separate explicit voxel patch when the exercise starts. It uses material 1 and volumes 255,210,190,170,150,130,110. Its base layers are:

| Region, inclusive | Volume |
| --- | ---: |
| x=-2..15, y=0, z=2..21 | 255 |
| x=-1..14, y=1, z=3..20 | 210 |
| x=4..8, y=2, z=8..15 | 190 |
| x=5..6, y=3, z=10..12 | 190 |

Further explicit samples soften and shape the upper edges; for example `(9,2,10..12)=110`, `(9,2,13..14)=170`, `(9,2,9)=150`, `(5..6,3,13)=150`, `(6,3,9)=170`, and `(3,2,9)=130`. The complete write list is the source of truth. `DestroyTerrian` clears these exercise samples again. Thus a static file checksum cannot serve as an expected checksum after this exercise starts.

[`TerrainDigAppearance.cs`](../../Assets/Scripts/Jaluca/TrainingField/TerrainDigAppearance.cs) uses fade center `(14.8,1.53,11)`, temporarily scales/repositions the terrain presentation, and grants item 12 in `Start`. Capture a stable fully appeared state before comparing coordinates or silhouettes. The scripted patch crosses x=0: reproducing it completely needs a world-origin offset that permits original x=-2 or a translated test fixture, rather than silently dropping its negative-coordinate samples. The current file crop importer intentionally rejects negative file-local origins.

## Corrected query geometry

The original assigns extracted terrain meshes to `MeshCollider`, as seen in `VFVoxelTerrain.cs` and `VFVoxelChunkGo.cs`. The C renderer already uses the original marching-cubes lookup tables, corner ordering, threshold 128, and edge interpolation. Its former height and ray queries instead solved the trilinearly interpolated density surface. These are different surfaces away from grid edges.

Measured mismatches before the query correction:

| Fixture | Previous implicit result | Rendered triangle result |
| --- | ---: | ---: |
| Tutorial x=12.2,z=12.2 | y=2.76047492 | y=2.81947117 |
| Tutorial x=13.3,z=2.4 | No surface, height returned 0 | y=1.02752297 |
| Single volume-255 sample at (10,10,10), x=z=10.2 | y=10.21568680 | y=10.09803925 |

`pe_world_cell_triangles` now extracts at most five triangles into a stack buffer using the same tables and interpolation as rendering. `pe_world_raycast` traverses cells with DDA and intersects these triangles; it does not allocate or build a complete mesh. Surface-hit normals are triangle normals, oriented toward decreasing density. `pe_world_height` now queries those triangles downward. `pe_world_density` remains an independent trilinear density query for callers that need sample-field values.

Regression fixtures cover the isolated sample's exact planar surface, upward and downward rays, finite reach, a ray passing through an implicit lobe with no rendered triangle, and both authored tutorial mismatch coordinates. All 256 nondegenerate marching-cubes cases also compare the query cell vertices with rendered vertices. Raw imported voxel bytes and rendering topology are unchanged.

Validation after the change: `make -C rebuild test` passed the core suite, 10 content tests, and 12 reference-launcher tests. `make -C rebuild sanitize` passed the core suite with AddressSanitizer, UndefinedBehaviorSanitizer, and leak detection enabled.

Remaining query limits are explicit: an origin already inside the implicit solid field still returns an immediate hit, preserving existing bounded-world semantics, and a solid top boundary still reports the top of the crop. The bounded controller's cylinder collision probes still sample implicit density, not triangle/capsule contacts. Consequently, this correction establishes rendered-surface ray/height agreement, not exact original character physics. A subsequent controller change should compare capsule contacts with rendered triangles and slope behavior using stable movement fixtures.

## Material and mining parity boundaries

`MarchingCubesSW.cs` selects each intersection vertex's material from the solid endpoint of its crossed edge. The C mesh does the same and preserves raw material IDs. The original then packs all three triangle material IDs plus normal data into UV channels for its terrain shader; C currently uses flat palette colors and does not reconstruct those texture blends. Gradient normals agree with the source discrete neighboring-sample differences inside the crop; crop-edge gradients clamp because outside halo samples are not retained.

Current mining has the source hardness polynomial, attenuation, byte damage, and removal threshold (volume ≤127 becomes zero). [`DigVoxelIndicator.cs`](../../Assets/Scripts/BuildNDig/DigVoxelIndicator.cs) searches `BSMath.RayCastDrawTarget` at thresholds 1,11,...131 and accepts a target when it or one of its six axial neighbors is solid. `Action_DigTerrain` checks a snapped target's distance from `player.position + Vector3.up` against 3 m.

The C world API now reproduces this targeting in `pe_world_dig_target`. It casts against sample-centered boxes using `BSVoxelData.Offset=(-0.5,-0.5,-0.5)`, rather than using mesh triangles to select the nearest solid corner. It retains `BSMath.RayCastV`'s per-axis first candidate, strict minimum-axis selection (equal-entry ties are rejected), and rejection when the ray origin is inside a sample at the current threshold. A failed unsupported target advances to the next threshold. The final origin-near fallback rounds the shifted ray origin using Unity's nearest-even midpoint behavior. Reach is strictly less than the requested distance from the player's feet plus one meter vertically, independent of ray origin distance. `PeHit.x/y/z` is the exact selected sample, and its density may be below 128 or even zero for the supported fallback.

World regressions verify low-density boundary selection, unsupported-wisp threshold retries, exclusion of diagonal-only neighbors, exact reach equality rejection, selection from a distant camera when the player is near, strict corner-entry tie rejection, the air-sample origin fallback, and invalid/huge inputs. Imported crop origins participate in absolute-coordinate rounding: an odd source origin changes the winning local sample at nearest-even midpoint ties. Positive and negative odd-origin regressions protect this correction. The C bound is the imported 64×32×64 crop, replacing the original streaming LOD0 view bound. Tool animation timing and UI presentation are outside this world query.

An independent traversal audit compared 1,000 arbitrary directional rays with brute-force intersections against the complete generated rendered mesh and found no mismatches. A permanent 128-ray version of that oracle and six-direction isolated-sample checks supplement the exact tutorial fixtures. The oracle does not call the cell-query API or reuse DDA traversal.

`DigTerrainManager.DigTerrain` also iterates an x/z radius and y=0..height region, conditionally clips to a sphere when `square` is false, deletes vegetation, and invokes dig callbacks. Its ordinary tool damage is distinct from the smooth spherical subtraction helper `pe_world_dig`; replacing one with the other would lose fidelity. For the current radius 0.49 footprint, the source `IntVector3(float,float,float)` rounds its offsets back to the selected sample. A selected zero-density sample therefore does no terrain damage; `DigVoxels` returns immediately. Tool-specific wider range/height footprints, vegetation removal, animation callbacks, and authored exercise scripting remain separate work.

## Query cost

A headless `-O2` C microbenchmark ran 100,000 queries of each kind against generated seed 666, using changing x/z positions. Downward mesh raycasts from y=30 averaged **1.330 μs/query**; downward mesh height queries averaged **1.370 μs/query**; indicator queries from y=15 with player feet at y=12 averaged **0.173 μs/query**. The benchmark used monotonic wall time and accumulated results to prevent unused-result elimination. These are one-machine CPU measurements, not frame-rate or worst-case claims. Queries allocate no memory, generate at most five triangles per visited cell, and never rebuild a complete mesh. Geometry remains cached independently by the frontend renderer.
