# Sandbox camera: source evidence and integration boundary

Subsequent implementation: [activity-space and obstruction recovery](CAMERA_ENVIRONMENT.md) now supplies source sampling, target scaling and the two-second recovery clock in Adventure, using the existing tree-free snapshot. The framing and capsule geometry below remain native adapters; full near-plane/ring/YawPitchFinding behavior is still unported. This supersedes the earlier missing-activity/lock statements while preserving their evidence.

This is a source-only investigation prompted by recovered tree foliage obscuring the provisional sandbox spawn. It records the supplied project's active CameraForge path, not a new original-client observation. The subsequent bounded host change in `pe_sandbox_view.c` adds explicit third-person wheel zoom: default6m, bounds1.8–10m normally and1.8–20m in construction. Its .8m-per-raylib-notch conversion is an adapter assumption derived below. The host retains a provisional eye+2×direction look target, .5m camera rise and .1m capsule sweep against the sandbox movement snapshot. It does not evaluate the complete source camera graph, activity-space search or near-plane geometry. This audit made no tree-collision or fading change; graphical verification of zoom is separate.

## Vegetation is excluded from the source obstacle mask

[PeCamera.cs](../../Assets/PeCamera/Scripts/PeCamera.cs) sets `Obstacle LayerMask` in `Start` and defines it near line467. Active layers are Default0, SceneStatic11, VFVoxelTerrain12, Building13, Unwalkable16 and ProxyPlayer23: integer mask8468481. **TreeStatic14, GIEProductLayer19 and NearTreePhysics21 are explicitly commented out.** `Pathea.Layer` constants live in `Assets/Scripts/ZhouXun/Voxel Creation/Scripts/WhiteCat/PE-VC/Layer.cs`.

`Assets/Scripts/ZhouXun/Layered SubTerrain/LSubTerrainMgr.cs` defines NearTreeLayer21 and assigns that layer to nearby instantiated tree colliders in `RefreshTreeGos`. The recovered tree11_grassland capsule supports axe interaction; putting it into camera obstacle queries would not match this camera mask. Player collision has its own mask: `Motion_Move_Motor.cs` includes NearTreePhysics. Camera occlusion, physical movement and resource selection therefore need separate query categories.

The inspected ThirdPersonFollow/GeometryClamp/YawPitchFinding camera path contains no foliage material fading. `TreeLight`'s distance fade controls lights and is not evidence for transparent leaves. This bounded audit does not prove that every shader or alternate camera path lacks fading; it supplies no basis for inventing such a solution. The repeated tree mesh, provisional positions/scale and source species generation remain separate causes of differing visibility.

## Wheel zoom and original input ownership

The binary camera assets were decoded using their own `ControllerAsset`/`PoseNode`/`Var` serialization format. `PECamera_Start`'s Init modifier sets `Dist`, `DistWanted` and `DistLevel` to6m, yaw30° and pitch−20°. The current source assets have SHA256:

- `Assets/PeCamera/Resources/Controllers/PECamera_Start.asset`: `e1956bec6ee32bc4c9e304f05211b9114adc2f4d7f44c784fa6fddc206650694`.
- `Assets/PeCamera/Resources/Controllers/PECamera_Update.asset`: `253ba07513eae22cf3f4e404d63a9942ddea7c13bfb12215bd476000f3760938`.

In the update graph, pose node1 is normal ThirdPersonFollow. Its serialized minimum is1.7999999523m, and maximum is input node7, `Lerp(10,20,Build Mode)`: normal1.8–10m, build1.8–20m. Damp Rate is approximately.4; pitch bounds are−70°..55°. Follow Speed takes Camera Inertia or default7.5 and becomes0 in build mode. Shovel pose node16 has its own maximum10m, damp.3, follow speed0 and unlocked cursor. Do not replace these graph values indiscriminately with the class constructor defaults.

[ThirdPersonFollow.cs](../../Assets/PeCamera/Scripts/ScriptModifier/ThirdPersonFollow.cs) reads `Mouse ScrollWheel`, then applies:

```text
wheel = MouseOnScroll ? 0 : InputModule.Axis("Mouse ScrollWheel")
dw = -wheel * 8
DistLevel = clamp(DistLevel + dw, DistanceMin, DistanceMax)
```

