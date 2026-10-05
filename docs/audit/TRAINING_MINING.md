# Training751: shovel action and real terrain edits

This is the source/installed-asset audit for the implemented `PeTrainingDig` action host and the `PeTraining`751→752 bridge. Headless strict C11 and sanitizer tests pass; original-runtime dig event dispatch remains unobserved. The726 authored mound samples and mission accounting remain in [the gathering audit](TRAINING_GATHERING.md). The generic sandbox already damages terrain, but its temporary cooldown is not a faithful shovel animation clock.

## Verified equipment

Source `Assets/Resources/Prefab/Item/Equip/Weapon/Spade/wood_spade.prefab`, component11483140, matches the installed ResourceManager resource `prefab/item/equip/weapon/spade/wood_spade`: GameObject18710, PEDigTool356236. Installed custom fields were decoded using the actual player header plus the source custom schema with strict full-byte consumption; relevant scalar/mount values agree. This compares serialized data, not decompiled installed action code.

| Field | Serialized value |
| --- | --- |
| Item / action skill | Wood Shovel12 /20110001 |
| Animator bool / speed | `Spade` /1 |
| Stamina per charge | **4**, overriding the class initializer5 |
| Slowdown thresholds |40 and20, each multiplies speed by.9 after the charge |
| Held / stowed mount | `mountMain` / `mountBack` |
| Draw / stow animation | `MinePutOn` / `MinePutOff` |
| Held mask / digging mask | EquipmentHold13 / Dig27 |
| Aim IK | `m_ApplyAimIK=false`, `m_SyncIKWhenAnim=true` |
| Equipment position |528, sharing bit16 with knife33 and axe1527 |
| Durability / attack wear |3553 /5 |
| Item Atk / ResDamage / ResRange |23 /0 /0 |

The item uses buff30200091 to add its attack to the player's base10: effective attack33. Base ResRange.49 remains. The indicator's serialized radius1.94 is overwritten every `PEDigTool.Update` from the entity ResRange; it is not the live tool footprint or reach. The current generic content/sandbox stamina5 and temporary.5-second cooldown must not be copied into this lesson.

Inspection outputs are ignored `rebuild/build/wood-shovel-source.json`, `wood-shovel-installed.json` and `wood-shovel-clips.json`. Source prefab SHA256 is `04216f14143156cfea9d4a5088198f5d6ad4f12c16955111aecadbd246cef71e`; action SHA256 is `c8c82a6d1b1d35074990585914dce7bb895c16e6ad02824476f7063d556a812a`.

## Authored clips and controller transitions

Installed `PE_Client_Data/sharedassets1.assets` AnimatorController779 (`FemaleAnimator2`) has direct leaves `FullBody.Dig.Spade1/2/3`. Male override controller780 substitutes only the loop among these three. Times below are authored clip seconds, not wall time.

| Clip | Female ID | Male ID | Length | Loop | State speed | Events |
| --- | ---: | ---: | ---: | --- | ---: | --- |
| Spade1 |189 |189 |.6999969482 |No |2 |None |
| Spade2 |201 |208 |Female1.3333358765 / male1.6000003815 |Yes |1 |See below |
| Spade3 |211 |211 |.8333358765 |No |1 |None |

Female Spade2 calls `AnimEvent("ChangeTarget")` at.0488018654 and `AnimEvent("DigTerrain")` at.6035147905. Male calls those at.1286661327 and.6481015086. These are actual serialized clip events; there is no nominal skill cooldown to substitute for them.

Spade1 transitions to Spade2 at normalized exit.6 with fixed transition duration.4 and offset0. Spade2 transitions to Spade3 when `Spade=false`, normalized exit.8, fixed transition duration.2 and offset0. Spade3 exits at normalized.9 with non-fixed transition duration.1. Its StateMachineBehaviour1241 is `AnimatorEventSender` (script1916 in sharedassets0), with empty enter string, exit string `DigEnd` and zero timed events. It therefore emits DigEnd **on state exit**, not at an invented clip event time. The source sender SHA256 is `8a093e1e69c6b062cadf72def019d79d74c12abc6e73518ad0c59e168dbf0012`.

Transition blending, source entry conditions and Unity's incoming-clip event evaluation still need either explicit controller emulation or a measured live-controller trace. Isolated clip pose baking establishes the bone pose at authored time, not live transition/event dispatch time. A simple entry-length+loop scheduler would ignore the verified early exits and blends; do not present it as exact timing.

## Source action semantics

`Action_DigTerrain.cs:56–78` requires an equipped, nonbroken tool and an active indicator. Reach is strict distance from **player feet+up1** to the snapped sample `<3m`. It is not camera distance and not5m. `DigVoxelIndicator` tries density thresholds1,11,…131, checks a solid axial neighbor for sub-isosurface samples, and has the shifted-camera-origin fallback. Its `show` and UI conditions control presentation. Reuse `pe_world_dig_target` with crop-local ray/player coordinates; that helper preserves source-origin rounding conventions.

`DoAction` captures the first target, raises Dig mask, sets the Spade bool, adds the downward physics subacceleration and charges4 stamina immediately. It allows low stamina; after every charge the value is clamped and animation speed becomes1, .9 or.81 at the two thresholds, finally clamped to[.5,1.5]. While the action remains active, `Update` refreshes the consumption timestamp, including its ending tail. Source recovery therefore waits until the action ends and the normal minimum.1-second delay elapses.

