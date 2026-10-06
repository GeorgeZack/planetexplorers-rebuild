# Sandbox deployables: Torch 10

This increment implements one purchased deployable, source item **10 (Torch)**. Findings below are **source-derived** unless explicitly marked implemented or observed. No original-game Torch placement or recovery observation has yet established visual or interaction parity.

## Source contract

The current source prefab is `Assets/Resources/Prefab/Item/Equip/Other/torch.prefab`. PrototypeItem identifies model `Prefab/Item/Equip/Other/torch`, stack size 1, durability 1500 and equipment position 16. The exporter records database, prefab, physics-settings and script SHA-256 hashes in [generated metadata](../../rebuild/generated/pe_torch_source.json).

| Operation | Source behavior | Native implementation |
| --- | --- | --- |
| Preview/place | `ItemDraggingBase` casts a geometry ray to 100m. The hit must be within inclusive 10m Euclidean distance of player position. Torch has no `ItemDraggingBounds`. | Uses the shared terrain/building/block geometry ray, excluding trees and actors. Places exactly at its hit point, without slope, overlap, snapping or upward-settling rules. Native world bounds and a 64-object capacity apply. |
| Orientation | `ItemDraggingTorch.OnDragOut` and `ItemScript_Torch.OnConstruct` set local rotation to (-90,0,0). | Original item-local model receives the same initial rotation. |
| Turn | `DragItemAgent.Rotate` multiplies the current quaternion by a local Y quarter turn. | Four quarter-turn states; render, query capsule, flame and light use the same transform. This rotates the horizontal axis of the initially upright Torch, rather than applying a world-up rotation. |
| Selection | `MousePickable` uses 4m distance to collider bounds, with coarse root-axis checks at 5m. Capsule ray excludes origins inside it. Global selection permits an obstacle-distance +10m margin, up to the 100m query. | Analytic source capsule query and range checks. Geometry is not supplemented with an invented strict occlusion rule. |
| Get | `DragItemMousePickItemList` checks capacity, grants the `ItemScript_ItemList` items, then destroys the original item/agent. This Torch's list is **Charcoal 1001 ×1**. | The menu starts a source-timed pickup panel. Strictly after 2 seconds, atomic inventory staging consumes the deployed Torch and grants Charcoal. A full inventory leaves the Torch unchanged; the panel closes and can be reopened to retry. It does **not** return the Torch. |
| Burning | Current `torch.cs` has no fuel timer. It extinguishes when water depth exceeds .5m. | Dry-land burning; no fuel countdown. Water sampling/extinguishing remains unsupported. An obsolete 18,000 timer in the installed legacy bundle is deliberately not used. |

The two current prefab CapsuleColliders are identical triggers: local Z axis, radius .08m, height .7m, center (0,0,.2). Their GameObjects use layers 16 and 0. They supply interaction geometry, **not** movement, camera, terrain-edit or construction blockers. `ItemScript_Torch.m_Colliders` is empty. No persistent terrain-support check is added: the source object's location remains fixed when its support disappears.

Source reconstruction has a quirk: `DragArticleAgent` applies archived transform during view creation, then `ItemScript_Torch.OnConstruct` resets rotation to (-90,0,0). Native save records currently preserve the explicit quarter turn. Original reload behavior has not been physically tested; preserving turns is a documented native difference until that check or a deliberate compatibility decision.

## Ownership and transactions implemented in C

[pe_deployable](../../rebuild/src/pe_deployable.h) is independent of raylib. The saved `PeGame.deployed` array owns at most 64 records, each containing item ID, instance ID, durability, world position and quarter turn. Preview is pure. Place re-runs the ray/range check at commit time, removes the exact inventory instance, clears matching equipped/quickslot references and transfers its identity and durability into the world record. It does not create another instance, debit money, edit terrain or repair the item. Failure preserves inventory/world records.