There is no delta-time multiplier on this increment. Rotation separately requires the prior pose's cursor lock or right mouse without `Mouse Op GUI`; wheel zoom is outside that rotation condition. `ProjectSettings/InputManager.asset` supplies Mouse ScrollWheel sensitivity .10000000149, mouse-movement type1, axis2, zero gravity/deadzone and invert=false (SHA256 `90b6d7956a121f2b78d4d85f8b15ac837ae81e3e7524c54536aaa84a6c6b7677`). A Unity axis value+.1 therefore reduces the distance level by.8m. Mapping one raylib wheel unit to that Unity value is an input-adapter assumption until measured, not an observed hardware calibration.

There are two UI/input gates:

1. [PeInput.cs](../../Assets/Scripts/Input/PeInput.cs) registers `InputModule.SetAxis("Mouse ScrollWheel", GetMouseWheel)` in its static constructor. GetMouseWheel returns0 if global InputEnable fails or `MouseExcluder()` reports `UIMouseEvent.opAnyGUI`; otherwise it returns `Input.GetAxis`. InputEnable includes global disable, cutscene/player animation, story pause, creation editor and text-input focus restrictions.
2. `PeCamera.Update` sets `Mouse On Scroll` from [UIMouseEvent.opAnyScroll](../../Assets/Scripts/Chenzhi/NewUI/Scripts/UIComponent/UIMouseEvent.cs). That helper raycasts512m on GUI layer and checks `UIOnScrollMouse`; while a mouse button is held it retains the previous cached value.

With ordinary UI closed and no exclusion, third-person wheel zoom is enabled. This is hover/focus/input ownership, not simply a boolean that any visible panel disables every scroll. A native host may conservatively consume all modal input, but should label that difference.

## Distance adjustment and collision ordering

Changing DistLevel is only the first part of original zoom. `PeCamera.Update` evaluates activity space from the actual camera anchor with min.5, max50, upward weighting and accuracy4, using the same obstacle mask. `Assets/Camera Forge/Scripts/Utilities/Utils.cs::EvaluateActivitySpaceSize` samples rays, weights their distance and upward direction, and returns the greater of minimum hit distance and half the weighted average. The function receives `min` but does not clamp its return to that argument.

ThirdPersonFollow computes `distcoef=clamp(ActivitySpace*.15,.15,1)`, then `DistWanted=clamp(DistLevel*distcoef,DistanceMin,clamp(ActivitySpace*2,DistanceMin,DistanceMax))`. Actual distance uses `SmoothDamp` with .5s smooth time. A geometry clamp sets a two-second recovery lock; during that lock distance only recovers toward the minimum if shorter than it. New wheel input clears that lock. The source target is `Anchor.position` (fallback Default Anchor), and final position is `target−distance*forward`; the calculated offset is not applied in the active assignment. Rotation uses Euler(−pitch,yaw,roll), preserving Unity world coordinates.

[GeometryClamp.cs](../../Assets/PeCamera/Scripts/ScriptModifier/GeometryClamp.cs) follows normal pose node1 through Set Cursor node24, before YawPitchFinding node4 and the normal third-person blend. It:

- Scales near clip by `clamp(ActivitySpace*.2,.4,1)`; derives near-plane radius from FOV/aspect and bounds it using overlap queries with a .05m minimum and .005m binary-search tolerance.
- Tests target-to-eye visibility from center and rings at1.5m/3m. It skips distance shortening when an unobstructed offset sightline exists and the eye sphere is clear. Preserve the actual loop branching: radius0 and1.5 each run4 iterations, radius3 runs8; the apparent `n=1` branch is unreachable beneath the earlier `r<2` test.
- Otherwise sphere-casts toward the eye with radius`NCR−.01`, moves the near-plane eye to the hit, zeros distance velocity and raises `Geometry Clampd`. All these physics calls explicitly ignore triggers.
- Computes a bounded short-distance rise along `up+forward*.3`, using a second sphere cast of radius`NCR−.02`, then lerps rise by.1; shooting disables that rise.

[YawPitchFinding.cs](../../Assets/PeCamera/Scripts/ScriptModifier/YawPitchFinding.cs) is enabled by `!Build Mode` in the graph. It waits until No Rotate Time≥.3; sustained obstruction increments hideTime by literal.02 per evaluation. After hideTime>.2 it tests5°..90° rings in5° increments, with theta0..180° in45° steps, rejects pitch<−70°, and writes smoothed wanted yaw/pitch for a clear sphere-cast direction. This is not a foliage-specific response and is frame-dependent in the source. It uses the same tree-excluding obstacle mask.

