# Gameplay/domain replication audit

Audit method: static source inspection only. Facts below are verified in checked-in C#; runtime behavior, all tuning, and scene-specific activation remain unverified until a working reference build or extracted data confirms them. Line numbers refer to this checkout. Recommendations are explicitly labeled and are not claims about the original game.

## Product modes and game loop

`Assets/PeLauncher/PeGameMgr.cs:7` declares player types Single, Multiple, Creation, Tutorial; scene modes Story, Adventure, Build, TowerDefense, Custom, Tutorial; multiplayer game types Cooperation, VS, Survive. Difficulty exposes Easy and Normal. Settings include unlimited resources and monsters enabled. Tutorial categories are GatherCut, Replicate, DigBuild. These enum entries establish supported concepts, not proof every combination is shipped/working.

Source-backed core activities are exploration, gathering, felling, terrain digging, replication/crafting, construction, weapon combat, missions, followers and colony management. `Assets/Scripts/PeEntity/Cmpt/Motion/PEAction/PEAction.cs:8` enumerates movement, sprint, rotation, jump, damage reactions, death/revive, melee/shield, gun/reload, bow, sleep/eat, dig/fell/gather/pickup, drive, jetpack/parachute/glider/climb/build, pumping/drawing water, seats/operations, train, cutscenes, cure, carrying interactions, rope gun, abnormalities and ride. Do not equate a simple voxel sandbox with complete Planet Explorers replication.

Startup intentionally withholds player control until terrain collision, player entity and movement component exist (`PeGameMgr.Run`, same file around 177). Single-player starts autosave except in the dungeon yird. This establishes a collision-ready spawn/loading requirement.

## Entities, player and simulation state

`Assets/Scripts/PeEntity/PeEntity.cs:7`: entity = stable integer ID + prefab path + entity prototype + scenario ID + attached components + message listeners. Export currently identifies version 2 and serializes components. `PeEntity.SendMsg` dispatches typed `EMsg` plus arguments. `PeCmpt` supplies lifecycle and serialization. A C rewrite should preserve domain identities and component concepts without reproducing GameObject/MonoBehaviour.

Components under `Assets/Scripts/PeEntity/Cmpt/` cover transform, player/NPC/monster identity, attributes and skills, movement, equipment, package, replicator, targets, AI, requests, servants/leader, view/LOD, networking, abnormal status, colony/tower/vehicle functionality. Source contains parallel older/newer implementations (`Items/Equipment` and `Items/PEEquipment`, `AiScripts` and newer behavior-tree actions). Port active call paths, not every similarly named class indiscriminately.

`Assets/Scripts/PeEntity/Cmpt/PESkEntity.cs:8` defines the attribute index contract: HP/max, stamina/max/reduction/recovery/delay, comfort/max/reduction/recovery, oxygen/max/reduction/recovery, hunger/max/reduction/recovery, energy/max, resource damage/range/bonus, attack/range/critical/defense, rigidity/max/recovery, shields, movement speeds/jump height, hitfly, hygiene, nutrition classes, ten debuff probability/reduction pairs, knockback thresholds, XP, cutting damage/bonus, weapon multipliers, force scale, XP max, visual radius/angle/listening radius, faction/player IDs, deletion timer, injury state, damage ID and HP recovery. The enum explicitly must remain below 128. `AttribPair` around 120 clamps current values into [0,max] and clamps current when max drops.

Replication requirement: preserve stable stat IDs for imported data, but expose typed C accessors. Separate deterministic simulation updates from rendering/animation. Exact walk/sprint/jump acceleration, mass and camera tuning must be extracted from movement code and prefab data; values invented for the initial C slice must be documented as provisional.

## Items, inventory, equipment

`Assets/Scripts/Items/ItemData.cs:183` defines `ItemProto`; fields around 549 include ID, localized description, icon/resource paths, label/level/sort/tab/class, currency values, stack limit, placement, equipment slots/sex/type, durability/energy, recipe unlock IDs, bundle definition, editor/category, tower ID, buff/skill IDs, properties, repair/strengthen material costs and weapon info. `Mgr` loads `item_editor_type`, `WeaponInfo`, and `PrototypeItem` around 713–744. Prototype ID and instance ID are distinct: `Assets/Scripts/Items/ItemObj.cs:9` defines component-bearing `ItemObject` and `ItemManager.cs` manages instances. A formula book is an item component (`ItemObj.cs:675`).

`Assets/Scripts/Items/ItemPak/ItemPackage.cs:383` has tabs Item=0, Equipment=1, Resource=2, Armor=3. Tab routing comes from prototype `tabIndex`. `SlotList.cs` owns stack/capacity mechanics, splitting/merging, count-by-prototype, and sorting. `ItemPackage.CodeIndex` around 746 packs tab in the top byte and slot in lower bits. Change events distinguish Put, Reset, Clear, Update. `NewFlagMgr` at file start marks a newly acquired item for 300 seconds. `PlayerPackageCmpt.cs:83` creates 27 shortcut entries.

