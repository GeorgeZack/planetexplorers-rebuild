# Camera activity space and distance recovery

This increment recovers the source activity-space evaluator and the distance lock following obstruction. It uses source code and the serialized active CameraForge graph; no original game, Unity scene, desktop session or native graphical test was run. It extends the independently verified scalar SmoothDamp implementation described in [SANDBOX_CAMERA](SANDBOX_CAMERA.md). Full CameraForge geometry, anchor animation, rotation and near-plane behavior remain outside this slice.

## Source qualification

[export_camera_environment.py](../../rebuild/tools/export_camera_environment.py) reads the original `PECamera_Start.asset` and `PECamera_Update.asset` binary payloads using their own `ControllerAsset`, `ModifierAsset`, `PoseNode`, `Node` and `Var` serialization contracts. It checks whole-file SHA256 values for those assets, the serializers and the three camera/evaluator sources. It requires the selected graph links and values to match, then generates [source metadata](../../rebuild/generated/pe_camera_environment_source.json) and [C constants](../../rebuild/generated/pe_camera_environment_source.h). This check requires the standard Python library only.

The decoded normal path is ThirdPersonFollow pose1 → Set Cursor pose24 → GeometryClamp pose3 → YawPitchFinding pose4. Normal Follow's serialized minimum is1.7999999523162842, and its maximum input is node7 `Lerp(10,20,Build Mode)` via user-variable node5. Its Damp Rate is approximately.4, separate from the distance SmoothDamp's hardcoded.5 seconds. Start's embedded Init modifier sets `Dist=6`, then `DistWanted=Dist`, then `DistLevel=DistWanted`. Constructor defaults are not substituted for those graph values.

The source owners are:

- `Assets/PeCamera/Scripts/PeCamera.cs`: evaluates activity around `trans.camAnchor.position`, upward weighting, minimum argument.5, maximum50, accuracy4, obstacle mask8468481.
- `Assets/Camera Forge/Scripts/Utilities/Utils.cs::EvaluateActivitySpaceSize`: samples and aggregates rays.
- `Assets/PeCamera/Scripts/ScriptModifier/ThirdPersonFollow.cs`: computes the activity-dependent desired distance, applies the obstruction lock and advances SmoothDamp.
- `Assets/PeCamera/Scripts/ScriptModifier/GeometryClamp.cs`: commits corrected `Dist`, zeroes `DistVelocity` and raises `Geometry Clampd` after the Follow node.

Mask8468481 includes Default, SceneStatic, VFVoxelTerrain, Building, Unwalkable and ProxyPlayer. TreeStatic, NearTreePhysics and other tree layers remain excluded. Source physics queries ignore triggers. Player movement blockers must not automatically become camera blockers.

## Activity evaluator

`pe_sandbox_camera_activity(anchor, callback, context, &activity)` reproduces the source sampling loop. Accuracy4 gives a22.5° pitch step. Pitches below−35° are skipped; each remaining pitch uses its own cosine-dependent yaw step and `sin(pitch*1000)*180` starting yaw. The qualified profile issues66 rays with maximum50m.

For each ray, with returned first-hit distance `d` or50 on a miss:

```text
direction_weight = (clamp(dot(authored_direction, up), -1, 1) + 1)^1.5
distance_weight = 50 / (d + 50*.3)
weight_sum += float(direction_weight * distance_weight)
distance_sum += float(direction_weight * distance_weight * d)
activity = max(minimum_hit_or_miss_distance, float(distance_sum/weight_sum)*.5)
```

Products retain source float precision before double accumulation. Directions are normalized for the query callback, matching construction of a Unity Ray; weighting retains the authored direction. The source's minimum argument.5 is unused, so the evaluator must accept results below.5, including0. A fully clear50m sample returns50, not25, because the minimum-distance branch wins.

The callback returns `false` for a backend failure. On success it writes a finite distance in[0,50], using50 for a miss. It receives the qualified mask and a normalized direction. The entire output is committed only after every query succeeds; malformed distances, a nonfinite anchor or a failed callback leave the caller's previous activity unchanged. The callback owns selection of supported geometry; this interface does not imply that the native world contains every source collider category. Cross-platform C/Unity trigonometric bit identity is not claimed.

## Follow and recovery clock

`pe_sandbox_camera_follow_step(camera, recovery, input, activity)` replaces the old scalar `pe_sandbox_camera_step` for hosts using this slice. Never call both in one frame. Desired distance remains the user's `DistLevel`, with normal bounds1.8–10 and construction bounds1.8–20. Activity changes the smoothing target without changing that level:

```text
coefficient = clamp(activity*.15, .15, 1)
maximum = clamp(activity*2, 1.8, mode_maximum)
target = clamp(DistLevel*coefficient, 1.8, maximum)
```

The source lock order is significant:

1. A permitted nonzero wheel delta clears the lock.
2. A positive remaining lock decrements by `clamp(frame_dt,.001,.1)`.
3. The prior geometry-clamp latch sets the lock to2 seconds and clears that latch.
4. With no positive lock, distance damps toward the activity target. During the lock it damps only if shorter than1.8, toward1.8.