## Bounded next integration

The implemented first slice uses explicit zoom state with the verified default/bounds/sign while retaining the native collision approximation. Zoom is gated by the host's `world_input` and third-person mode; all modal panels block it, which is stricter than the original hovered-GUI/input gates. The first host slice used immediate distance changes. The new headless smoothing helper below implements the verified scalar SmoothDamp step; activity-space scaling and two-second geometry-clamp recovery remain unported. Validate normal versus build maximum, GUI-consumed wheel, first-person isolation, pause/load behavior and no camera-dependent tool reach extension. A fuller port needs layered obstacle queries, source anchor/FOV/near clip, activity-space evaluation, damped distance and clamp recovery, followed by geometry and yaw/pitch finding in the serialized order. Fixtures should distinguish a terrain wall, a near-tree collider and leaf cards: the latter two must not silently become original camera blockers. None of this authorizes moving saved trees or fading their leaves to hide a provisional spawn issue.


## Verified scalar zoom helper

`rebuild/src/pe_sandbox_camera.c/h` now separates desired distance, smoothed distance and velocity. `pe_sandbox_camera_init` sets6/6/0. `pe_sandbox_camera_step` accepts scaled frame time, wheel notches and build/third-person/input/paused flags. Call it once per displayed frame, so a wheel edge is not multiplied by fixed-step catch-up. Its distance is **before** the host obstruction adjustment; geometry hits must not overwrite desired distance or invent source clamp recovery. Host integration remains separately owned by `pe_sandbox_view.c`.

The smoothing calculation is independently verified against the actual installed Unity5.2.4f1 `PE_Client_Data/Managed/UnityEngine.dll`, SHA256 `f728161940fbb8f27659c979bfcabe3f544002858779abfaa1143065926c051b`. The six-argument `UnityEngine.Mathf.SmoothDamp` method is token`0x0600063f`; its163 IL bytes have SHA256 `445bedb062f7af1e8d860bd949f5a8b9660f0114e21a91a30180b72533050ed2`. The four-argument overload used by ThirdPersonFollow supplies positive-infinite maximum speed and `Time.deltaTime` to this method. The C helper specializes its arithmetic for smoothTime=.5 and unlimited speed, preserving the rational decay coefficients .48/.235, velocity update, evaluation order and strict overshoot comparison.

