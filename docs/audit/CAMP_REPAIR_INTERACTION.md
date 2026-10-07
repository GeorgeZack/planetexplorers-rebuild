# Camp repair machine spatial interaction

October7,2026, US/Central. This extends the [camp repair qualification](CAMP_REPAIR.md) with source picking, exact archived geometry and a headless C interaction owner. No original-game launch, desktop input or new capture was needed. It establishes spatial admission and query results; final rendered service-window usability remains a separate integration check.

## Source identity and collider

The existing entity20006 has source position **(-613.5,29.5,404)** and archived quaternion **(0,1,0,-4.371138828673793e-8)** in both qualified archives. Its identity, model resource and creator binding are described in the preceding audit. It is already rendered and collidable in both supported crops:

| Crop | Dimensions | Source origin | Local machine root |
|---|---|---|---|
| Starting settlement |64×32×128|(-627,16,323)|(13.5,13.5,81)|
| Hunt route |416×64×416|(-640,0,64)|(26.5,29.5,340)|

The source binary `RepairMachine_Static.prefab` and installed `RepairMachine_Static.unity3d` each contain one enabled nontrigger root BoxCollider: local center **(0,.5,0)** and size **(1,1,1)**. The prefab root transform is identity, with scale one. `OperatableItemRepairMachine.Init/PostInit` explicitly resets its local scale to one. No capsule, enlarged interaction radius, shifted origin or substitute mesh is invented. The exporter retains the quaternion and checks source/installed collider and root bindings; drift fails regeneration. The installed static model remains in its previously recovered rest pose, without animated machine behavior.

## Active picking and window distance

[MousePickableChildCollider.OnStart](../../Assets/Scripts/MousePicker/MousePickableChildCollider.cs) calls [MousePickable.CollectColliders](../../Assets/Scripts/MousePicker/MousePickable.cs). That method uses `GetComponent<PeTrans>()` on the machine root, not its entity parent. The qualified prefab has no root PeTrans; the root box therefore supplies the distance bounds. Its enabled status and the single collider are independently checked.

The current C# `MousePickable.operateDistance` initializer is **19m**. Neither the source nor installed serialized operation component contains an override. This is source-derived behavior, not a runtime measurement of the installed executable. Admission first rejects an axis displacement from the root greater than **19×1.25m**, then requires squared distance from the player's position to `Collider.bounds` to be **≤19²**. `CheckPick` casts against the collected collider with **100m** maximum ray distance. The unused `OperatableItem.SqrOperateMaxDistance=6*6` never participates in this path.

[MousePicker](../../Assets/Scripts/MousePicker/MousePicker.cs) returns early when disabled or `gamePause` is true, rejects picking under `UICamera.hoveredObject`, and uses `PeCamera.mouseRay`. The repair object's inherited priority is Level0. Nearest hits win within a priority; a newly selected object's zero hover-delay still requires positive elapsed hover time. The global obstruction ray uses VFVoxelTerrain and SceneStatic layers, but returns **nearest obstruction distance+10m**; without an obstruction this is110m, while the individual collider still limits its ray to100m. Consequently the original source can admit an object behind some intervening geometry. This allowance is not silently treated as strict visibility.

[OperatableItem.CheckOperate](../../Assets/Scripts/Items/OperatableItem/OperatableItem.cs) invokes `Operate()` for either `InteractWithItem` or `OpenItemMenu`; the latter also dispatches the inherited right-click event. [PeInput](../../Assets/Scripts/Input/PeInput.cs) and [settings](../../Assets/Scripts/Input/PeInputSettings.cs) bind these by default to E and RMB. [PeInputExt](../../Assets/Scripts/Input/PeInputExt.cs) makes E a release within .5s; RMB releases require displacement<20px and duration<.25s. The existing native E press and3px/.25s camera gesture remain explicit shared-input adapters. This module does not change them or duplicate pointer ownership.

[UIRepairWnd.Update](../../Assets/Scripts/Chenzhi/NewUI/Scripts/PE_GameUI/NGUIScripts/UIRepairWnd.cs) closes the window only when the player's **three-dimensional root distance is strictly greater than8m**. Exactly8m stays open. Closing releases the UI binding; it does not cancel or destroy the machine's inserted item/job.

