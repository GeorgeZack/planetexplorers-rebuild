# Disconnected camera geometry evaluator

October7,2026, US/Central. **Source-derived:** active normal CameraForge graph bindings and `GeometryClamp`/`EvaluateNearclipPlaneRadius` control flow. **Implemented in C:** a pure evaluator with explicit physics-query callbacks and transactional rise state. **Not integrated:** native camera framing, collision backend, near-camera rendering or pointer projection. No desktop input, Unity runtime, game launch or new physical acceptance is involved.

The preceding [geometry qualification](CAMERA_GEOMETRY_GATE.md) remains the authority for source paths, query/projection mismatches and original-versus-native boundaries. The existing [camera activity/recovery host](CAMERA_ENVIRONMENT.md) continues unchanged. This increment completes the pure-math step proposed by that gate; it does not remove the backend or rendering prerequisites.

## Guarded source and active graph

[export_camera_geometry.py](../../rebuild/tools/export_camera_geometry.py) reuses the standard-library CameraForge asset decoder, independently guards source hashes, verifies the active normal path and emits [constants](../../rebuild/generated/pe_camera_geometry_source.h) plus [metadata](../../rebuild/generated/pe_camera_geometry_source.json). Neither UnityPy nor Unity is needed. It rejects changed source, graph inputs, default blend selections, incoming near clip and final output links.

The checked graph contains GeometryClamp3 after Follow1 and SetCursor24. SystemSettings2 supplies near clip **.800000011920929** and the `Original Fov` runtime variable; the independently pinned `SystemSettingData.cs` initializes the user setting to60°. Runtime user changes are valid inputs, not replaced with60°. The normal chain continues through YawPitchFinding, blends, water and shakes before ChangeNearclip26; the pure evaluator does not execute those later nodes. Metadata retains the later near modifier so its presence is visible rather than implicitly discarded.

All evaluator queries carry mask **8468481** and `ignore_triggers=true`. This does not make an arbitrary callback backend equivalent to Unity layers or PhysX. No tree, crop-boundary or collider-identity policy is silently attached to the mathematical evaluator.

## API and ownership

[pe_camera_geometry.h](../../rebuild/src/pe_camera_geometry.h) defines:

- `PeCameraGeometryInput`: incoming Follow position, authored forward/right/up basis, current Dist, near clip, FOV, aspect, activity and Shoot Mode.
- `PeCameraGeometryState`: transient rise only. It contains no desired distance, velocity, hold timer, clamp latch, saved data or graphics handle.
- A tagged query callback: ray, line, overlap or sphere cast, with explicit endpoints/direction, radius, maximum distance, mask and trigger policy. The callback returns query success separately from hit/miss.
- `PeCameraGeometryResult`: corrected position, near clip, NCR, pre-rise Dist, proposed rise and a clamp **event**.

`pe_camera_geometry_evaluate` reads a const state and returns a proposal. Repeated aim/preview queries cannot advance rise or update a clamp latch. `pe_camera_geometry_step` computes the same proposal, then commits rise and output only after complete success. The future host must call that step once for the final active display evaluation; there is deliberately no fixed-step or dt-based lerp API. Paused, first-person and preview scheduling remain host responsibilities. On an actual committed clamp, the host may clear distance velocity and set its previous-clamp latch; a miss must not erase a pending latch. Desired distance is never modified here.

Inputs must be finite and within their mathematical domains. The authored basis must be unit, orthogonal and right-handed (`right×up=forward`) within.0005; the evaluator rejects an incoherent basis instead of silently repairing it. FOV is strictly between0° and180°, aspect/near are positive, distance/activity/rise nonnegative. Query answers must have finite bounded hit distances; misses and overlap queries use distance0. The callback must return false when its backend cannot answer. Any invalid input, failed query, invalid answer or overflow leaves both output and state byte-for-byte intact. Callback side effects themselves cannot be rolled back, so a future backend should query immutable snapshots.