On `ChangeTarget`, the first locked target is retained; later swings update the target only if the indicator is valid. On `DigTerrain`, the first swing can run even if the indicator subsequently becomes invalid. Later swings require a valid indicator and refresh the target before firing skill20110001. The event then sends `Battle_EquipAttack`, which consumes the item's5 durability. A continuing action charges the next4 stamina at that hit event; if already ending it does not. A valid attempted swing at an empty fallback can still wear the tool/charge stamina even when no voxel is removed; the generic sandbox's no-spend empty-sample rejection differs.

Release sets `Spade=false` and starts a5-second end fallback. `DigEnd`/`OnEndFullAnim` ends the action earlier. Immediate cancellation resets the FullBody state and clears masks/subacceleration. These release/action-tail semantics must remain distinct from the inventory/terrain transaction: a failed capacity reservation should not silently invent successful resource acquisition.

Skill20110001 has zero own/shared cooldown, trigger233 and zero timing phases. Trigger233 modifies target HP through `TryGetHurt`; the terrain HP callback invokes `ProcDigVoxels`. `SkAliveEntity.digPosType` marks actual Dig actions as resource-returning (`w=1`). The resulting terrain damage is `(10+23)*(1+0)*5=165` before material attenuation. The generic combat target-defence formula inside trigger233 is not the final voxel-density damage formula.

## Exact material1 edit and lesson accounting

`DigTerrainManager.DigTerrain` loops x/z from−radius to+radius in unit increments, y from0 through ResDamage. Radius.49 and height0 select one sample. `IntVector3(float,float,float)` rounds with its+.001 bias; this is not floor sampling. Material1 duration.21167 has no attenuation for damage165; its byte damage is floor(165*.21167)=34. Positive densities at/below127 are removed after damage. Representative uninterrupted strikes to removal are density255:4,210:3,190:2,130:1,110:1. These counts describe the selected sample only, not a fixed number of required input clicks.

Each removed material1 sample yields **two dirt268**, with fixed_num0, self_num2 and production268@100. Reducing density without removal yields nothing. The mission requires at least five dirt, so three distinct valid sample removals can produce six; they may require different numbers of actual DigTerrain events. Preserve `PeTrainingGathering.removed_voxel`'s before/after identity, changed revision, material, density and coordinate replay checks. Do not independently add dirt through both `PeGame.inventory` and the lesson ledger.

The mound uses absolute coordinates including negative x. A bounded world must choose an origin covering all726 samples, translate room-world queries/rendering consistently, and combine its mutable collision with the static room. Entry overlays those samples; exit clears the authored coordinates rather than restoring an invented pristine floor. Do not grant dirt from clicks while leaving the mound decorative.

## Implemented core boundary

[pe_training_dig.c](../../rebuild/src/pe_training_dig.c) advances one fixed1/60 scaled-game-second tick, exposes `Spade1/2/3` and authored clip time, and owns movement/stamina locks. The scheduler uses the verified entry normalized.6 exit, loop event times, normalized.8 release and complete exit clip. It **does not reproduce Unity transition blends**, incoming-event evaluation, AnyState entry delay, IK or moving/first-person digging. The source events are exact; their absolute action start times in this discrete scheduler are an approximation, not measured original dispatch parity. Female and male loops have separate event schedules; the current graphical player uses the verified female pose assets.

Each DigTerrain event stages the density edit and the one ledger receipt before commit. Material1 loses34 density per strike, yields two dirt only on removal, and accepts low stamina with source slowdown rather than a hard gate. A capacity-blocked removal consumes that animation opportunity but preserves terrain, wear and the following stamina charge; this is an explicit conservation repair. The initial action's already-spent stamina remains spent. First-target locking, later indicator refresh, empty-target wear, release tails and invalid-input atomicity have dedicated regressions.

`PeTraining` owns two separate stable shovel instances, both initially3553 durability, and equips one by instance index. Equipping replaces knife/axe ownership in the shared equipment slot without deleting items. On five-or-more earned dirt, a staged751→752 acceptance clears the exact726 authored coordinates, preserves unrelated voxels, grants9000 dirt once and presents4074/300244/300245. A blocked successor grant retains completed earned output and retries acceptance only. Three real sample removals normally leave9006 dirt; the number of DigTerrain events depends on the chosen densities.

The frontend rebuilds both marching-cubes rendering and its immutable collision snapshot after a world revision and before physics. Room, mound and dynamic build collision are combined by earliest sweep hit. A bounded source-independent overlap recovery raises the player in.25m increments up to4m without crossing a room ceiling, then settles on the earliest walkable surface. At a steep slab edge without walkable support it can keep the first clear raised position airborne and allow ordinary gravity/slide to continue. It preserves horizontal position and fails without mutation; it is not Unity depenetration parity.

Tests cover female/male event boundaries, strict3m body reach, first-target locking, UI/FP/movement/broken-tool gates, low stamina and recovery locking, all five representative density counts, partial edits with no yield, real three-sample completion, capacity rollback, separate shovel wear, invalid inputs, replay prevention, exact mound cleanup, unrelated-voxel preservation and recovery success/failure. See [training terrain](TRAINING_TERRAIN.md) for the authored fixture and collision provenance. The source receiver puzzle following752 is audited separately in [training building](TRAINING_BUILDING.md).