Replication acceptance: adding an item fills compatible stacks then vacant slots without exceeding per-prototype limits; insufficient capacity must fail transactionally; splitting preserves total quantity; non-stackable instances keep durability/energy/components; equipment changes recalculate stats and obey slot/skill/sex rules; hotbar stores references without duplicating stock. Explicit C design recommendation: transactional inventory mutation is preferable to the original partial failure paths.

`ItemData.Bundle.GetItems` samples cumulative probability bins plus fixed contents. Its loop uses `i <= getNumber` (one more iteration than the sampled number), so faithfully importing loot needs a deliberate compatibility choice, not an unexamined port.

## Crafting/replicator exact rules

Source: `Assets/Scripts/PeEntity/Cmpt/ReplicatorCmpt.cs`.

- Formula (`:71`): integer ID, product item ID, output count (short), required seconds, material list, workspace ID.
- Loader (`:101`): table `synthesis_skill`; columns `ID`, `object_ID`, `num_each_product`, `need_time`, up to six consecutive `materialN`/count pairs, `workspace`. Stop reading materials at first nonpositive count.
- Known recipe state = ID plus UI flag. Learning dispatches an event; multiple recipes may produce the same product.
- `MaxProductCount` (`:274`): starts at 9999 and takes minimum integer owned/required across materials; result is batches, not output item count.
- `Run` (`:321`): immediate craft validates material maximum, removes each material, creates output. It does not roll back removal if creation fails. Do not reproduce item-loss bugs inadvertently.
- `StartReplicate` (`:450`): rejects a second active job; stores formula, requested batches, batches left, completed batches awaiting delivery, running time, last delivery time.
- `UpdateReplicate` (`:484`): add delta time; when running time >= recipe seconds * `needTimeScale`, require capacity for all undelivered output plus one batch. Insufficient space resets elapsed and shows a warning. Recheck materials. Single-player consumes one batch on completion, increments completed, decrements left. Deliver accumulated output when all batches finish or more than five seconds have passed since delivery. Multiplayer sends merge request instead of mutating inventory locally.
- Cancellation (`:461`) delivers completed output and discards unfinished work; source checks capacity against `finishCount+1`, a potentially overly strict edge case.
- Source subtracts unscaled `timeNeed` even though threshold uses scaled time; this is a suspected skill-speed bug and requires a compatibility decision.
- Serialize (`:533`) saves version marker -1, known IDs/flags and running formula/request/left/completed/time. Restoring incomplete jobs is part of expected behavior.
- `ReplicatorCmpt.OnUpdate` applies crafting time reduction only when `RandomMapConfig.useSkillTree` is on.

Recommended C baseline contract: one job, recipe definitions separated from state, validate positive counts/known recipe/materials/capacity, atomic batch consumption and output, fixed-step job time, save progress. Immediate output per batch is a clearly disclosed first-slice simplification versus legacy five-second delivery batching.

## Gathering, digging and building

`Assets/Scripts/NaturalRes/NaturalRes.cs:17`: resource definitions load table `resource`, including level IDs, illustration/type, durability, production distributions, ordinary/rare/special extras, fixed/self yield, ground effect, walk sounds and gather skill. Actual rewards are data-driven; do not invent names, item IDs or yields and imply they match the original.

`Assets/Scripts/PeEntity/Cmpt/Motion/PEAction/Action_DigTerrain.cs:29`: maximum dig distance default 3 m. A valid tool and active indicator are required; durability must exceed epsilon; action masks and animation govern execution. `DigTerrain` around 157 starts the tool's skill on `VFVoxelTerrain.self`. Stamina cost around 180 is `tool.m_StaminaCost * entity.StaminaReducePercent`, subtracted from current stamina. `DigTerrain` animation events trigger repeated digging, so rates also depend on animation/skill data. `Action_Fell`, `Action_Gather`, `Action_PumpWater`, `Action_DrawWater` are distinct actions; they should not be collapsed permanently into a universal instant interact operation.

Construction includes voxel and block systems, placed equipment, buildings and user creations. `Items/DrawItem/ItemDragging` has placement subclasses for colony, plant, tower, railway, train, flags, warehouses, creation and connections. Build skill gates distinguish shapes, block levels, bevel, ISO and voxel. Placement must eventually include resource cost, collision, range, orientation, ownership and category-specific validity. The root terrain/creation audit should supply geometry details.

## Combat and skills

