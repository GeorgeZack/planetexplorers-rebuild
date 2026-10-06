# Wood Sword successful-contact wear

October 6, 2026. Source-derived behavior and implemented headless transactions; no new original or native desktop observation. See [starter equipment](STARTER_SWORD_SHIELD.md) and [shield ownership](SHIELD_OWNERSHIP.md) for the separate equipment, asset and blocking boundaries.

## Source contract

[SkExpFuncSet.TryGetHurt](../../Assets/Scripts/SkillSystem/SkExpFuncSet.cs) calls the caster's `OnHurtSb` after an admitted damage execution. [SkAliveEntity.OnHurtSb](../../Assets/Scripts/SkillSystem/SkAliveEntity.cs) dispatches `Battle_EquipAttack` for the current weapon. [EquipmentCmpt](../../Assets/Scripts/PeEntity/Cmpt/EquipmentCmpt.cs) invokes that item's attack durability expenditure and explicitly bypasses it in Build. [ItemObj.EquipAttack](../../Assets/Scripts/Items/ItemObj.cs) subtracts the item factor once per notification; reaching zero removes its property buff. Wood Sword1 has source durability2369, expenditure5 and attack bonus10, supplied by generated `pe_starter_sword_shield_source.h`.

A miss or deduplicated contact does not execute that notification. Different admitted targets each do; overkill does not reduce the expenditure. The notification itself does not require positive remaining-HP loss. At zero durability the sword's item attack bonus disappears. `Motion_Equip.WeaponCanUse` explicitly returns true for a `PeSword`; `EquipmentCanUse` delegates its weapon branch there. `PeSword.CanAttack` and `Action_Sword.CanDoAction` do not add a durability gate, nor does queued combo continuation. The separate `CheckEquipmentDurability` helper's inspected callers are NPC code. Thus a depleted held sword retains the source attack action with base attack, rather than inheriting the native Knife's existing positive-durability gate.

## Native transaction

`pe_sword` validates the exact item1/count1/nonzero-instance slot and finite durability in `[0,2369]`. `pe_sword_attack(slot, base_attack)` returns base plus10 while intact and base alone when exhausted. `pe_sword_wear` subtracts5 and clamps at zero. These functions own no inventory, save record or persistent combat state.

The optional `PeHuntCombatPlayer.weapon` points to the authoritative selected bag slot; `base_attack` excludes the sword's bonus. The controller validates exact setup/frame/active-action identity and rejects aliases with shield or worn clothing. The owning frontend remains responsible for resolving a valid inventory selection. The transaction stages this slot with player HP, shield/clothing wear, creature HP, stamina, kill credit, clocks and contact deduplication.

New `pe_sandbox_melee_resolve_with_weapon`, `pe_sandbox_melee_step_with_weapon` and `pe_hunt_encounter_melee_with_weapon` preserve the existing resolver order and clock. They recompute attack from current staged durability before each target, then debit5 on each successful contact. An earlier target can exhaust the item bonus before a later target in the same tick. The encounter stages wear until its death dispatch also succeeds; the outer hunt commits only after incoming damage also succeeds. Any invalid call leaves original state and outputs untouched. The old wrappers and a null optional slot retain the existing setup attack and no-wear behavior, including Knife33. Build bypass remains host-owned; this hunt is an Adventure adapter.

## Focused evidence and limits

The existing melee, hunt encounter and hunt controller suites all passed optimized and strict ASan/UBSan with leak detection enabled. Both builds and runs exited0, with no compiler warnings or asset skips. The final sequence took89.64 seconds. Ignored evidence is `rebuild/build/sword-wear/{optimized.log,sanitize.log,verification.json}`; the JSON records tested source hashes.

The added controller fixture uses the actual modern Wood Sword blade capsule and body/tool poses from `starter-pair-unified-03`, against recovered full-HP hare capsules in deliberately arranged geometry. It verifies multiple contacts per tick, attack20 then base10 after a five-durability sword breaks, already-depleted attack10, no wear on misses or repeated guide contacts, and one expenditure for an overkill kill. A successful control replay proves outgoing wear occurs before a deliberately malformed later incoming guide would reject the whole tick; that failure preserves all original state. Fifteen invalid slot/base/setup/frame/active-instance and equipment-alias cases also reject atomically. Existing Knife, incoming shield/outfit and running-pose tests remain in the focused suites.

This does not activate normal Sword/Shield use, qualify visible shield composition, establish original event ordering between different targets, add Build combat, or prove a physical hunt outcome. The source item values and headless contact transactions are separate from frontend readiness and live parity.
