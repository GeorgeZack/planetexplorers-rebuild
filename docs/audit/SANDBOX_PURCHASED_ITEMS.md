# Purchased equipment and deployables

This audit distinguishes source behavior, the current C integration and the next bounded implementation. The selected first slice is **Torch10**, which the original supports both as main-hand equipment and a placed light. Wood Bed283 and armor are audited dependencies, not implemented by this document. Original gameplay observations remain in [PLAYTEST](../PLAYTEST.md).

## Existing C boundary

The eight native shops transact284 source offers, preserving instance identity and durable-item wear. The314-item catalog contains93 formulas, but purchase metadata does not implement arbitrary equipment, lights, furniture, consumption or combat. `PeGame.equipped_instance_id` currently selects one owned inventory instance. [pe_game](../../rebuild/src/pe_game.c) supports the existing hand tools/limited weapon categories; [pe_sandbox_actor](../../rebuild/src/pe_sandbox_actor.c) renders only explicitly recovered scenes. Character UI shows one main-hand item; defense is currently unavailable. General armor slots and avatar-part replacement are absent.

The source ownership model is broader. [EquipmentCmpt.PutOnEquipment](../../Assets/Scripts/PeEntity/Cmpt/EquipmentCmpt.cs) verifies sex, optional skill-tree permission, removable conflicting items and return capacity. Any overlapping `equipPos` bits conflict. It returns displaced instances, applies the item's buff, adds the model and identifies bit16 as main hand. `equipReplacePos` controls avatar parts independently of occupied equipment slots. Native expansion must keep one instance owner and stage replacements atomically; merely adding an icon to a slot is insufficient.

## Torch10: selected first slice

Source database `PrototypeItem10` has type31, class77, slot16, sex0, stack1, durability1500, attack13, buff30200087 and durability decrement0. Class77 contains `Property,RecycleReplicate,Durability,Drag,Instantiate,EquipAttack`. Sherley/entity20031 sells it through store310 for70 digital currency, stock3, refresh46800 game seconds. Existing C saves can already contain the purchased instance.

The exact prefab is [torch.prefab](../../Assets/Resources/Prefab/Item/Equip/Other/torch.prefab). UnityPy inspection of that binary asset establishes:

| Field | Source value |
| --- | --- |
| Held/stowed mounts | `mountMain` / `mountBack` |
| Draw/stow animation names | `SwordPutOn` / `SwordPutOff` |
| Active/put-away action and hold mask |13 /16 /13|
| Base/held movement style |Normal0 /Sword1|
| Source visual |One MeshRenderer and one MeshFilter, preserving diffuse material |
| Flame root in item space |(0,0,.41)|
| Point-light position in item space |(0,0,.45)|
| Light range / intensity |20 /1.2|
| Light RGBA |(.955223858,.715146720,.342169732,1)|
| Physical prefab capsules |Two **trigger** capsules, radius.08, height.7, localZ axis, center(0,0,.2)|
| Drag distance |10m|
| Item bounds |Null; generic overlap/tree/entity bounds tests are skipped for this prefab |

[PETorch](../../Assets/Scripts/Items/PEEquipment/PETorch.cs) inherits PeSword but separately controls burning from the equipment hold mask. [torch.cs](../../Assets/Scripts/Lilj/torch.cs) suppresses burning when `PointInWater(position)>.5`; it does not contain a burn-down timer. The field `durDec=0` must not become invented fuel consumption. Torch combat inherits additional sword actions/contact rules and is outside the lighting slice.

[ItemDraggingTorch](../../Assets/Scripts/Items/DrawItem/ItemDragging/ItemDraggingTorch.cs) changes the dragged root to localX=-90°. [ItemScript_Torch](../../Assets/Scripts/Items/DrawItem/DragItemNonLogic/ItemScript_Torch.cs) applies that rotation on construction, enables burning and loops its `m_Colliders` array; the inspected prefab array is empty. Therefore a native placed torch must not acquire an invented solid capsule blocker. Separate picking geometry may expose interaction without changing player movement collision.