Get stages the Charcoal addition before removing the world record. Turn and Get re-check the player's range. The state validator rejects unsupported items, duplicate IDs across inventory/deployed/equipped/quickslots, IDs outside the allocated range, invalid durability or coordinates, excessive counts, unsupported quarter turns, and nonzero unused records. Deployables are currently Adventure-only. Parent save/world-replacement integration owns serialization and disposal; transient preview/menu/render state is never serialized.

The UI now reproduces the pickup clock in `UIItemOp.GetItem/Update` and `UTimer.Update`: a Get click resets a transient timer and hides the command list. Each display frame contributes the source float delta truncated to 100,000 ticks per second. The callback runs once only after the accumulated count exceeds 200,000 ticks; exactly two seconds does not commit. Get requires a click, not a held button. Repeated Get/Turn inputs cannot restart or modify an active pickup. Capacity is checked at completion, and the source panel closes even if the inventory is full; the unchanged Torch can be reopened for another full attempt. Saving game state never stores this UI clock.

Cancellation, focus and presentation retain explicit native adapters. Escape or the visible close button, death, range loss, or host cancellation/load discard the operation before a completion-frame transaction. Focus loss and paused UI freeze the clock. Source `UIItemOp` hides when its operator disappears, and its ordinary hide resets the callback; it does not perform an equivalent pickup-range check in `Update`, and the pickup slider hides its main command/close panel. Native retains its existing modal range/liveness guards and a visible cancel control. The native progress bar uses existing UI colors/layout, not a recovered exact `ItemOpWnd` slider layout. These are source-derived and automated behaviors; an original timed pickup observation remains outstanding.

This is a bounded equipment/deployment slice. Other purchased placeable items, Torch melee attacks, fire damage, terrain ignition and destruction by combat remain unsupported.

## Recovered visuals and light

Run from the repository root with UnityPy installed in the indicated environment:

```sh
TMPDIR="$PWD/rebuild/build/compiler-tmp" /tmp/pe-asset-tools/bin/python rebuild/tools/export_torch_assets.py
TMPDIR="$PWD/rebuild/build/compiler-tmp" /tmp/pe-asset-tools/bin/python rebuild/tools/export_torch_assets.py --check
python3 rebuild/tests/test_torch_assets.py
```

`--source-only` omits installed runtime extraction. Runtime files are ignored under `rebuild/build/reference-assets/torch`; source metadata belongs in `rebuild/generated`. The installed `AssetBundles/Item/torch.unity3d` supplies the actual static mesh, material, diffuse texture and flame/smoke/rubble textures. Its legacy gameplay metadata is not authoritative. The recovered mesh contains 94 vertices and 96 triangles, in **raw item-local coordinates**, with no placement rotation baked in. Local bounds are approximately (-.033709,-.030243,-.112705) to (.035892,.028813,.457801). The current source mesh GUID could not be resolved to an included repository mesh; installed visual recovery is not a claim that every source/installed asset revision is identical.

The runtime manifest `torch.json` records the bundle mesh/material metadata, current-source metadata and individual extracted-file hashes. [export_baked_torch.py](../../rebuild/tools/export_baked_torch.py) consumes this shared model for held/stowed animation assets. There is one recovered static geometry source for both equipment and deployment.

Current-source light metadata, shared by held and placed views, is:

| Field | Source value |
| --- | --- |
| Local light point | (0,0,.449999988) |
| Range | 20m |
| Intensity | 1.20000005 |
| RGB | (.955223858,.715146720,.342169732) |
| Local flame emitter | (0,0,.409999996), distinct from light point |

[pe_deployable_view](../../rebuild/src/pe_deployable_view.h) loads the recovered model and flame texture. Complete absence of the Torch directory is optional for old asset installations; an existing incomplete directory fails loading. The renderer uses source flame rate 40/s, lifetime .15s, maximum 8 particles, size .5m with the authored zero-tangent size curve, initial color and alpha curve, source radius .01m, speed .1m/s, and gravity modifier -4 with project gravity -9.81000042m/s². The exact source curves and particle component trees are exported for further recovery.

