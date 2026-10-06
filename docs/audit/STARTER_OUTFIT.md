# Starter outfit ownership and defense

Implementation checkpoint: October 6, 2026. This adds the source starter clothes to the supported female Adventure player's gameplay state. It corrects a missing equipment contribution without changing base defense, creature statistics or damage formulas. Source rules, headless verification and physical observations are separate; the preceding [difficulty audit](HUNT_DIFFICULTY.md) explains the omission and remaining combat limitations.

## Source contract

[`SingleGameInitData.AddAdventureInitData`](../../Assets/PeWorld/SingleGameInitData.cs) calls `AddDefaultClothToPlayer`, creates real item instances, filters the six starter IDs by player sex and equips the resulting pieces. The supported native female appearance therefore receives:

| Slot | Item | Defense | Maximum durability | Wear factor | Equipment buff |
| --- | --- | ---: | ---: | ---: | ---: |
| Coat | Issued Overalls95 | 7 | 2156 | 0.1 | 30200097 |
| Pants | Issued Pants131 | 4 | 2156 | 0.1 | 30200098 |
| Shoes | Issued Shoes192 | 1 | 862 | 0.1 | 30200100 |

The source player's base defense remains0. Intact clothing adds12 through equipment buffs. Male113/149/210 have matching values, but the native bounded owner does not implement gender selection or alternate starter appearances. Comfort modifiers9/10 in these buffs are both zero. Generic item HP/HPMax100 properties are not added to player health.

[`EquipmentCmpt`](../../Assets/Scripts/PeEntity/Cmpt/EquipmentCmpt.cs) applies instance buffs and spends defense durability on `Battle_BeAttacked`. [`ItemObj.EquipDefence`](../../Assets/Scripts/Items/ItemObj.cs) subtracts `max(0, damage × durabilityFactor)` from every equipped defense item, clamps at zero, and removes a broken item's buff. Source Build mode excludes wear. [`SkExpFuncSet`](../../Assets/Scripts/SkillSystem/SkExpFuncSet.cs) sends calculated damage after capsule scaling through the hurt notification, before remaining-HP clamping. Consequently a fatal hit still spends durability using its calculated damage, not merely the final sliver of HP removed. Healing does not repair equipment.

[`export_starter_outfit.py`](../../rebuild/tools/export_starter_outfit.py) generates [runtime constants](../../rebuild/generated/pe_starter_outfit_source.h) and a [provenance manifest](../../rebuild/generated/pe_starter_outfit_source.json). It guards the source creation/sex filter, equipment components, property/buff values, initial durability and wear path. The manifest retains male bindings as reference evidence without implying a native male outfit feature. The catalog now contains320 items, including the three clothing definitions and their original icons.

## Native ownership

[`PeOutfit`](../../rebuild/src/pe_outfit.h) owns three fixed `PeWornItem` records, each containing item ID, unique instance ID and current durability. `PeGame.outfit` is separate from inventory slots, the held-tool identity and deployed objects. Fresh Adventure initialization allocates these identities after the existing starter inventory, retaining the starter Knife's instance1. Legacy initialization keeps all three records zero.

`pe_outfit_init` accepts a fresh zero outfit only. It rejects invalid or exhausted allocation counters, any existing reference at or above the counter, and overflow before committing the three records and updated counter. It never replaces or repairs an existing outfit. `pe_outfit_state_valid` requires the exact95/131/192 order in Adventure, finite durability within each source maximum, distinct nonzero identities below the shared counter, and no aliases with inventory, held equipment, quickslots or deployments. Exhausted pieces remain owned instances. Legacy permits only the zero outfit.

For standalone combat fixtures, `pe_outfit_valid` also accepts a completely empty outfit. `pe_outfit_defense` sums only positive-durability pieces and returns an invalid result for malformed state. `pe_outfit_wear` stages all three records and commits only valid finite input; negative finite damage does nothing and cannot repair clothing. General purchased armor, replacement/unequip transactions, repairs, shield blocking and broader equipment buffs remain unsupported.

## Combat and presentation

[`pe_hunt_combat_step`](../../rebuild/src/pe_hunt_combat.c) stages outfit wear alongside creature state, melee clocks, HP, stamina and kill credit. Each accepted incoming capsule contact uses base defense plus the currently staged outfit contribution. The resolver's calculated damage then spends wear immediately, so a piece breaking on one hit changes protection for the next hit, including a later contact in the same update. An invalid operation leaves the original outfit and other committed state untouched. A null outfit pointer preserves existing standalone base-defense behavior.

The sandbox passes the actual `PeGame.outfit` only after ownership validation; it continues using the same rendered body yaw, source clip or locomotion blend for player defense capsules. Clothing does not replace the capsule pose or add an independent action clock. Native integration is limited to the Adventure hunt controller. Source Single Player Build is not implemented by this owner; Adventure's B-key construction is not that source mode and does not create a wear exemption.