[ItemDraggingBase](../../Assets/Scripts/Items/DrawItem/ItemDragging/ItemDraggingBase.cs) raycasts up to100m against terrain, products, static scene and unwalkable layers, then compares the actual player-to-hit distance with10m. Dragging disables the source object's colliders. [ItemDraggingArticle](../../Assets/Scripts/Items/DrawItem/ItemDragging/ItemDraggingArticle.cs) creates a DragArticleAgent and removes the original stack1 instance from inventory. It creates a new item only when splitting a larger stack. [DragItemAgent](../../Assets/Scripts/Items/DrawItem/DragItemAgent.cs) saves item instance ID, agent ID, position, scale and quaternion. Generic pickup through [DragItemMousePick](../../Assets/Scripts/Items/DrawItem/DragItemMousePick/DragItemMousePick.cs) returns the same instance, but **Torch overrides it** through [DragItemMousePickItemList](../../Assets/Scripts/Items/DrawItem/DragItemMousePick/DragItemMousePickItemList.cs): the current prefab has `m_items=[{protoId:1001,count:1}]`. Get therefore adds one Charcoal1001 and destroys the original torch instance. Full capacity leaves the scene object intact. Source operations include90° turn and a timed pickup panel, with prefab interaction distance4m. The pickup UI uses a strict timer greater than2 seconds.

The bounded native target is equip/draw/stow with the purchased identity, source-mount visual and actual light; place preserving identity/wear, then Get consuming the placed torch into one Charcoal1001; save/reload across ownership locations. Placement, picking and lighting must use one exported item transform. Source draw/put-away transitions, Sword locomotion-controller layering, attack contacts, Unity particles and underwater suppression require separate evidence/implementation. Existing sampled Normal locomotion can carry the exact mount as an explicitly documented presentation approximation; it cannot establish Sword-style animation parity.

## Why armor is a separate increment

Tammana/entity20037 has42 offers including source armor and scripts. Concrete female items include Iron Helmet76 (slot1, defense28, replacement mask3073, price1185), Iron Armor99 (slot2, defense55, price1974), Iron Pants135 (slot64, defense35, price1974), and Iron Boots196 (slot128, defense7, price962). Male alternatives occupy the same slots but have different item IDs and model resources. These prices exceed the ordinary395-currency post-medicine balance; an observed purchase needs legitimate funding or a clearly identified fixture.

`EquipDefence` in [ItemObj](../../Assets/Scripts/Items/ItemObj.cs) spends `max(0,hpChange*durabilityFactor)` on damage. Buffs30200096/97/99/100 add source defense and selected comfort properties; exhausted durability prevents applying the buff. Shield34 uses a separate shield multiplier and slot512; it cannot be treated as a right-hand sword. Sex comes from source metadata, never names. `PEHelmet` attaches a rigid model to the head, while source avatar replacement can hide additional parts; replacement mask3073 must not be reduced to slot1 accidentally.

The current C actor contains baked multipart meshes for six locomotion clips and separate dig/felling/mining bodies. Faithful armor needs exported alternate body parts with the same clocks and rig mapping across **all** active body scenes, not just the idle preview. A bounded helmet or boots implementation still needs a persistent additional equipment slot, source sex/defense/replacement fields, atomic return capacity, UI ownership and correct scene replacement. Full durability effects also depend on general incoming-damage integration. This is larger than the torch's single rigid attachment.

## Wood Bed283: follow-up deployable

Sherley store744 sells Wood Bed283 for63 currency, stock2, refresh46800. Source type34/class21 is `RecycleReplicate,Drag,Instantiate`, stack1, with visual [Wooden_bed.prefab](../../Assets/Resources/Prefab/Item/Furniture/Wooden_bed.prefab) and logic [bed_logic.prefab](../../Assets/Resources/Prefab/Item/Furniture/bed_logic.prefab). The visual has one mesh/material and three enabled solid BoxColliders, all on identity local transforms:

| Collider | Center | Size |
| --- | --- | --- |
| Headboard |(-1.45,.75,0)|(.45,1.5,2.3)|
| Footboard |(1.37,.65,0)|(.44,1.3,1.95)|
| Mattress |(-.03,.54,0)|(2.3,.15,1.7)|

The logic sleep anchor is local(0,.7,0), yaw90°. Source [PESleep](../../Assets/Scripts/Operate/PESleep.cs) starts the Sleep action using buff30200055 and animation `Sleep`; bed tilt must be strictly below the configured limit (class default30°). Buff30200055 adds65 comfort every2 seconds. [SleepController](../../Assets/Scripts/ZhouXun/Voxel%20Creation/Scripts/WhiteCat/PE-VC/SleepController.cs) advances game time with the configured sleep scale (class default120), locks certain interactions, and permits cancellation after2 real seconds. Configuration prefab values still require validation before treating class defaults as runtime values.

A placed/pickable bed can reuse the torch's inventory-to-scene transfer and scene-record layer. Its generic Get returns the bed instance instead of charcoal, and its three colliders must join render/ray/movement/placement/save-spawn validation consistently. A **usable** bed additionally requires Sleep animation/placement, safe wake-up, clock acceleration, comfort state/buff and modal cancellation; instantly moving the game clock is not full source sleep behavior.

## Implemented attachment and asset boundary

[export_torch_assets.py](../../rebuild/tools/export_torch_assets.py) recovers the shared mesh/material and original flame texture, while retaining current-prefab light, particle and pickup metadata separately from the older visual bundle. [export_baked_torch.py](../../rebuild/tools/export_baked_torch.py) checks those asset hashes and current source identity before attaching the unrotated local mesh to the player's source `mountMain` or `mountBack`. It validates the mount ancestry and initial transform against the closed sampled locomotion capture, then requires every exported clip's name, duration, frame count, sample rate, loop and terminal-frame policy to match the body scene. The installed `player-torch` and `player-torch-stowed` scenes each contain one rigid part and ten source clips; only the six existing locomotion states are selected by this integration.

[pe_sandbox_actor](../../rebuild/src/pe_sandbox_actor.c) requires the complete held/stowed pair and checks locomotion clocks before GPU upload. Holding is distinct from action readiness: Torch10 can bind to the equipped instance and draw or stow, while attack clips remain rejected. The light and flame queries sample the same attachment joint and body clock, transform their separate source offsets into world space, and produce no emitter for a stowed, stale-instance or dead-player state. The host owns actual light/flame rendering and inventory/deployed ownership. Normal locomotion and immediate draw/stow attachment changes remain explicit approximations of the unported Sword controller and transitions.

The bounded actor unit test covers partial asset groups, mismatched clocks, exact instance binding, stow/hold selection, rejected attacks, emitter transforms and failed-pose output preservation. [test_baked_torch.py](../../rebuild/tests/test_baked_torch.py) checks source/hash rejection and raw local geometry, then uses the native headless PEAC loader to verify all installed Torch/body clip clocks and sampled joint points. These are exporter and headless checks, not physical lighting or source-animation parity observations; physical integration evidence belongs in [PLAYTEST](../PLAYTEST.md).

## Verification requirements

Torch acceptance should cover exact instance/wear across inventory→equipped→scene, followed by source Charcoal1001 recovery and destruction of the torch instance; rejection without mutation for missing assets, invalid reach, stale selection and full pickup inventory; no false solid player blocker; source light enabled only while held or placed; and save/load ownership uniqueness. Physical checks should use the already purchased normal-money torch, show draw/stow and visible nearby lighting, place it through UI, walk around its query-only geometry, pick it up and reload. No attack parity should be claimed from those checks. Armor and bed implementation remain separate until their explicit dependencies are satisfied.