This is a **bounded particle adapter**, not a Unity particle-system port. Native particles use a deterministic instance-based phase and radial pattern; they rebase on the current emitter instead of maintaining original world-space trails. Source random-direction/cone behavior, smoke, rubble, complete material shader behavior and blend precision remain unverified/unported. The original flame texture is used; no replacement cube or invented Torch mesh is rendered.

[pe_terrain_material_view](../../rebuild/src/pe_terrain_material_view.h) accepts all held/deployed light points in crop-local coordinates and selects the nearest eight finite points to the current camera, preserving input order for equal distances. Each light adds the source warm RGB and intensity, Lambert diffuse response and squared linear falloff to zero at the source 20m range. This attenuation is a native rendering adapter, not a recovered Unity lighting equation. The existing ambient/day baseline is unchanged; no lights restores that baseline. Static buildings, other models and construction are not illuminated by this terrain shader, and shadow casting remains unsupported. Parent host integration and physical comparison are recorded separately.

The sandbox's existing first-person mode hides its own body and equipment models. A drawn Torch continues illuminating terrain from the sampled hand mount in that mode, but its flame is hidden alongside the absent model. A dedicated first-person Torch model remains unsupported. Third-person mode draws the held model and flame together. Placement mode is Adventure-only and cannot activate during construction; missing deployment or held/stowed assets produce an explicit diagnostic instead of silently accepting the request.

## Verification recorded

Implemented headless checks in [test_deployable.c](../../rebuild/tests/test_deployable.c) cover a normal-price source-vendor purchase followed by preview/place, exact identity and durability transfer, equipped/quickslot cleanup, unchanged currency/terrain/allocation counter, inclusive 10m placement, invalid rays, stale geometry between preview and commit, trigger-only collision, source capsule entry and interior rejection, obstacle margin, 4m bounds range, quarter-turn transforms, malformed saved records, inventory-full atomic Get retry and the exact Charcoal reward. The isolated optimized runner and AddressSanitizer/UndefinedBehaviorSanitizer runner with leak detection passed. Strict warning-as-error syntax checks passed for core/view/test. Five exporter/manifest tests passed, including current input hashes and unsupported source-curve rejection. Full source/runtime `--check` regenerated matching bytes with UnityPy. These are native/source checks, not observations of the original game.

Physical host integration, save migration/round-trip checks, lighting comparison and original interaction comparison are tracked separately by the parent integration/playtest work. A loaded runtime asset is not itself proof of live parity.

October 5 continuation: repeated optimized deployable checks, AddressSanitizer/UndefinedBehaviorSanitizer checks, strict warning-as-error core/view/test syntax, five Python asset checks and complete UnityPy regeneration comparison passed. This continuation used `ASAN_OPTIONS=detect_leaks=0` because the managed sandbox does not support LeakSanitizer's process inspection; it adds no new leak-check claim. Terrain GPU-mock checks cover source light uniforms, nearest-eight selection, stable ties, invalid coordinates, clearing lights and staged load cleanup; optimized and sanitizer runs passed. `glslangValidator -l` compiled and linked the actual terrain vertex/fragment strings successfully. No Xvfb executable was available for a separate virtual-display shader run; these checks do not establish rendered pixel correctness.

October 5 pickup continuation: headless sandbox UI regressions cover the strict two-second boundary and per-frame UTimer quantization, no inventory mutation before completion, one-time completion, capacity rejection/close/reopen, repeated command suppression, invalid time, paused/unfocused time, range loss, Escape, death and host cancellation. The completion frame and held/release frames remain owned by the modal pointer gate. The optimized UI runner and ASan/UBSan runner passed, including an explicit `ASAN_OPTIONS=detect_leaks=1` run after unrestricted access was restored. Frontend strict warning-as-error syntax and changed-file whitespace checks passed. Physical progress-bar and original timing comparison remain separate work.