## Native owner and explicit adapters

[pe_camp_repair_interaction](../../rebuild/src/pe_camp_repair_interaction.h) exposes `present`, `in_range` and `pick`. It is independent of raylib and mutates neither the game nor collision cache. Presence requires Adventure mode, an exact supported crop/source origin, a matching loaded settlement model, and an initialized collision snapshot. It returns the machine root in crop-local coordinates. Missing model readiness, unsupported worlds and missing/uninitialized snapshots fail closed.

**The host owns snapshot freshness and model identity.** A nonzero snapshot generation proves initialization, not that an arbitrary snapshot belongs to the supplied current game. The host must refresh successful geometry changes, pass the corresponding crop's loaded model state, and stop admission after a failed refresh. These helpers perform no file loading and create no machine in another world.

`in_range` uses finite living player coordinates, blocks the get-up phase, and preserves the inclusive8m root boundary. `pick` additionally normalizes a finite nonzero ray, intersects the exact rotated source box and enforces100m plus the source collider-reach check. Rays originating inside/on the box fail closed. Its returned distance is in meters, fraction is distance/100, and triangle is `UINT32_MAX` because this is a collider hit rather than a mesh triangle index.

Two stricter native policies are intentional:

- Admission uses the same **≤8m** root limit as window retention, preventing the source's open-then-immediately-close behavior at longer distances. The source19m box-distance condition remains part of the qualified pick predicate.
- The current tree-excluding geometry snapshot supplies actual obstruction distance. Any obstruction earlier than the candidate by more than **.0001m** rejects it; the source's +10m allowance is omitted. The .1mm equality tolerance resolves floating-point ties between the source box and its baked scene triangles. It never skips an arbitrary earlier object or disregards all machine-area triangles. Terrain, construction and static geometry participate; NPCs, trees and synthetic crop-boundary planes do not. This uses the already documented native layer/query adapter rather than claiming every original Unity layer is reconstructed.

Host-level UI hover, focus/pause, nearest-target ordering, action cancellation, mode/load boundaries and held-pointer ownership remain with the existing sandbox input owner. Spatial helpers cannot open a modal by themselves or cancel a repair transaction merely because the player walks away.

## Verification

[test_camp_repair_interaction.c](../../rebuild/tests/test_camp_repair_interaction.c) exercises both crop transforms, invalid/missing readiness, unsupported world/source/mode, six box faces, nonnormalized rays, strict8m boundary including one-float steps outside, death/get-up, NaN/Inf/zero rays, inside-box rejection,100m ray boundary, real geometry occlusion and a .3mm foreground blocker. It compares game bytes and collision generation before/after repeated queries. Synthetic checks use the actual native collision snapshot and triangle backend, not a stubbed obstacle answer.

Its optional `--installed` check loads the existing terrain and static collision assets, without a graphics context or changing a save. Both installed crops produce these same results:

| Ray origin relative to root; aim at box center | Actual snapshot hit | Repair selection |
|---|---:|---|
|(+4,.5,0)|1m, intervening geometry|Rejected|
|(−4,.5,0)|3.5m|Accepted,3.5m|
|(0,.5,+4)|3.5m|Accepted,3.5m|
|(0,.5,−4)|3.5m|Accepted,3.5m|
|(0,+4,0)|3m|Accepted,3m|

The accepted rays match the actual machine's static surface within .0001m; the east-side ray proves that an earlier real object is not mistakenly treated as the machine's own collider. This is a CPU geometry check, not an on-screen interaction observation.

The optimized test, including both installed crops, passes. The isolated UnityPy exporter suite passes11 tests with zero skips, covering regeneration, source drift, exact geometry, extra colliders, root offsets and range overrides. The same synthetic and installed-crop suite passes ASan/UBSan with leak detection (`build/camp-repair-interaction-sanitize.log`). Relevant generated files remain exporter-owned; original assets and archives are read-only evidence.