The Character panel shows the three existing clothing icons in coat/pants/shoes slots, actual effective defense, and instance durability tooltips. Exhausted pieces are marked broken and contribute zero defense. These are the clothes already visible on the recovered player model; no new body meshes or clothing-swap presentation were installed. The fixed worn slots do not advertise unsupported dragging, unequipping or repair.

## PECSv14 contract

V14 retains the literal v13 field layout and appends36 bytes: three fixed12-byte records in coat/pants/shoes order, each encoded as little-endian32-bit item ID,32-bit instance ID and IEEE32-bit durability. No structure padding or transient pointer is serialized. Legacy writes36 zero bytes. The existing header length/checksum cover the full payload.

A v14 load validates the saved outfit exactly and never synthesizes missing pieces or repairs wear. Missing/partial records, invalid IDs, aliases, out-of-range or nonfinite durability, truncated payloads and trailing data fail atomically, preserving the live game and owned world. Invalid save attempts likewise leave an existing save file intact.

Versions1–13 first decode and validate their complete existing state. Only then does Adventure allocate an intact starter outfit after the validated shared identity counter. The counter increases by three; bag contents and their identities, equipped tool, quickslots, deployments, world and progression are retained. This is an explicit compatibility adapter: older native saves displayed clothes but recorded no historical wear, so that wear cannot be recovered. Loading the same older checkpoint gives deterministic identities; a subsequent v14 save preserves the migrated identities and all later wear without adding or repairing pieces. Older Legacy saves remain outfit-free and do not advance their counter.

## Verification at this checkpoint

- Source export regeneration and all10 exporter tests pass.
- The focused outfit core suite passes strict optimized C11 and ASan/UBSan with leak detection, without skips. It covers exact stats, sequential piece breakage, calculated overkill wear, negative/invalid damage, late-record validation failure, identity alias/counter/overflow rejection, source starter Knife identity, Legacy state, cloning and healing independence. Logs: `rebuild/build/outfit-optimized.log` and `outfit-sanitize.log`.
- Focused persistence and historical-save suites pass optimized; outfit/v13 persistence also passes ASan/UBSan with leak detection, with no asset skips. They exercise heap-owned worlds, literal retained-prefix records, worn/broken round trips, malformed-data atomicity, existing-file preservation, old-save prevalidation, deterministic migration and preserved tool/deployment identities. The bounded local result is `rebuild/build/running-next/outfit-save-result.json`.
- The integrated real-contact regression compares the same posed hare hit against bare, intact and first-hit-breaking outfits, checks the source formula reduction and calculated overkill wear, and rejects an invalid frame without committing wear. It passes in the final optimized and full ASan/UBSan core runs.
- Independent code review found the per-hit defense update, staged commit, null-outfit path, body-pose mapping and displayed defense consistent with the ownership contract.

The optimized frontend build and full `make test` pass with exit0. Its21 optional UnityPy-dependent skips remain skips, not passes. Full `make sanitize` passes ASan/UBSan with leak detection enabled; content regeneration checks and all23 CLI cases also pass. Final local logs are `rebuild/build/outfit-next/final-build.log`, `test.log`, `sanitize.log` and `content-cli.log`. The earlier `core.log` contains an intermediate failed run and is superseded by the final `test.log` result.

## Native physical check

Native PID2142398 loaded the earned v13 checkpoint through the ordinary migration path. The [Character panel](../reference/starter-outfit-character.png) displayed the three clothing icons and Defense12. Ordinary travel reached the real hare encounter; eight incoming contacts reduced HP500→215.395752. No new player attacks or consumables were used before this checkpoint, and the kill count remained0.

The saved v14 outfit retained identities31/32/33, with durability2127.539794921875 for overalls and pants, and833.5396118164062 for shoes. Pause→Load→Save produced byte-identical checkpoints; the reloaded Character panel showed HP215 and Defense12. The [bounded physical record](../reference/starter-outfit-physical.json) contains inspection evidence and hashes; [PLAYTEST](../PLAYTEST.md#october-6--starter-outfit-defense-and-wear) records the sequence. The authoritative worn values come from the confirmed immutable save snapshots, not the earlier live-file inspection that changed during reading.

A separate follow-on stationary-Knife attempt started from the reloaded worn checkpoint at approximatelyHP215, used one Herbal Juice and no Med Pack, and ended in death after six outgoing/five incoming contacts with0kills. An additional empty-slot4 press consumed nothing. The operator restored the prior checkpoint, returned to the main menu and quit with exit0; the confirmed worn save remained untouched. This is not a full-HP or controlled survival comparison.

This verifies native outfit migration/presentation, real incoming-hit wear, and save/reload without repairing or duplicating clothing. It does not establish physical breakage, exact per-hit reduction against a matched original trace, four-kill survival or overall difficulty parity. The earlier [consumable motion correction](SANDBOX_CONSUMABLES.md) has its own headless and physical evidence and is not additional outfit coverage.
