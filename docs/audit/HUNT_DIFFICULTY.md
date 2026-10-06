# Hunt difficulty and missing equipment parity

Audit: October 6, 2026. This is a source/code audit following a native manual run reported as 23 outgoing contacts, seven incoming contacts, no kills, and player HP500→0 across seven seconds of the native combat clock (which excludes modal suspension). It is not a matched original/native difficulty experiment. No damage, enemy HP, attack rate or equipment values were changed for this audit.

## Intended order and available equipment

The source `AdRandomQuest_List` chain is welcome→Arrival9031→supplies9032→medicine offer9071→medicine9072→followup9074→hunt acceptance9075→four kills9126→return9127→Town Defense9077. The explicit prerequisite for9074 is completed9072; there is no additional mandatory weapon-upgrade quest between medicine and the hunt. Medicine supplies ten Med Packs925 before the hunt. Other activity can improve equipment, but skipping an intervening required quest does not explain this native result.

Source supplies9032 already grant **Wood Sword1, Wood Shield34 and Wood Bow43**, among the tools/scripts. The native inventory contains these source rewards, but current combat only implements Knife33. Possession is not functional equipment parity.

| Item | Source combat properties | Availability before the hunt |
| --- | --- | --- |
| Knife33 | Attack9; durability100; attack wear0 | Bounded native supported weapon |
| Wood Sword1 | Attack10; durability2369; attack wear5 | Supplies9032; archived vendor20002/store1183 also sells it for158 |
| Wood Shield34 | Defence9, melee protection0.22; durability4312; defence wear0.1 | Supplies9032; vendor20014/store1308 sells it for189 |
| Iron Sword4 | Attack51; durability4578; attack wear5 | Archived vendor20033/store232, price1034 |
| Iron Shield37 | Defence26, melee protection0.47 | Archived vendor20037/store236, price1292 |

Prices are the actual exported source store offers, not `PrototypeItem.currency_value`. See [shop transactions](ADVENTURE_TOWN_INTERACTIONS.md) and [purchased equipment](SANDBOX_PURCHASED_ITEMS.md). The current quest checkpoint's money does not automatically cover advanced equipment; native selling/buyback and general armor equipment are not implemented.

A Wood Sword changes base10+Knife9 to base10+Sword10, a small attack-stat gain. It must still receive its own validated weapon/action assets and wear semantics. A shield is a separate equipment/hold/block system: source buff30200093 adds item defence times `ShieldMultiple` and the item's `ShieldMeleeProtect`; [`Action_SheildHold`](../../Assets/Scripts/PeEntity/Cmpt/Motion/PEAction/Action_Sheild.cs) toggles `EnableShieldBlock` and buff30200177. The protection number is **not** evidence for applying a blanket percentage reduction to every native hit. Shield pose, collision/eligibility, action interaction and modifier semantics need their own implementation audit.

## The confirmed defence omission

Source `initProp` player1 has base Defence0. That generated native base is correct. [`SingleGameInitData.AddAdventureInitData`](../../Assets/PeWorld/SingleGameInitData.cs) calls `AddDefaultClothToPlayer`: it creates and equips real instances from `{113,149,210,95,131,192}`, filtered by sex. The supported female appearance therefore starts with:

| Item | Defence | Maximum durability | Wear factor | Equipment buff |
| --- | --- | --- | --- | --- |
| Issued overalls95 | 7 | 2156 | 0.1 | 30200097 |
| Issued pants131 | 4 | 2156 | 0.1 | 30200098 |
| Issued shoes192 | 1 | 862 | 0.1 | 30200100 |

Male113/149/210 have matching defence/durability values. Buffs add the item Def property, so intact starter clothing gives **12 total defence**, without modifying base0. The source Character panel observation and both [captured hunt cohort](../reference/hunt-encounter-cohort.json) and [reload observations](../reference/hunt-encounter-reloads.json) record player defence12.

The pre-fix native actor already displayed these clothing parts, but `pe_sandbox_view.c:hunt_player_pose` passed only `pe_player_defaults.defense` (0). That build had no authoritative gameplay owner for these worn items. This is a concrete omission, not a reason to permanently hardcode base defence12.

