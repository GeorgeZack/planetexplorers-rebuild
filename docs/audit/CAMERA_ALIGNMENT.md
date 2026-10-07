# Sandbox camera collision and cursor alignment

October 7, 2026. The Adventure host now integrates the source-derived [GeometryClamp evaluator](CAMERA_GEOMETRY.md) with a dedicated [camera query backend](CAMERA_QUERY.md). This is a bounded native implementation; it does not reproduce the entire CameraForge graph or Unity's two-camera compositor.

## One display pose and separate query ownership

`pe_sandbox_view.c:sandbox_camera_pose_frame` copies the scalar camera, recovery and geometry-rise state, evaluates them against the current immutable collision snapshot, and commits only after all queries succeed. The active third-person path uses the existing verified Follow distance/activity clock and recovered GeometryClamp near-radius search, visibility rings, sphere correction and rise. A real geometry clamp feeds its pre-rise distance back into the existing velocity/recovery owner without changing desired distance. Pause, focus loss, zero elapsed time and first person do not advance rise or consume clamp feedback. Loading initializes the transient rise state.

The committed camera is computed once before action admission and fixed-step gameplay, then used unchanged for interaction rays, tool rays, construction previews, labels and rendering. The former separate pre-simulation aim camera and post-simulation render camera are removed. Player movement/world edits affect camera queries on the next display snapshot. This one-display following order is a native adapter and may be visible at low frame rates; it must not be silently replaced with a second geometry-clock update. The retained player anchor at foot position plus 1.58m remains a native substitute for source animated anchor following. Legacy and synthetic smoke overview retain their preceding framing.

Camera queries exclude synthetic crop boundary planes, so the camera can move outside the captured crop without invented terrain; the prior camera sweep constrained those boundaries. Camera queries also exclude trees and trigger geometry; they do not change player movement sweeps, placement validation, tool masks or save-spawn policy. The backend preserves source static collider component identity recovered from existing OBJ names; its terrain/construction identities and triangle overlap/cooking policy are explicit native adapters. Unknown static identities or unsupported query policy fail rather than appearing clear. See the backend audit for precise limits and source metadata regeneration.

## Coordinated projection adapter

[pe_camera_projection](../../rebuild/src/pe_camera_projection.h) owns reflected source-coordinate perspective rays and screen projection. Source/display right is opposite conventional OpenGL camera right. The same FOV, aspect, basis and position feed both projection directions. Off-axis ray origins lie on the actual visible near plane: advance along the normalized ray by `near / dot(ray,forward)`, not by `near` alone. Player-eye and player-root reach checks remain independently enforced by existing interaction/tool/construction owners; changing the camera origin grants no extra reach.

The source primary camera receives a dynamic logical near plane from GeometryClamp; an auxiliary source camera renders close objects from .03m to primary-near+.01m with its own layer mask. The native renderer uses a **single .03m–4000m depth pass** for the combined supported scene. GeometryClamp still receives its recovered .8m input and calculates its logical near/NCR for collision. Native picking starts at the visible .03m plane. This keeps close objects visible and targetable with the same projection, but deliberately differs from source primary-camera ray origins and auxiliary layer/depth composition. It is not an exact two-camera port.

The sandbox saves/restores raylib's process-local clip values around each 3D pass; Character previews, tutorial and main menu do not inherit this clip setting. Reflected winding remains scoped to the sandbox scene. First-person/overview rays use the same actual visible near-plane convention.

## Verification and remaining acceptance

- Pure projection tests cover reflected direction, off-axis near-plane origin, screen/ray round trips at 1280×720, 640×480, 320×240 and 954×1048, pitched cameras and invalid-input rollback.
- CPU-only tests against installed raylib matrices verify the same pixel/ray direction and projected coordinates at all four sizes without initializing a window.
- The production-included camera host test covers actual current collision snapshots, wall clamping, tree exclusion, recovery feedback, frozen/first-person state and whole-frame rollback on an unready backend. The prior scalar/activity tests remain in place.
- Original-game visual parity, animation-anchor following, water/vehicle/shake graphs and a true auxiliary near-camera pass remain unqualified.

Reproduce the focused integration with `make -C rebuild camera-projection-test camera-geometry-test camera-environment-test sandbox-camera-environment-host-test`. Backend qualification, real-route timing and graphics results are recorded separately when completed.

Manual review: in Adventure use all three F5 modes, orbit near a building and terrain slope, resize, and aim at a nearby NPC/herb/tool target toward the screen edge. Check close geometry visibility, wall behavior, projected labels and cursor targeting. Keep action reach and tree exclusion distinct from movement collision. No fresh Steam session is needed to perform these native checks.


## Completed checkpoint evidence

The production build, optimized core/aggregate suite and26 CLI tests pass. The aggregate has51 optional Python dependency/asset skips, not passes. All new focused camera tests pass without skips. Actual-host integration, pure projection, installed raylib matrix comparison and dedicated backend suites pass ASan/UBSan with leak detection. The existing scene/sandbox movement regression also passes. Five camera-query exporter tests and the content regeneration check pass with the installed UnityPy environment.

The backend audit records full-route tests against387,048 snapshot triangles: sampled geometry evaluation averages about.09ms open and.61ms obstructed. These CPU measurements are not a hard frame-time bound or full-game performance claim. The live activity clock retains its previous tree-free ray callback.

Ordinary production input on private display`:103` loaded an isolated copied Adventure save in Annot. F5 cycled free third person→captured third person→first person→free third person; [captured orbit](../reference/conversation-camera/captured-camera.png) and local first-person images show rendered scene changes. Initial F5 attempts over the minimap were correctly UI-blocked; accepted mode evidence was taken after moving off it. Ordinary WASD movement approached Tammana while retaining normal reach limits; a short RMB click on her off-center visible body at approximately(898,343) opened [her menu](../reference/conversation-camera/vendor-open.png). Earlier clicks from beyond the retained interaction bound did not open it. This checks one ordinary visible-target interaction, not every tool/edge/occlusion combination.

The same session verified conversation save/Clear/load, then returned to Main Menu (sandbox result2) after12,878 display frames. The supervisor intentionally stopped the menu process with SIGTERM afterward; this is not a standalone process exit0 claim. The baseline/source save hashes and desktop socket/lock identities were unchanged. No original-game or desktop input was used. Physical wall/corner camera quality, low-frame-rate following feel and comparison against the original remain suitable user-assisted checks.
