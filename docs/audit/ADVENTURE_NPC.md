# Adventure supplier encounter: placement and picking

The headless encounter in `rebuild/src/pe_adventure_npc.c` represents one fixed reference supplier, Eitai, entity20016 / RandomNpc prototype22. It supplies a stable pose and interaction query to the sandbox host. Its local placement near `(32,27)` is provisional; it does not reconstruct an Adventure settlement, NPC scheduling, navigation, recruitment, shop, or saved NPC simulation. Dialogue and resource ownership remain in the Adventure gameplay/dialogue modules.

The original Adventure archive identifies this supplier as FemaleModel with a generated appearance. The archived position is `(-598.5214233398438,22.978425979614258,341.3612365722656)` and rotation identity; it cannot be copied into the bounded64×32×64 C world. Dedicated tutorial NPCs and their portraits are not interchangeable with this character. Appearance recovery is separately owned by the supplier asset exporter; the headless helper does not substitute a tutorial model.

## Source interaction contract

`Assets/Scripts/System/GameConfig.cs` defines `NPCControlDistance=4.5`. `MousePicker/MousePickableNPC.cs` assigns it as the operate distance. `MousePickable.DistanceInRange` first rejects root-axis differences outside ±5.625m, then compares the player's root with the closest point of `PeTrans.bound` using squared distance≤20.25. It is not a4.5m root sphere. `PeTrans.SetModel`, called for `View_Model_Build`, initializes these bounds from the physical `Model` subtree using `PEUtil.GetLocalColliderBoundsInChildren`, excluding trigger colliders.

`MousePickable.CollectColliders` collects child colliders; `CheckPick` returns the first collider ray hit within100m. The inspected living, idle FemaleModel has these relevant upright capsules, in source hierarchy/creation order:

| Capsule | Center relative to model root | Radius | Total height | Scope |
|---|---|---|---|---|
| Model, CapsuleCollider13600000 | `(0,.875,0)` | `.3700000047683716` |1.75| Physical bounds and grounding |
| Defence, CapsuleCollider13690738 | `(0,1,0)` |.5|2| Enabled trigger, layer25, included by collider picking |
| Generated OutCollider | Same as Model | Model radius+.1 | Model height+.2 | `BiologyViewCmpt.BuildOutCollider`, layer9 |

The helper checks these in that order, rather than choosing the nearest of the three. `PEAttackTrigger.Start` disables the limb attack triggers. The living model's ragdoll is inactive. Active fighting/ragdoll/weapon collider selection is outside this idle supplier slice. `BiologyViewCmpt` can disable the Defence trigger when injured/dead state changes; the host must reject unavailable/dead suppliers, since those states are not simulated here.

`MousePicker.GetMaxDistance` raycasts only `VFVoxelTerrain` layer12 and `SceneStatic` layer11, to100m, then adds10m to the result. This permissive obstacle allowance is preserved. It is not strict line of sight. The new `pe_sandbox_collision_ray_geometry` selects the nearest actual cached MC/Block45/furniture triangle and excludes trees, NPCs and synthetic crop boundaries. Furniture retains the current C box representation and BVH rays retain their existing two-sided behavior; full Unity collider/backface semantics are not claimed. The caller passes this nearest distance, or positive infinity for no hit, to the encounter query.

`EntityCreateMgr.NpcTalkRequest` separately rejects root distance>9m and invalid/dead/follower/busy actors. UI hover suppresses `MousePicker`; its zero delay still requires `mHoveredTime>0` after selection. There is no pre-interaction facing cone. `UINpcWnd` issues `CmdFaceToPoint(player.position)` after selecting the NPC and opening the window, with start/stop talk commands. Those permission, UI, hover, priority and talk-state gates belong to the host; the headless function returns geometric eligibility only.

## Collision policy and generated evidence

Both the physical FemaleModel player capsule and supplier capsule are on layer10 (`Player`). The source collision matrix disables10↔10. Layer8 is `AIPlayer`, and8↔10 is enabled; the old tutorial documentation describing an8↔10 exclusion was incorrect and has been corrected. Generated OutColliders use layer9 (`Show Model`), whose self-pair is disabled. `BiologyViewCmpt.ResetOutCollider` explicitly ignores NPC/player outer colliders against their physical model colliders. Consequently the supplier is not inserted into player movement solids.

`export_adventure_npc_collider.py` decodes the original binary `Assets/Resources/Prefab/PlayerPrefab/FemaleModel.prefab`, checks component IDs, active/enabled flags, trigger flags, layers, vertical axes, and identity ancestor transforms. It generates the checked-in `rebuild/generated/pe_adventure_npc_collider.h` with source SHA256 and exact hexadecimal float constants. Optional outputs are a36-byte PESC v1 physical capsule and a JSON manifest containing physical/Defence provenance. PESC layout is magic, u32version, fivef32(centerXYZ/radius/height), u32direction, u32flags; the physical supplier uses version1,direction1,flags1. The native simulation loads no Unity files or graphics.

```sh
/tmp/pe-asset-tools/bin/python rebuild/tools/export_adventure_npc_collider.py \
  --output rebuild/build/reference-assets/adventure-npc-collision
/tmp/pe-asset-tools/bin/python rebuild/tools/export_adventure_npc_collider.py \
  --check --output rebuild/build/reference-assets/adventure-npc-collision
/tmp/pe-asset-tools/bin/python rebuild/tests/test_adventure_npc_collider.py
```

The two Python checks verify generated source/header/PESC agreement and decode the original project matrix to guard the corrected layer policy. They require UnityPy; absence is not a source-validation pass.

