# Recovered scene collision

The C rebuild can load the recovered tutorial room's static collision geometry and query it without Unity or raylib. The implementation is [`pe_scene_collision.c`](../../rebuild/src/pe_scene_collision.c), with its public API in [`pe_scene_collision.h`](../../rebuild/src/pe_scene_collision.h). This module supplies geometry queries; the separate [`pe_training.c`](../../rebuild/src/pe_training.c) supplies a new capsule movement controller. These headless results establish geometric behavior, not complete original physics parity. The separate [playtest record](../PLAYTEST.md) records the subsequently passed graphical smoke and interactive input check.

## Source geometry and coordinates

[`export_reference_assets.py`](../../rebuild/tools/export_reference_assets.py) writes `rebuild/build/reference-assets/room-collision.obj`. The file contains already transformed Unity XYZ coordinates in meters. The recovered hierarchy includes the room placement at `(12,1.5,12)` and its −90° Y rotation; consumers must not apply that transform a second time.

The exported source manifest identifies three enabled non-trigger, non-convex mesh colliders:

| Source object | Triangles |
| --- | ---: |
| `TrainingRoom_MeshPart0` | 53,359 |
| `TrainingRoom_Col` | 828 |
| `TrainingRoom_MeshPart1` | 2,259 |
| Total | **56,446** |

The loaded OBJ has **69,618 vertices**, three object records, and no degenerate triangles. All three collider meshes are retained. Measured bounds are approximately `(-35.44,-2.96,-13.52)` to `(69.47,23.38,33.13)`. The original flat room visual model is not used as a substitute for these collider meshes.

OBJ does not encode Unity trigger or convex flags. The current source manifest establishes that all three exported colliders are ordinary non-trigger static meshes. If later exports contain trigger meshes, filter them during export or add explicit metadata before collision loading; the C loader cannot infer trigger semantics from triangle coordinates.

## Loading and BVH

`pe_scene_load_obj(path,error,capacity)` returns a new opaque `PeSceneCollision` or `NULL` with an error. It never changes an existing scene and releases partial allocations on failure. `pe_scene_free` releases the owned triangle and node arrays. `pe_scene_stats` reports vertex, triangle, object, node, and degenerate counts plus bounds.

Input limits are 256 MiB, one million positions, one million triangles, and 4,095 bytes per input line. Position coordinates must be finite and within ±1,000,000. Faces must be triangles with positive indices referencing already declared records. The loader accepts plain position indices and the exported position/UV/normal index forms, validates optional auxiliary references, and ignores UVs and normals after parsing. It accepts object/group/material labels without opening referenced material files. Negative indices, polygons, unknown directives, missing references, nonfinite numbers, embedded NUL bytes, and files without usable triangles are rejected. This is the supported exporter format, not a general-purpose OBJ implementation.

The BVH recursively splits triangles at the median of their centroids along the longest node-bounds axis. Leaves contain at most eight triangles. The tutorial room produces **16,383 nodes**. Original file-order triangle IDs survive BVH reordering. Degenerate triangles, if present in a mixed valid file, remain counted but are excluded from collision queries. Queries use a fixed traversal stack and allocate no memory. Median subdivision bounds tree depth within that stack under the loader's triangle limit.

## Query contracts

| API | Behavior |
| --- | --- |
| `pe_scene_raycast(scene, origin, direction, range)` | Normalizes a nonzero direction and returns the nearest triangle contact within range. The reported normal faces against the ray. |
| `pe_scene_capsule_overlap(scene, feet, radius, height)` | Tests penetrating overlap between a vertical capsule and nearby triangles. Exact touching is not classified as penetration. Invalid inputs conservatively return true. |
| `pe_scene_capsule_sweep(scene, feet, radius, height, delta)` | Continuously translates the capsule and returns the earliest blocking contact fraction in `[0,1]`. It does not move the caller or slide along a surface. |

`PeSceneHit` contains `hit`, obstacle contact `position`, contact `normal`, travel `distance`, normalized `fraction`, and source `triangle` ID. Ray fractions are distance divided by supplied range; capsule fractions are the portion of the supplied displacement traveled. Invalid or zero-direction rays and invalid or zero-displacement sweeps return no hit.