`Assets/Scripts/SkillSystem/SkInst.cs:41` `SkData`: interruptibility; individual and shared cooldowns; prepare pre/post times/effect; loop condition; arrays of main pre/duration/post times; one-shot and per-iteration effects; trigger-event lists per main stage; ending pre/post/effect. Loaded from `skMain` after effects, buffs and trigger events. `SkInst` controls runtime and cooldown checks (around 451 onward). Replicate as an explicit skill state machine and data tables; do not put every weapon in unrelated hard-coded logic.

`SkAttribs.cs` stores raw and summed attributes, dirty flags and buff package. Buff modifiers support multiplication, pre-add and post-add; changes emit events. `SkExpCompiler.cs`/`SkExpEvaluator.cs` show a runtime expression system. A C interpreter should parse an explicitly allowed expression grammar rather than execute arbitrary imported code.

Verified damage fragment: `Assets/Scripts/SkillSystem/SkExpFuncSet.cs:48` `TryGetHurt` scales damage by hit/collider and projectile multipliers; calls `CanDamage`; projectiles hit shield first at 10 HP per shield unit, with overflow into HP. Other damage reduces HP directly in this path. Hurt timestamps/events update, and optional XP accrues to actual projectile owner. `CanDamage` (`:128`) requires reputation permission, force conflict, and nonzero `DamageData` matrix entry. `GetHurt` is a separate unconditional path. Attack-vs-defense equations reside in skill expressions/data, so no generic `atk-def` formula is verified here.

Weapons have distinct hold/aim/reload/fire/melee and energy/durability mechanics (`Items/PEEquipment` and `PeEntity/Cmpt/Motion/PEAction`). Melee collision timing and projectile spawning must follow skill/animation events for fidelity. Minimum acceptance: blocked faction damage stays blocked, death event emits once, shield overflow is correct, cooldown cannot be bypassed, ammo is consumed once, and saving/loading cannot duplicate projectiles/rewards.

## AI, followers and NPC work

`PeEntity/Cmpt/BehaveCmpt.cs` links behavior asset paths to `BTLauncher`; death resets/pauses AI and revive resumes. Patrol mode supports no patrol, spawn center or current center plus min/max radius. AI refuses to stop when explicit requests exist or NPC is follower/base member. `TargetCmpt` receives the tree's attack actions.

`Assets/Scripts/BehaviorTree/Scripts/BTAgent.cs` instantiates library/tree by reflection, registers action Start/Init/Tick/Reset callbacks, and schedules using `1 / tree.Frequency`. The behavior source topology plus its serialized assets are required; implementing only these action classes does not reconstruct trees. `Assets/Scripts/Behave/Action/` includes idle/normal/attack/target/threaten/native/group, NPC follower/base/campsite/combat/request, tower and robot actions. `PeEntity/Cmpt/RequestCmpt.cs`, `ServantCmpt.cs`, `ServantLeaderCmpt.cs`, `NpcCmpt.cs` provide higher-level orders and membership.

Recommended portable abstraction: stable entity handles, perception/target memory, navigator interface, finite action states, behavior-tree evaluator, queued requests and per-tree tick rates. Start with idle/wander/chase/attack/dead only as an explicit prototype subset.

## Missions, story and progression

`Assets/Scripts/Mission/Mission.cs:8` mission types Talk/Main/Sub/Time/Mul/Var/Unknown; targets Unknown/KillMonster/Collect/Follow/Discovery/UseItem/Messenger/TowerDif. `MissionCommonData` (`:123`) carries giver/reply NPC, script, repeat limit, descriptions, target IDs, prerequisite/after/mutex limits, reputation limits, linked missions, accept demands/removals/grants, fixed/choice/multiplayer rewards, completion removals, opening/in-progress/ending dialogue and triggered missions, abandonment/reset/deletion rules, timed story transitions, colony NPC/missions, auto reply, skill points and reputation effects, dungeon creation.

Table map in same file: Quest_List (:958), Quest_KillMonster (:1467), Quest_LootItem (:1527), Quest_HuSong (:1591), Quest_EnterArea (:1802), Quest_UseItem (:1866), Quest_Delivery (:1942), Quest_TowerDefence (:1972), Quest_TD_Info (:2075). Story actions additionally load Plot/AdvPlot/Initialization/Camera (`Mission/StoryRepository.cs:401`, :1373, :1527, :1954). Story requires scripted world changes and conversations, not just an objective checklist.

`Mission/PlayerMission.cs:117` stores mission/target states, string-key/value objective variables, timers, follower paths and positions. `IsGetTakeMission` (:679) rejects invalid, active or completed missions; prerequisite type 2 requires all IDs, other prerequisite types require any; IDs 1000–8999 are interpreted as targets in this gate. After/mutex and reputation further restrict availability. Objective processing entrypoints include `ProcessUseItemByID` (:458), `ProcessMonsterDead` (:2356), `ProcessCollectMissionByID` (:4299). `CompleteMission` (:1956) applies rewards/progression and story; `Export` (:4424) persists state.