A prior clamp therefore overrides a wheel unlock in the same Follow evaluation. A later wheel delta without a new clamp releases the lock. Repeated obstruction continually renews it. The decrement may leave a small negative value, which is preserved until another lock or wheel operation. Source `recalcDistTime` is separate and does not gate these distance operations.

Only the hold decrement uses the clamped interval. The `.5` SmoothDamp overload uses actual `Time.deltaTime`; the C specialization preserves that distinction. For example, a.5s update while locked and at1m damps to approximately1.4470588 with velocity.9411765; a subsequent hold decrement consumes.1s. The scalar formula remains the independently checked Unity5.2 implementation, including its strict overshoot rule.

`pe_sandbox_camera_feedback(camera, recovery, actual_distance, true)` commits the final shortened distance, clears velocity and queues the next Follow latch. A false clamp is a no-op and cannot erase a pending latch. Feedback belongs after the final display geometry evaluation, never an intermediate aiming/query camera. The original GeometryClamp construction is not implemented by this function; its host caller supplies the qualified or explicitly adapted correction.

## Native ownership and limits

Both camera and recovery state are transient. Initialize them together on a new host and successful load; never serialize the lock, latch or velocity. Input remains owned by the existing UI/pointer policy. Modal gating blocks wheel input while an unpaused third-person camera can settle. Pause, zero delta and first person freeze the clock and pending latch; these are retained native policies. Leaving construction still enforces an immediate upper10m range even while paused, another existing native policy. Geometry-shortened actual distances may be below1.8 or zero and must not be snapped to the minimum before the recovery branch.

The pure API accepts finite display intervals[0,1]; no gameplay catch-up tick may advance it again. It validates all state and inputs before commit. The old scalar helper remains available for hosts without environment queries and retains its previous tests and behavior.

Still unported are the source animated camera anchor, FOV/near-plane-radius search, visibility rings, dynamic near clip, clamp rise, YawPitchFinding, and the full graph's blends and alternate camera branches. The existing wheel-notch conversion and free-pointer gesture are native input adapters. This increment establishes the portable evaluator, targets, lock ordering and clocks, not full visual camera parity. Host geometry integration and its limitations are recorded separately by the host owner.

## Verification

[test_camera_environment.c](../../rebuild/tests/test_camera_environment.c) passes optimized strict C11 and ASan/UBSan with leak detection. It checks66 normalized tree-excluding rays; clear, uniformly enclosed, subminimum, zero-distance and corridor geometry; failed-query atomicity; activity target convergence and construction bounds; clamp/wheel ordering; hold renewal and release; distinct hold/damping intervals; below-minimum recovery; modal/pause/first-person gates; and malformed-state atomicity.

The existing [scalar and controls tests](../../rebuild/tests/test_sandbox_camera.c) also pass unchanged. Four [exporter tests](../../rebuild/tests/test_camera_environment_export.py) pass with zero skips, covering exact reproduction, active graph/init bindings, source hash drift, and truncated/ambiguous asset rejection. `export_camera_environment.py --check` passes. These are source and headless checks, with no claim of a new physical camera observation.


## Sandbox host integration

The Adventure host now evaluates activity through the actual tree-excluding collision snapshot, then advances Follow once before its aim queries. Only the final displayed third-person pose sends clamp feedback; repeated aim/query calls cannot advance the recovery clock. Pause, focus loss, first person and synthetic smoke overview do not generate feedback. Successful loads reset the transient camera and recovery to the existing6m default; no save fields were added. Legacy keeps the prior scalar-only helper.

The existing native eye anchor at player feet+1.58m, eye+2×direction look target, .5m rise and .1m capsule sweep remain explicit framing/geometry adapters. A native sweep hit feeds back `distance * safe_fraction` along the existing offset, preserving the selected distance instead of repeatedly adding the artificial rise. This is not the source GeometryClamp near-plane/ring/sphere implementation. Activity rays exclude synthetic crop-boundary planes, while the final existing sweep still respects crop bounds. Geometry outside the captured route is unavailable; an open ray near a crop edge does not establish that the original world is open there.

The actual-host headless test includes `pe_sandbox_view.c` and a real collision snapshot: open space, live trees versus a wall/corridor, read-only aim calls, final clamp feedback, pause/first-person/overview isolation, the recovery hold, wheel release, invalid backend rollback and Legacy dispatch pass optimized and ASan/UBSan with leak detection. No window is created.

A separate CPU-only benchmark loaded the unchanged prior route fixture and used the actual host callback on387,048 collision triangles. Five saved/archived anchors ×300 evaluations issued99,000 rays. Mean evaluation cost was .378–.429ms per display sample, maximum observed .541ms; load2.462s and collision construction2.404s were measured separately. All activities were finite/repeatable (15.2769–21.0503),6m targets remained6m, and game/world/snapshot hashes were unchanged. Evidence is ignored under `rebuild/build/camera-environment-route`. This is one machine/run and does not measure rendered FPS or visual camera acceptance.