`feet` is the capsule's bottom point. Its vertical axis runs from `feet.y + radius` to `feet.y + height − radius`; hemispheres complete the shape. Radius must be positive, height must be at least twice the radius, and both are capped at 10,000 meters. The training controller uses radius **0.30 m** and height **1.80 m**. A height equal to twice the radius is supported as a sphere.

Overlap computes the closest distance between the capsule axis segment and each candidate triangle. The narrow phase handles segment/face intersections, endpoint/triangle Voronoi regions, and segment/edge distances. Sweeps first traverse the swept capsule bounds, then use conservative advancement against the same segment/triangle distance. Tangent or separating initial contacts do not block motion. A closing contact reports fraction zero when already touching. The implementation uses small numerical tolerances and up to 48 advancement iterations per candidate triangle.

## Headless validation

[`test_scene_collision.c`](../../rebuild/tests/test_scene_collision.c) enters through `pe_test_scene_collision()`. Synthetic fixtures cover:

- nearest floor raycasts, ray normalization, finite reach, and invalid directions;
- nonpenetrating floor contact and penetrating floor, wall, and ceiling overlaps;
- horizontal wall sweeps, separating contact, and immediate closing contact;
- downward landing and upward ceiling contact;
- a 10,000-meter displacement stopping at a thin wall;
- the zero-length-axis sphere case;
- exact capsule support height and normal on a sloped ramp;
- malformed indices, missing auxiliary records, nonfinite vertices, unsupported faces, degenerate-only input, and embedded NUL rejection.

When `PE_SOURCE_ROOT/rebuild/build/reference-assets/room-collision.obj` exists, the suite also loads the actual exported asset, verifies all three collider counts, and checks the original spawn `(27,2,11)`. Absence of this optional recovered asset skips those asset-specific checks; it does not skip the synthetic tests.

At that spawn, a downward ray measured the deck at **y=1.47578394**, with normal approximately `(-0.00004,0.99997,-0.00765)` and travel **0.52421606 m**. A downward capsule sweep places its feet at approximately **y=1.47579265** before controller skin is added. The spawn capsule itself is not penetrating the scene. These measured values explain why settling the character to y=1.5 by assumption is less precise than querying the recovered collider.

The standalone module and tests passed strict C11 compilation with `-Wall -Wextra -Wpedantic -Werror`, then AddressSanitizer, UndefinedBehaviorSanitizer, and leak detection. These checks covered the actual available room asset as well as synthetic geometry.

One local optimized microbenchmark measured approximately **199 ms** to parse the room and build its BVH. Over 10,000 varying positions near the deck, raycasts averaged **3.567 μs**, downward capsule sweeps **4.442 μs**, and capsule overlap checks **0.946 μs** per query. These are one-machine CPU observations, not frame-rate promises or worst-case bounds.

## Controller integration and limitations

[`pe_training.h`](../../rebuild/src/pe_training.h) keeps movement state separate from the survival world's existing terrain controller. `pe_training_init` begins from the source spawn, looks for a nearby walkable surface using the scene capsule queries, and leaves its destination unchanged if no safe spawn is found. `pe_training_step` runs a fixed 1/60-second tick and owns contact sliding, limited stepping, grounding, stamina, and tutorial movement events. Its current controller uses a 0.003 m skin, 0.35 m step limit, and walkable normal-Y threshold 0.65. Jump impulse and gravity are provisional constants, not recovered Unity physics.

The collision module handles static, two-sided triangle surfaces. It does not provide rigid bodies, dynamic object contacts, friction, penetration recovery, Unity layer masks, trigger events, moving lifts, or PhysX cooking behavior. A capsule starting deeply through a triangle requires caller recovery or a new safe spawn; sweeps are not a depenetration solver. The new movement response is not an exact recreation of the original character controller. Existing terrain density collision queries remain a separate system.

The room collision tests prove loading and headless geometric behavior. Visual frontend operation and the first two tutorial lessons have separate evidence in the [playtest record](../PLAYTEST.md). Complete tutorial progression, original camera feel and later gameplay interactions remain unverified.
