# Gameplay source map

Source-derived subsystem entry points. Current native behavior belongs to [feature contracts](../FEATURE_CONTRACTS.md), acceptance to [PLAYTEST](../PLAYTEST.md), and next work to [HANDOFF](../HANDOFF.md). This map does not claim original or native parity.

| Area | Source authority and reconstruction boundary |
|---|---|
| Modes/lifecycle | GameConfig, PeGameMgr and single-player loaders; distinguish Story/Adventure/Build/Custom, Tutorial and multiplayer. Adventure construction is not Single Player Build. |
| Entities | PeEntity/PeEntityCreator, components and initprop/PrototypeNPC/PrototypeMonster. Resolve active prefab overrides and archive identities, not template defaults alone. Separate persistent entities from transient actors/actions. |
| Inventory/equipment | PlayerPackageCmpt, PlayerPackage/ItemPackage, ItemObj/EquipmentCmpt and PrototypeItem. Exact object identity, stack rules, components, equipment masks, wear and atomic transfer precede UI exposure. |
| Crafting | ReplicatorCmpt and synthesis_skill. Formula unlock, workspace, output quantity, all materials, job counts/time, buffered delivery, cancel and save semantics form one owner. Source one-completion/display differs from native catch-up/retry adapters. |
| Dig/fell/gather | DigTerrainManager, NaturalRes, SkEntityVoxelTerrain, SkEntitySubTerrain and action components. Terrain/buildblock/blocktype/resource/PrototypeTree are different catalogs. Tool event, damage, wear, yield/RNG and geometry commit must agree. |
| Construction | Building System/PEBuildingMan and Block45. Source per-operation material rounding, valid placement and collision update are separate from terrain edits and the creation editor. [Block contract](BLOCKS.md) |
| Combat | Action_Sword/Action_Beat, PEAttackTrigger/PECapsuleTrigger, SkInst/SkTriggerEvent and skill tables. Animation notifications, guide dedupe, damage, force/reaction and playback have distinct order/owners. Staged assets do not authorize closed branches. |
| AI/followers | Behave trees, scene/entity agents, target/camp permissions and NPC components. Archived NPC appearances do not supply combat, recruitment, schedules or work behavior. Recover actual behavior paths and inputs before adding a host. |
| Missions/story | PlayerMission, AdRandomMission, mission tables, scenario dialogue, role bindings and plots. IDs are graph nodes, not a sequential counter; acceptance/reward/target/plot timing needs active callers. [Disconnected events](ADVENTURE_MISSION_EVENTS.md) |
| Colony/production | Assembly/entity types, power/storage/workspace and NPC staffing components; source csd_* data. Placement alone does not establish functioning colony machinery. Qualify worker/input/power/capacity/lifecycle and offline/save rules together. |
| Farming | plant data and source growth/water/weeding/harvest/farmer logic. Growth is simulation-owned, not renderer-frame based. Catalog seeds are not supported planting. |
| Survival | Initial buffs, SkBuffs/SkExpFuncSet and UseItemCmpt. Preserve strict scheduler boundaries, fresh/load/death ordering and consumable grouping. Hunger/Comfort are implemented; Oxygen/water remain separate recovery work. |

Generated catalogs are selected dependency closures, not proof that every database row is usable. Keep numeric IDs/hashes, source terms and unsupported branches explicit. [Source-first workflow](SOURCE_FIRST_REBUILD.md) defines proof and [extraction](SOURCE_FIRST_EXTRACTION.md) distinguishes static data from engine evaluation.
