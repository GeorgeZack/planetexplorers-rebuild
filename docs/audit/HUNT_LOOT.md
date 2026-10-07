# Adventure hare loot

Implemented in C from source; no new original-game session. This increment covers only actual deaths in the bounded six-hare Adventure encounter, not general creature spawning or corpse interaction.

## Source evidence

`PeEntityCreator` binds PrototypeMonster13's Loot11 to `CommonCmpt.ItemDropId`. `PESkEntity.DispatchDeathEvent` calls `LootItemMgr.RequestCreateLootItem`; the older `ItemDropPeEntity` attachment is commented out. `ItemDropData.GetDropItems` supplies the ordinary roll. [The generated provenance](../../rebuild/generated/pe_hunt_loot_source.json) pins the active C# files, database and loot prefab. `export_hunt_loot.py` guards the call chain, source tables, item components and supported mission exclusions, with optional offline UnityPy prefab verification.

Loot11 always supplies Meat229 in integer range22–34. Five further trials iterate this ordered list, drawing a **new** random value for each comparison until one succeeds:

| Item | Per-comparison threshold |
|---|---:|
| Animal Fat1011 | .10 |
| Animal Skin1008 | .20 |
| Peach Flower Fruit236 | .40 |
| Rabbit Leg Meat1026 | .44 |
| Soft Fur1228 | .45 |
| Poisonous Blood1742 | .55 |
| Nothing0 | 1.0 |

These are independent comparisons, not cumulative thresholds or each item's final probability. Duplicate IDs combine. Every supported item is stackable and lacks a Durability component; raw prototype durability defaults do not establish one. No mission-special loot is registered by the supported hunt.

The manager creates separately identified world items. A living player automatically collects a complete stack at strict squared distance<16 from the scene item's transform, without a use key or line-of-sight condition. Failed package insertion preserves the drop; later drops can still succeed. Successful stackable insertion destroys the world instance and adds ordinary stackable bag quantities. Non-quest drops expire only when saved game time minus creation time exceeds93600 seconds. Drops are archived independently of their monster. Source scene generation uses48m and icon visibility10m.

## Native ownership and transactions

`PeGame.hunt_loot` owns up to64 records. `pe_hunt_loot_spawn` stages the entire death batch, existing saved RNG and shared next-instance counter. `pe_hunt_loot_combat_step` stages contact, health, stamina, wear, actor and melee state, identifies actual positive-to-zero contact deaths, then commits combat and loot together. A rejected batch preserves all of those owners and caller outputs. It does not roll back movement already completed before the contact transaction.

Create loot for every actual death, including extra simultaneous deaths in the batch that earns the fourth quest credit. Disappearance when the remaining cohort is cleaned up is not death. Never infer drops from saved kill totals, and never deduplicate by transient hare entity IDs, which can repeat after reload. With three prior credited deaths plus a six-death finishing batch, the bounded mission permits at most nine actual deaths and at most54 merged stacks; capacity64 accommodates that bound. General repeatable encounters would require revisiting this policy.

World records own a unique ID below `inventory.next_instance_id`, never aliased by bag/equipment/outfit/quickslots/deployment/repair/buyback. IDs remain spent after pickup/expiry. Existing stackable bag ownership stays canonical instance0. A malformed record or owner causes failure before mutation. Pickup stages inventory and compacted drops together; each full-stack add independently succeeds or remains pending.

The Adventure host updates pickup/expiry once per focused, unpaused display after fixed simulation. Gameplay modals do not block collection or expiry; death blocks collection only. This code does not consume movement, camera or action input. Quest return cleanup, main-menu transitions and saves retain outstanding drops. Direct world replacement rejects outstanding loot; a new game has none.

## PECSv22

Retain the literal v21 payload and append2052 bytes: u32 count plus64 fixed32-byte records. Each record contains instance/item/count (u32), native crop-local XYZ (f32) and creation game seconds (f64). Unused records must contain canonical positive zero. Validate supported item/stack bounds, finite in-crop positions, nonfuture creation times, unique unaliased identities and Adventure route ownership. A malformed/truncated/extended file fails without replacing the live game.

Pre-v22 saves initialize empty loot without rerolling old kills or changing their saved RNG/identity counter. Clone/move preserve the embedded state using the existing owning-world APIs. Transient display, collision and animation state is never serialized.

## Explicit adapters and remaining parity

The native saved xorshift32 sequence substitutes for `System.Random`; it preserves decision ordering and saved results, not the original random sequence. Meat uses modulo mapping and comparisons use24 random fraction bits. Pickup tests the saved death root, while the original scene item can move during its visual drop animation. All native drops are evaluated each active display; original generation/update batching and periodic expiry checks are not reproduced.

The renderer reads the immutable camera pose and uses existing shared item icons for small clusters at strict distance<10m. Icons are screen offsets above saved death roots; terrain/building/tree occlusion is a native presentation policy and never gates pickup. There is no corpse pose, glow/ballistic scatter/flight, collection sound or effect implementation. Original full visual parity remains unqualified. Shields, reaction animation, pounce and Town Defense9077 remain separate gated work.

## Verification

Run `make -C rebuild hunt-loot-test hunt-loot-source-check` plus the full core suite for historical-save compatibility. The source guard requires the available offline UnityPy interpreter for prefab verification. Runtime/save tests cover deterministic rolls, whole-stack capacity failures, strict radius/lifetime boundaries, dead-player behavior, ownership/canonical-state rejection, rollback, literal v21 migration, reload without reroll, clone/move and world replacement. Contact integration and marker checks are recorded with final results in [PLAYTEST](../PLAYTEST.md); fixtures do not establish another earned hunt or original visual acceptance.

The staged actual marker renderer was inspected at [1280×720](../reference/hunt-loot/loot-1280x720.png) and [640×480](../reference/hunt-loot/loot-640x480.png), using all seven recovered icons in two artificial clusters on a grid. It ran on an owned private high display and touched no gameplay saves or desktop input. These images verify icon availability/layout, not ordinary gameplay or original particle fidelity.
