# Camera geometry: qualified math and query boundary

Subsequent checkpoint: [Adventure host integration](CAMERA_ALIGNMENT.md) now connects this evaluator through dedicated camera queries and a documented native projection adapter. The disconnected status below describes the preceding checkpoint.

This October7 audit qualifies the remaining `GeometryClamp` and `EvaluateNearclipPlaneRadius` source path. It follows the implemented [activity-space and recovery increment](CAMERA_ENVIRONMENT.md). No runtime camera, collision code, exporter or Makefile changed. The source algorithm is recoverable without manual play; its physics and projection prerequisites prevent a safe radius-only replacement of the current host approximation.

## Actual graph values and order

The existing standard-library decoder in [export_camera_environment.py](../../rebuild/tools/export_camera_environment.py) reads the original update asset with SHA256 `253ba07513eae22cf3f4e404d63a9942ddea7c13bfb12215bd476000f3760938`. The normal settled third-person path is:

```text
History0 → InitCursor25 → SystemSettings2 → Follow1 → SetCursor24
 → GeometryClamp3 → YawPitchFinding4 → ThirdPersonBlend5
 → First/ThirdPersonBlend23 → WaterClamp27 → GlobalBlend9
 → AttackShake17 → ExplosionShake18 → CutsceneBlend13
 → ChangeNearclip26 → ForceShowCursor29 → final
```

Serialized normal selections are0 for ThirdPersonBlend, First/ThirdPersonBlend and CutsceneBlend; GlobalBlend selects1. Runtime crossfades and alternate modes remain separate. `SystemSettings2`'s embedded `ConeOutput` sets **near clip.800000011920929 every evaluation**. Its FOV input reads `Original Fov`, set by `PeCamera.Update` from `SystemSettingData.CameraFov` (default60°, persisted user setting). `Pose.Default`'s.3 near clip is therefore not the active normal value. The final `ChangeNearclip26` passes this result through unless `OnMonorail` is true, in which case it interpolates to.6000000238418579.

`CameraController.PlayController` assigns the final pose's position, rotation, FOV and near clip to the Unity camera. `PeCamera.AfterControllerPlay` also synchronizes an auxiliary Near Camera: near.03, far=main near+.01, matching FOV and parent transform. Its source prefab has culling mask13212945, layers0,4,8,10,11,12,15,16,19,22,23. This extra depth pass is absent from the current native renderer. Simply setting the native primary near plane to.8 would remove close geometry that the original can still draw through that auxiliary camera.

## Source geometry calculation

`Assets/PeCamera/Scripts/ScriptModifier/GeometryClamp.cs` receives the already evaluated Follow pose. `target=position+Dist*forward`; Follow previously sets `position=anchor−Dist*forward`. It uses the actual camera aspect and pose FOV:

```text
ur = sqrt(1 + aspect²) * tan(FOV*.5*Deg2Rad)
scaled_near = incoming_near * clamp(activity*.2, .4, 1)
NCR = EvaluateNearclipPlaneRadius(target, .05, scaled_near*ur)
near = NCR/ur
eye = position + near*forward
```

At60° and16:9, `ur≈1.177638`; an unobstructed high-activity normal view permits radius≈.942110m and near.8m. Minimum activity scaling gives radius≈.376844m and near.32m before local overlap reduction. At the radius floor.05, the corresponding near is≈.042458m. This is much larger than the current fixed.1m camera sphere in ordinary open space, and it changes with aspect/FOV.

`Utils.EvaluateNearclipPlaneRadius` first tests overlap at.05 and returns.05 if occupied. Otherwise it tests the maximum and returns it if clear. When the maximum overlaps, it binary-searches until `max−min<=.005`, returning the last clear lower bound. It does not search for escape from an occupied minimum. Do not replace its two endpoint queries or stopping condition with a generic closest-hit distance.

Visibility then checks rings at radii0,1.5,3 around the target in the camera-right/camera-up plane. The branch order gives **4,4,8 iterations**, not1,4,8: the `else if(r<1)` branch is unreachable. Angles use integer `360/n` followed by degree-to-radian multiplication. Radius0 skips its radial ray but can repeat the same blocked linecast four times. For each nonzero ring point, the radial target→point ray must be clear before testing point→eye. The first clear line jumps out of both rings. Thus the worst case uses16 linecasts and12 radial rays, with early exit for clear views.

If no offset line is clear, or the eye's NCR sphere overlaps, the source sphere-casts target→eye at radius`NCR−.01`. Despite its `while`, it performs at most one cast: both hit and miss branches break, and `curr_dist` never advances. A hit replaces the eye with the sphere center at the hit distance, clears `DistVelocity` and sets `Geometry Clampd`. It then reconstructs position=eye−near*forward. Without a sphere hit it does not invent a clamp event.

Distance is measured **before rise**. The separate rise query casts from the eye along normalized `worldUp+forward*.3`, radius`NCR−.02`, length`NCR*10`. The default rise cap is NCR; a hit replaces that cap with`hit.distance*.95`, which can be greater than NCR. When distance<2*near, desired rise is `sqrt(1.0001−(distance/(2*near))²)*2*near`, capped by that value; Shoot Mode forces0. Rise interpolates by literal.1 per evaluation. Final displacement uses the **unnormalized** `worldUp+forward*.3` vector. The persistent Dist remains the pre-rise distance. These exact distinctions matter to avoid inventing a time-scaled rise, double updates or outward distance drift.