The existing damage formula `attack²/(attack+defence)` is already source-derived. Against the captured hare attack range52.592968–82.06734, defence12 reduces pre-capsule damage by approximately18.6%–12.8%. It would help, but this calculation does not establish that clothing alone fixes survival, target acquisition or the reported seven-second outcome. Outgoing contact totals can be spread across several creatures;23 contacts do not imply23 hits against one hare. Captured HP and defence also vary substantially by creature.

## Durability and a bounded faithful implementation

[`EquipmentCmpt`](../../Assets/Scripts/PeEntity/Cmpt/EquipmentCmpt.cs) applies each equipped instance's buff and, on `Battle_BeAttacked`, calls `ExpendDefenceDurability` for every equipped item. [`ItemObj.EquipDefence`](../../Assets/Scripts/Items/ItemObj.cs) subtracts `max(0, damage × durabilityFactor)`; at zero durability it removes that item's buff. `Equip.AddBuff` refuses an already exhausted item. Build mode bypasses equipment wear.

[`SkExpFuncSet.TryGetHurt/GetHurt`](../../Assets/Scripts/SkillSystem/SkExpFuncSet.cs) applies the collision damage scale and sends that calculated damage through `OnGetHurt`→[`SkAliveEntity`](../../Assets/Scripts/SkillSystem/SkAliveEntity.cs)→`Battle_BeAttacked`. The notification is not clamped to the target's remaining HP. A native wear adapter must consequently use resolver damage, not merely `min(damage, remaining HP)`. It must update wear after each accepted hit so a piece breaking on one hit affects later hits. Healing does not repair equipment.

The smallest supported next scope is a bounded starter-outfit equipment owner, with three exact item IDs, unique persistent instance identities, current durability and source-generated metadata. Compute effective defence as base plus buffs from nonexhausted equipped instances. Keep these records separate from the single held-tool identity and reject ownership aliases with bag/equipped/deployed items. Fresh Adventure creation equips the source sex-matched outfit once; source visuals already depict that outfit. A save version must preserve wear and identity, with an explicit migration policy for older native saves whose previous clothing wear cannot be reconstructed. Initializing those old visual outfits intact is a documented compatibility adapter, not recovered historical state.

This bounded step should not silently enable purchased armor replacement or shield blocking. Those need presentation, slot replacement and transaction support. Tests should cover exact initial7+4+1, zero-durability removal, sequential-hit breakage, source damage versus overkill wear, healing independence, Build exclusion, unique identities, atomic invalid load, old-save migration and save/reload without repairing or duplicating equipment. Headless combat should demonstrate the same attack/capsule doing the formula-derived lower damage with intact clothing and reverting as pieces break; original/manual checks remain separate.

## Remaining limits

Current native knife attacks, running root motion and posed blade/defence contacts are tested separately in [running Knife parity](SANDBOX_RUNNING_KNIFE.md) and [hunt combat](ADVENTURE_HUNT_COMBAT.md). The native hunt still has bounded arbitration and no complete source pounce, hurt/stagger, target-selection or vertical sword IK behavior. Those can affect survival and whether hits concentrate on a target. The nonmodal healing-input correction is being verified separately: consuming a Med Pack should not stow the knife or suspend held movement and enemy combat. No blanket balance adjustment is justified by this audit.

Primary numeric input: `Assets/Resources/DataBase/localData.bytes`, SHA-256 `c3d63ee865a7e10e309e11e9ee561c93feef39c379a584dcf68722a523cd4376`; tables `initProp`, `PrototypeItem`, `skBuff`, `AdRandomQuest_List`, plus exported source shop offers. This document distinguishes source-backed mechanics from proposed native work; the outfit owner and general sword/shield support are not implemented by this audit.


## Subsequent implementation

The [starter outfit increment](STARTER_OUTFIT.md) implements the bounded owner above, source-generated stats, transactional per-hit wear and PECSv14 migration. The character window now reads the same worn records and effective defense used by combat. General armor swaps, repair, sword/shield use and matched original/native difficulty testing remain outside this increment; restoring clothing does not establish that the hunt is now balanced or physically completed.