Vector normalization uses an explicit native1e-5 small-vector guard, and trigonometry uses C float functions. This reproduces the qualified control flow and formulas without claiming bit-identical Unity engine arithmetic. Generated constants retain source values. Extremely small positive near/FOV combinations that would request a nonpositive sphere radius fail closed rather than inventing a radius or issuing an invalid backend query.

## Preserved calculation and query order

The target is incoming position+Dist×forward. Near-plane scale is `sqrt(1+aspect²)*tan(FOV/2)`; incoming near is first multiplied by the source clamp of activity×.2 to[.4,1]. The radius search queries minimum.05 first, then maximum. An occupied minimum returns immediately. A clear maximum returns that maximum. Otherwise the source lower-clear binary search stops at width≤.005. No extra sorting/clamping is inserted if the requested maximum is below.05: source endpoint order can return a smaller clear maximum or the larger occupied minimum, and tests cover both.

Rings retain radii0,1.5,3 and the source branch's **4,4,8** iterations. Radius0 repeats blocked center lines four times. Each nonzero point first tests its radial ray, then only if clear tests the point-to-eye line. Integer360/n angle arithmetic and the first-clear-line early exit are preserved. When all lines block, the source short circuit omits the eye overlap query; otherwise it tests eye overlap at NCR.

When correction is needed, the evaluator makes at most one target-to-eye sphere cast at NCR−.01. Zero-length correction skips that cast. A hit moves the eye to the hit center and produces a clamp event; a miss does not. Position is reconstructed behind the eye by the recalculated near clip, and **Dist is measured before rise**.

The separate rise sphere cast always uses radius NCR−.02, normalized `worldUp+forward×.3` and range NCR×10. Its hit replaces the default NCR cap with hit distance×.95, even if this is greater than NCR. Desired rise follows the source `sqrt(1.0001−u²)×2near` branch when Dist<2near, capped by that result. Shoot Mode sets wanted rise0; existing rise then decays through the same literal.1 lerp. Final displacement uses the **unnormalized** rise vector. Source camera-relative up is used for rings; source world up is used for rise. Neither rise displacement nor hypothetical preview output feeds back into Dist prematurely.

## Verification and remaining gate

[test_camera_geometry.c](../../rebuild/tests/test_camera_geometry.c) uses deterministic analytical callbacks, including an infinite-plane fixture. It covers open space, aspect/activity changes, both near-search endpoints, exact binary progression, all ring order/counts, radial rejection, alternate visible rings, eye overlap, initial-overlap cast misses, one-cast correction, zero-length correction, hit caps above/below NCR, Shoot Mode, pitched camera/world-up distinction, sequential rise and read-only repeated previews. Every query in a complete binary-search/full-ring path is separately failed and given an invalid response to verify transactional rollback. Invalid domains, handedness and unbounded cast answers are rejected. The fixture's plane initial-overlap policy is an explicit test assumption; it does not qualify the installed Unity backend.

The strict optimized suite and ASan/UBSan with leak detection pass. [Five exporter tests](../../rebuild/tests/test_camera_geometry_export.py) pass with zero skips, covering exact regeneration, active graph contract, mutated connections/defaults, source drift and stale outputs. The exporter `--check` passes. These are focused mathematical/source checks, not a new full-rebuild result or visual acceptance claim.

Reproduce from repository root without graphics:

```sh
python3 rebuild/tools/export_camera_geometry.py --check
python3 rebuild/tests/test_camera_geometry_export.py
TMPDIR="$PWD/rebuild/build/compiler-tmp" cc -std=c11 -O2 -Wall -Wextra -Werror \
  -Irebuild/src rebuild/tests/test_camera_geometry.c rebuild/src/pe_camera_geometry.c \
  -lm -o rebuild/build/test_camera_geometry
rebuild/build/test_camera_geometry
```

Before host integration, qualify a camera-only query backend for initial overlaps, original collider bounds/granularity, touching and crop edges. Decide primary/auxiliary near-camera rendering and reflected pointer-origin behavior together. The existing shared movement sweeps and live camera framing remain unchanged; adding this evaluator does not authorize replacing their radius, projection, or collision semantics piecemeal.