## Bounded native placement

`PeAdventureNpc` is caller-owned transient state outside `PeGame` and saves. Zero it for a new game or load. `pe_adventure_npc_refresh` requires a successfully built collision snapshot for the same crop-local world. On first placement it tries `(32,27)` followed by deterministic half-meter rings through8m,32angles per ring. Candidate heights come from actual triangle rays, then the existing capsule settling policy verifies free space, walkable support and world bounds. Live tree capsules participate in clearance. Empty world boundary planes do not manufacture a supplier on an invisible floor.

After a collision generation changes, the helper first attempts to settle the previous position, preservingXZ. It searches again only if that position cannot settle. Resource depletion or geometry reload with a reused world revision still updates the cache generation. A failed search leaves every encounter field unchanged; the caller must suppress stale rendering/picking until refresh succeeds. Relocation and settling are bounded native presentation policies, not a source NPC physics/AI port.

The first successful pose is retained in `spawn_position`; later terrain settling or relocation changes only `position`. The gameplay arrival objective uses the immutable role2 start point (source integer-truncated coordinates, strictXZ distance<5), not the supplier's current visual position. Reloading recomputes this provisional local encounter; independent NPC save records are not implemented.

The source map label uses the current entity root and character full name. `MapCmpt` selects `PeMap.MapIcon.Npc=14`; the original `Icon` table maps that to `sign_npc` (MiniMap1, LargeMap0). `NeedArrow` is false and radius is−1. Available/completed main mission marker IDs25/26 map to `sign_task_getable`/`sign_task_cmplt` and may supersede a generic NPC marker. Visual/minimap position must follow `position`, not the frozen arrival anchor.

## Validation and remaining work

The strict C test suite `test_adventure_npc.c` verifies deterministic placement, same-generation stability, reused-revision tree insertion/depletion, non-overlap and support, unchanged arrival anchor, terrain lowering, and atomic failure with no terrain. Picking tests cover physical-first ordering, Defence-only and outer-only hits,4.5m bounds, normalized directional rays,100m range,10m obstacle allowance, interior origins, zero/nonfinite inputs and untouched miss outputs. Geometry tests distinguish furniture, Block45 and tree-excluded cached rays. The focused suite passes with AddressSanitizer, UndefinedBehaviorSanitizer and leak detection. These are headless results, not evidence that the integrated NPC has rendered or completed dialogue in C.

## Reusable source NPC window

The inspected original prefabs are `Assets/Scripts/Chenzhi/NewUI/Resources/Prefab/GameUI/NpcWnd.prefab` and `.../Prefabs/MissionSelItem_NPC.prefab`. Their old NGUI widgets encode dimensions in transform scales. `GameUISystem`'s default NPC window root is `(-320,-25)` relative to screen center. At1280×720 this gives:

| Element | Screen geometry | Source sprite/behavior |
|---|---|---|
| Window background | `(107,140,426,486)` | `midbg`; two tinted layers and source specular effect |
| Inner panel | `(120,203,400,400)` | `insidebox1` |
| Head | `(131,161,48,48)` | RandomNPC runtime `ExtGetFaceTex`; no fixed Gerdy/Andy substitution |
| Name | Left-middle `(185,184)` | Serialized label scale20, source font |
| Title strip | `(125,187,210,14)` | `titilebg` |
| Close button | `(498,159,25,25)` | `close1`, source blue tint |
| First choice text | Top-left `(146,225)` | Serialized label scale21, maximum line width360 |
| Choice marker | Center `(136,235)`,14×14 | `MainMissionGet` when that row is a mission |

The table uses one column and variable-height21; wrapped text must control actual row height and hitboxes. These are default source coordinates before resizing, user window movement, font normalization or dynamic row content. Existing recovered bottom dialogue chrome can be reused separately. The supplier choice list must expose only implemented source-backed choices; this geometry is not permission to fabricate recruitment or shop functionality.

Integrated C desktop verification subsequently passed: source menu and all five dialogue pages, visible saved appearance/portrait, cancellation without items, final grant once, and save/reload. See [PLAYTEST](../PLAYTEST.md); this does not imply a new original-game observation or full town parity.

## October 5 physical acceptance

Physical C input with no new original launch ([result](../reference/sandbox/rebuild-supplier-result.json)). The recovered Eitai Takayama (entity 20016, RandomNpc 22, mission role 2; twelve parts, 8,800 vertices) stands on supported ground near local X32/Z27.

- Arrival follows the source integer-truncated role-2 start and the strict 5 m XZ condition, and completed without any items.
- E, and free-cursor right mouse after Tab, open the window. Question 4089 leads through 4090–4094.
- Only acknowledging 4094 accepts 9032 and grants the eighteen-entry package, once. The tracker shows "Go to Annot" only after acceptance. The menu afterwards offers only "That's all.", with no repeat reward.
- Escape at the promise page cancels without a grant; this is a native choice. Background clicks advance pages.
- F9/F10 preserved the inventory, and the run recorded zero tool swings and no resource changes.
- Captures: [menu](../reference/sandbox/rebuild-supplier-menu.png), [page 4094](../reference/sandbox/rebuild-supplier-page-4094.png), [accepted tracker](../reference/sandbox/rebuild-supplier-accepted.png), [reloaded menu](../reference/sandbox/rebuild-supplier-reloaded-menu.png).

Contracts for this encounter: [dialogue](SANDBOX_DIALOGUE.md), [appearance](ADVENTURE_SUPPLIER_APPEARANCE.md).