All these source geometry calls use mask8468481 and ignore triggers. Trees remain excluded. WaterClamp and subsequent graph blends/shakes can still change the final pose; the normal geometry evaluator alone would not recover the entire graph.

## Actual native mismatch and CPU proof

Current `game_camera` keeps the player-eye anchor at+1.58m, targets eye+2*direction, adds.5m rise to its backward offset and sweeps a.1m-radius/.2m-height capsule. Its feedback uses `distance*hit_fraction`, intentionally preserving the previous bounded framing adapter. The source algorithm expects an unraised Follow pose and separate camera basis. Its near-plane sphere cannot be substituted for that capsule without changing the pose construction as well.

[reference_camera_geometry.c](../../rebuild/tools/reference_camera_geometry.c) runs only CPU collision/math APIs; it never initializes a window or graphics context. Against a plane, a sphere centered.05m from it with radius.1 is initially overlapping. The current C sweep returns an inward contact at fraction0, while separating motion has no hit. This is deliberate existing movement-query behavior.

Unity's archived5.3 [SphereCast documentation](https://docs.unity3d.com/530/Documentation/ScriptReference/Physics.SphereCast.html) states that initially overlapping colliders are not detected by that cast. Its [OverlapSphere documentation](https://docs.unity3d.com/530/Documentation/ScriptReference/Physics.OverlapSphere.html) describes a collider bounding-volume test. These are primary, nearby-version API contracts; they do **not** prove the exact installed5.2.4f1 native PhysX behavior. Our current merged triangle BVHs lack original collider identities/bounds, so neither skipping individual initially overlapped colliders nor reproducing the documented overlap granularity follows from the existing API. Skipping the entire sweep whenever anything overlaps would also miss later obstacles and is not equivalent.

Native `pe_sandbox_collision_overlap_geometry` adds crop bounds and solid containment to triangle contact. Its sweep adds synthetic crop planes. Native camera rays exclude those planes, and scene overlap uses a strict penetration tolerance while source OverlapSphere describes touching as included. Static merged scene interiors also cannot be treated as one solid building volume because they contain rooms. The replacement needs an explicit camera-query contract; changing shared movement collision semantics to accommodate the camera would risk unrelated behavior.

The same CPU probe against installed raylib6.0 reports default near.05/far4000. `GetScreenToWorldRayEx` returns the perspective camera-position origin both with near.05 and near.8; the sampled off-axis directions differ only by floating-point rounding. Unity5.3 [ScreenPointToRay](https://docs.unity3d.com/530/Documentation/ScriptReference/Camera.ScreenPointToRay.html) documents a ray beginning on the near plane. Current native rendering also reflects projection X, while picking reflects screen X. Future changes must keep that reflection, actual projection clip distance and the ray-origin policy consistent. Interaction/tool reach must remain independently limited from the player's eye; moving a pick origin must not extend it.

Observed probe output on this machine:

```text
initial overlap: inward hit=1 fraction=0; separating hit=0
raylib default: near=.05 far=4000
near=.05: ray origin=(0,0,6), direction=(.247476146,.092803568,−.964439213)
near=.8:  ray origin=(0,0,6), direction=(.247475356,.092803262,−.964439511)
```

The probe restores its process-local clip settings before exit. Reproduce without a desktop:

```sh
mkdir -p rebuild/build/camera-geometry-audit
TMPDIR="$PWD/rebuild/build/compiler-tmp" cc -std=c11 -O2 -Wall -Wextra -Werror \
  -Irebuild/src $(pkg-config --cflags raylib) \
  rebuild/tools/reference_camera_geometry.c rebuild/src/pe_scene_collision.c \
  -o rebuild/build/camera-geometry-audit/probe $(pkg-config --libs raylib) -lm
rebuild/build/camera-geometry-audit/probe
```

The strict optimized probe and ASan/UBSan probe with leak detection both pass. Local documentation links and whitespace checks pass. These checks establish current native query/math behavior; they do not execute original Unity physics or validate a new rendered camera.

## Pure increment completed; host gate remains

The subsequent [disconnected C evaluator](CAMERA_GEOMETRY.md) now implements the bounded mathematical step below, with guarded graph/source metadata, transactional query callbacks and analytical optimized/sanitizer tests. No live camera, movement backend or projection was changed. The remaining prerequisite is a qualified camera-query/rendering integration, not further recovery of these formulas.

First add a disconnected pure geometry evaluator and guarded graph metadata, with explicit callbacks for ray/line, sphere overlap and sphere cast. Port the source endpoint/binary search, ring order, one-cast correction and frame-based rise; return corrected pose, NCR/near clip, pre-rise distance and clamp flag transactionally. Use deterministic analytical callback fixtures for open space, narrow walls, initial overlap, alternate visible rings, aspect changes and failed queries. Keep desired distance untouched. Preview/aim evaluation must not commit rise or a clamp latch; only the final display evaluation may commit them.

Before enabling it in the host, specify a camera-only backend with explicit initial-overlap/collider-granularity and crop-edge behavior. Preserve the current player movement backend. Projection/near-camera rendering and pointer-origin behavior need a coordinated decision: a documented native rendering adapter is possible, but it must not be presented as exact Unity geometry parity. The qualified pure algorithm can be implemented and tested now without manual attention; a silent host radius swap cannot safely close these outstanding contracts. No new original-game launch is needed for this bounded next step.