Recommended C design: data-driven objective discriminated union and event bus; pure availability/completion predicates; atomic accept/turn-in inventory transactions; explicit not-started/active/ready/completed/failed/abandoned; separate dialogue/story interpreter; persistent completion history/counts. Preserve scripted special cases as identified content scripts rather than burying them in generic engine code. Test duplicate event delivery, simultaneous requirements, abort/reset and already-granted reward protection.

Skill tree: `Assets/Scripts/SkillTree/SkillTreeUnitMgr.cs:22` has locked/unlocked/learnt states; node ID/main type/skill type/group/grade/level/XP/parent/value/description/max level. Tables `skilltree` (:1181), `skilltreemt` (:1398). Gates include equipment/drive, rare resources, colony, product type/level, crafting time, building shapes/material level/bevel/ISO/voxel/custom creations. Both the legacy constant `ENABLESKTSYSTEM=false` and runtime `RandomMapConfig.useSkillTree=false` default exist; runtime setting is actually consulted by equipment, craft, build and drive and can be enabled in lobby/map settings. Do not conclude the tree is globally dead based on the constant. Adventure mission completion can award skill points (`PlayerMission.cs:1986`).

Reputation: `Assets/Scripts/ReputationSystem/ReputationSystem.cs` declares Puja/Paja targets, nine ordered levels Fear through Reverence, persistent camp values/override values and belligerency. Belligerency defaults to reputation below Neutral. This feeds quests and damage permission; it is not cosmetic.

## Colony, farming and production

Primary simulation is under `Assets/Scripts/WuYiqiu/Colony System/Scripts/`; similarly named `Assets/Scripts/Colony/` includes multiplayer wrappers. `CSConst.cs:9` enumerates assembly, storage, engineering, enhance/repair/recycle, farm/factory/processing/trade/train, medical check/treat/tent, dwellings, coal/solar/fusion power. Personnel occupations include dweller, worker, soldier, farmer, follower, processor, doctor, trainer; work policies include normal/when-needed/workaholic, farming specialisms, patrol/guard/standby/assigned. State distinguishes preparing, idle/rest/work/follow/dead/attack/patrol/plant/water/weed/harvest.

`Entities/CSAssembly.cs:163`: level determines radius, upgrade duration/materials, damage cooldown/value and per-building type limits. `SetLevel` (:232) rebuilds those limits. `InRange` (:330) uses distance <= radius. `AttachCommonEntity` (:419) checks membership radius/count and attaches electrical consumers to plants. Assembly upgrades are timers; completion increases level and emits upgrade events. `CSInfo.cs:235` loads `colonyinfo`. Additional tables include `collectfield`, `tradePost`, `trademanager`, `AbnormalDruggery`, `abnormaltypetreat`.

`Assets/Scripts/Items/FarmManager.cs:11` plant records carry seed ID, three growth stages/times/live/dead models, default water/cleanliness, depletion rates, thresholds, footprint/height and probabilistic maximum yield. `LoadData` (:60) reads `plant` and explicitly skips its first explanatory row. Persistent plant instances are keyed by item object ID and spatial helper coordinates. Full replication requires growth/death, water/weeding/harvest and colony farmer scheduling with save/offline time semantics.

Acceptance: placement radius/type limits, power loss, missing inputs, worker availability and queue capacity must block production appropriately; cancelling/upgrading/saving must conserve resources; plant growth must be time-based and repeatable, not renderer-frame dependent.

## Suggested implementation sequence and honest first slice

1. Import prototype IDs, stack limits, recipes and resource definitions into engine-neutral data; validate references and report missing/unsupported rows. Keep generated content derived from original data distinguishable from authored prototype content.
2. C deterministic fixed-step simulation with player transform, collidable editable terrain, input actions, inventory, gathering, one crafting job, basic construction and versioned save/load.
3. Attribute/skill runtime, creatures, basic perception/combat, equipment and death/loot; replace provisional movement/combat tuning with evidence-backed data.
4. Missions/dialogue/story and NPC followers; then colony, power/production/farming, transport/creations, advanced modes and networking.

A first slice may use procedural placeholder terrain and simple geometry and newly authored recipes/enemies. If it does, identify it as a C foundation demonstrating the loop, not a faithful reconstruction of the original planet, progression/content, physics or visuals. Original gathering/crafting values must only be claimed where imported/verified. A custom new save format is appropriate initially but is not legacy save compatibility.

Suggested first-slice tests: inventory conservation and overflow; insufficient craft input/capacity is atomic; elapsed craft batches independent of frame partitioning; blocked movement/collision and grounded jump; build costs consumed only on valid placement; terrain changes/inventory/player/craft round-trip; corrupt/truncated save does not modify live state; deterministic scripted replay under fixed seed.
