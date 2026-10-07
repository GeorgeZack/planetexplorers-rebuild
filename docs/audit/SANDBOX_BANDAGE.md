# Restoration Bandage924 and shared medicine cooldown

Restoration Bandage924 now uses the existing live-player healing path: one item restores200 HP immediately, up to maximum HP. Bandage924 and Portable Medpack925 share the same ten-second group9 cooldown. This increment is source-derived and checked headlessly; it does not claim a new original-game observation or recovered particle presentation.

## Source contract

The active path is [UseItemCmpt.Request → Use → ConsumeItem](../../Assets/Scripts/PeEntity/Cmpt/UseItemCmpt.cs), [Consume → Property.StartSkSkill](../../Assets/Scripts/Items/ItemObj.cs), then [SkInst.StartSkill/GetSkillCoolingInfo](../../Assets/Scripts/SkillSystem/SkInst.cs). Request rejects dead users and keeps Revival Shot937 out of ordinary use. Property starts the item's skill with the user as caster and target. Use destroys one item only after skill startup succeeds. There is no full-HP exclusion.

`Assets/Resources/DataBase/localData.bytes` provides:

| Item | Component class | Skill | Trigger | HP modifier | Own/shared cooldown | Group |
|---|---|---|---|---|---|---|
|Restoration Bandage924|27, `Property,Consume`|20150027|1|`mad,1,1,200`|10s/10s|9|
|Portable Medpack925|27, `Property,Consume`|20150028|2|`mad,1,1,350`|10s/10s|9|

Both triggers have condition`True`, no caster modification, zero force and zero caster effect. Both skills have condition`False`, zero preparation/main/ending delays and zero phase effects. The modifier adds to HP attribute1; [PESkEntity.AttribPair](../../Assets/Scripts/PeEntity/Cmpt/PESkEntity.cs) clamps it to maximum HP. The item's generic prototype HP property is not its healing amount.

Both target effects reference420: particle139, animation selector`0`, sound/camera/emission/procedural selectors`0`, special effect`False`. These rows require no drinking/bandaging body animation or movement lock. The existing unported healing particle remains a separate presentation limitation.

SkInst's cooldown lookup checks the same skill's own timer or another skill's shared cooldown group. With both supported group9 medicines using the same ten-second duration, one existing saved timer represents their shared availability. Herbal Juice916 uses group11 and remains independent.

[export_content.py](../../rebuild/tools/export_content.py) guards both exact item/class/skill/trigger bindings, healing modifiers, cooldown values/groups, zero delays/effects and the effect420 presentation fields. It emits the two-entry `PeInstantHealDef` table and manifest metadata with the original database hash. Existing medpack constants remain available for compatibility. A source change to any supported contract fails export instead of silently enabling an unsupported effect.

## Existing acquisition

No items, ingredients, recipes, shop offers or starting grants are added. The catalog remains341 items/108 formulas.

Annot's archived vendor Xing Zhang, entity20028/prototype153, already sells:

| Store ID | Item | Price | Stock | Refresh |
|---|---|---|---|---|
|1407|Restoration Bandage924|13|60|46800s|
|341|Restoration Bandage Script878|7|1|46800s|
|1408|Portable Medpack925|23|40|46800s|

Script878 already teaches three one-second workspace0 recipes, each producing924×1:

| Formula | Ingredients |
|---|---|
|146|Tulip235×1, Maria Sunflower239×1, Cycas234×2|
|391|Blue Anemone243×1, Mini Corn240×1, Snapjelly256×2|
|394|Jade Potato250×1, Dragon Pod251×1, Flute Grass246×2|

The ordinary shop, script learning and timed crafting owners remain authoritative. The headless acquisition fixture opens the existing shop directly; it does not establish walking to Annot or bypass the native host's NPC interaction gates.

## Native ownership and persistence

[pe_game_use_slot](../../rebuild/src/pe_game.c) resolves924/925 through generated healing metadata, consumes exactly one and commits HP plus the existing `medpack_cooldown`. Both directions are blocked: a bandage prevents medpack use and a medpack prevents bandage use until the timer expires. Shortcut aliases use that same timer. Full-HP use consumes an item as in the source. Dead users, malformed health, malformed consumable slots and invalid/active timers reject without changing gameplay state.

The historical field name `medpack_cooldown` now represents both supported group9 medicines. Its saved finite0..10 range and existing wire position are unchanged; this feature requires no save version beyond current PECSv20. The existing simulation-time countdown and sub-.1ms residual adapter remain unchanged. Saved mid-cooldown state blocks either medicine after load, regardless of which one started the timer. No new buff/action clock is introduced.

`pe_game_item_is_healing_consumable` identifies only supported live-player916/924/925. UI quickslot dispatch uses this shared classification for action-only ownership, including cooldown and empty-stack retries. Healing must preserve movement, held camera look, equipped weapons and enemy simulation. Inventory or other real modals retain their existing full ownership. The existing numbered conversations and requested medicine-quest final4100 acknowledgment are unaffected.

Herbal Drink917 remains unsupported for use: it needs its distinct healing-over-time buff identity to survive saving. Foods, illness treatments, group12 medicines and out-of-catalog group9 variants926–933 remain outside this change. Revival937 keeps its separate dead-player transaction.

## Verification

[The focused C test](../../rebuild/tests/test_bandage.c) exercises real vendor purchases, script878 learning, formula146 timed crafting and use; both cross-medicine cooldown directions and alias shortcuts; saved/reloaded timers in both directions;599/600 fixed-step boundaries; juice-buff independence; HP caps/full HP/death; malformed health/slots/timers; and empty retries. It uses an ordinary Adventure world and no recovered assets or global settlement configuration.

[Content tests](../../rebuild/tests/test_content.py) verify the unchanged catalog/acquisition closure and mutate component binding, skill, healing, cooldown group/duration, delay, phase effect, trigger list, caster modifier, force and target presentation to require rejection. The host's existing hunt-consumable fixture separately checks action-only input and same-frame movement/contact integration; its results belong to the parent aggregate verification.

Executed October7,2026: the focused bandage C integration test passes optimized and ASan/UBSan builds with leak detection enabled, with no diagnostics or asset skips. Content Python tests pass21/21 with no skips. Both content and town-trade-price exporter checks pass; no trade-price regeneration is needed because the catalog/acquisition set is unchanged. `git diff --check` passes. No physical playtest was required or performed for this source-portable increment.