[Unity's primary C# reference](https://github.com/Unity-Technologies/UnityCsReference/blob/master/Runtime/Export/Math/Mathf.cs) provides a readable contemporary counterpart. The installed IL and executed outputs establish the older version used here: notably its overshoot branch divides by deltaTime without the modern zero-time guard. The native helper deliberately freezes on dt0 and avoids invoking that undefined0/0 edge. It does not claim bitwise cross-platform JIT/FMA equality or reproduce all CameraForge behavior.

The new [offline oracle](../../rebuild/tools/reference_smoothdamp.c) invokes only the explicit six-argument managed method, supplying its own deltaTime. It does not load the game executable, Assembly-CSharp, scenes, graphics or Time getter. Reproduce its outputs with the local installed assembly:

```sh
TMPDIR="$PWD/rebuild/build/compiler-tmp" cc -std=c11 -Wall -Wextra -Werror \
  rebuild/tools/reference_smoothdamp.c -ldl -lm \
  -o rebuild/build/reference_smoothdamp
rebuild/build/reference_smoothdamp \
  "$HOME/.local/share/Steam/steamapps/common/Planet Explorers/PE_Client_Data/Mono/x86_64/libmono.so" \
  "$HOME/.local/share/Steam/steamapps/common/Planet Explorers/PE_Client_Data/Managed"
```

Twelve independent outputs are retained in [test_sandbox_camera.c](../../rebuild/tests/test_sandbox_camera.c), covering1ms–1s steps,30/60/120Hz sequences, overshoot, initial velocity opposing the target and the source's equal-target/nonzero-velocity behavior. For example,6→5.2 at1/60s starts at5.99835110 with velocity−.199587852; after60 steps distance is5.27354860. The implementation passes those reference values with explicit floating-point tolerances. The rational integrator is not exactly frame-partition invariant; the tests compare each interval against its own Unity result rather than imposing that false requirement.

Headless strict C11 and ASan/UBSan/leak tests passed. Additional tests verify input-blocked smoothing, pause/dt0/first-person freeze, finite huge-wheel clamping, nonfinite/error atomicity, no target overshoot, source mask exclusions and mode changes. Leaving construction clamps both distances to10 even while paused or in first person; an outward velocity at a bound is cancelled, while inward velocity survives. That immediate range enforcement is an explicit native host policy. Modal input blocks new wheel deltas but permits existing smoothing to settle unless paused. The helper accepts finite dt in[0,1] without a hidden dt clamp; the host remains responsible for its existing frame-time cap. These checks do not establish a new graphical test or original live camera parity.

Subsequent October5 host verification passed with actual XTest wheel input. Three upward notches changed desired distance6→3.6m; the rendered camera settled to3.6m. Three more notches while the character panel was open left desired distance unchanged. Before/after captures and final host telemetry are retained in the [playtest record](../PLAYTEST.md), with the reproducible [tree/migration/zoom desktop harness](../../rebuild/tools/playtest_sandbox_trees.py). This establishes native integration and input ownership; original camera parity remains limited to the separately verified scalar math and source bounds.

## Tutorial-compatible sandbox input policy

The headless `PeSandboxCameraControls` policy now uses the tutorial's mode order: 0 third person with a free pointer (default), 1 third person with a locked pointer, and 2 first person. F5 cycles that order and clears the effective Tab override. The existing UI remains the sole owner of the Tab/free-cursor flag; the host remains the sole owner of actual OS cursor capture and its warp suppression. This is a native tutorial-control alignment, not recovery of the full Unity CameraForge mode graph.

In mode0, a fresh world RMB press starts a gesture while preserving the screen pointer. Capture begins only after at least3 pixels of accumulated motion or a hold of at least.25 seconds; releasing restores the free pointer. A shorter stationary click never captures or recenters the pointer, preserving its NPC interaction ray. This delayed transition is an explicit native adapter; the tutorial currently captures immediately on press. World movement, action eligibility and pointer rays do not require capture. Pointer hover blocks a new camera gesture/capture, while the host decides keyboard and modal ownership separately. The existing UI press/hold/release gate must run first and supply `input_allowed`; an ordinary visible free pointer is not itself a modal. Blocking input or losing focus cancels a pending RMB click. The policy does not accept an inherited held button as a fresh mode0 drag.

The tutorial stows held tools on a release strictly less than .25 seconds after the press. The shared policy preserves that cutoff and additionally rejects a gesture after at least3 pixels of accumulated motion, including backtracking. This3px tolerance and delayed capture are explicit native input adapters to separate a quick camera drag from a click; they are not observed original-game thresholds or identical tutorial capture transitions. Hosts consume the resulting release click for interaction/stow, instead of also consuming the original press. Host warp-suppressed frames must supply zero motion to the classifier.

Focused headless tests cover default free-pointer movement, all three modes, F5/Tab precedence, capture transitions, hover, short clicks versus quick/long drags, missing releases, focus loss and modal ownership through release. Optimized checks and strict C11 ASan/UBSan pass. Graphical integration and physical behavior remain separately verified by the host operator.


October6 native host verification now passes free-pointer WASD, a short NPC RMB click, F5 through all three modes, held-RMB relative look and pointer restoration after release. The short-click test exposed the initial immediate-capture warp and passed after delayed admission. [Free-camera NPC interaction](../reference/sandbox-free-camera-npc.png), [first person](../reference/sandbox-camera-firstperson.png), and [free-camera orbit](../reference/sandbox-free-camera-orbit.png) are the final-build captures. Full core/UI tests and sanitizer checks pass. These are native integration observations; full CameraForge parity remains unclaimed. A subsequent headless regression fix permits F5 to clear the Tab cursor override directly. UI routing now reports camera-cycle admission before applying the Tab-only gameplay block; actual panels, their opening/closing frames, HUD activity and save/load requests still block that admission. The host additionally preserves pointer-held/release ownership, focus, resize and death gates. An accepted cycle consumes movement, tools and NPC interaction for that frame and clears the Tab override. Focused UI and camera policy tests pass; the subsequent physical Tab/F5 and inventory-precedence check below also passes.

## October6 Tab/F5 physical follow-up

Native PID1371835 physically passed captured-third-person→Tab pointer override→F5 first person. Inventory-owned F5 did not cycle; closing inventory retained first person. A final F5 restored free third person (exit log mode0). See [PLAYTEST](../PLAYTEST.md#october-6--med-pack925-pecsv13-and-tabf5-verification) for the exact session and limits.
