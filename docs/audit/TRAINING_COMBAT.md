# Training equipment and player combat audit

This is a source audit and implementation contract, not a claim that interactive combat has been rebuilt. The C training slice now accepts mission745, grants and equips its knife, presents mission746's combat introduction, and stops before attacks or enemy simulation. HP500 and the source Herbal Juice buff/cooldown are implemented. The preceding original-runtime playtest established that the player has ten Herbal Juices after accepting mission 743 and nine after consuming one; the knife/combat findings below come from source, database rows, and a decoded source prefab. Enemy construction and behavior are covered separately in [TRAINING_ENEMY.md](TRAINING_ENEMY.md).

## Mission acceptance and equipment

Authoritative database: `Assets/Resources/DataBase/localData.bytes` (SQLite; IDs are text). Relevant `Quest_List` records:

| Mission | Entry grant | Entry dialogue (`Scenario.talk_id`) | Objective | Completion |
|---|---|---|---|---|
| 745 | Item 33 ×1 | 4074, 300225, 300226; Plot 431 adds 300227, 300228 | 4104, equip a weapon | 746 |
| 746 | None | 300229, 4299; Plot 352 creates the enemy | 1025, `MonsterID=13_1_1` | Dialogue 300230, then 719 |

Mission grants are applied once in `PlayerMission.cs:2877–2905`, guarded by `!HasGetRewards(MissionID)`. They are not rewards for clicking an equipment UI control. The earlier mission 742's `Get_DemandItem=916_1` is a check-only prerequisite (`PlayerMission.cs:846–855`); only `m_Get_DeleteItem` removes items, and that row is zero. Consequently the established 5 → 10 → 9 juice sequence must remain intact when mission 745 is accepted.

`UseItemTask.cs:57–73` subscribes to equipment changes when 745 starts. The callback accepts an **added** equipped item whose `protoData.weaponInfo` is non-null; it is not a knife-ID-specific condition and does not count removal. It unsubscribes after completion. A C implementation that supports only the granted knife can require item 33 while explicitly documenting its narrower equipment support. Earlier equipment events must not pre-complete this lesson.

`TrainingTaskManager.cs:265–270,372–377` connects the equipment listener to mission 745 and marks 746 as the fighting mission. Mission 746's target means prototype 13, count one, target type one; it must be driven by a real qualifying death, not an attack button or a scripted hit counter. The active source listener accepts a matching prototype death without requiring the particular fixed spawn or a player kill; do not add those requirements accidentally. The old `FightingTask` helper is not the live enemy spawn path; see the enemy audit.

## Knife data: separate attributes, metadata, and serialized behavior

`PrototypeItem.id=33`:

| Field | Source value |
|---|---|
| Name | knife |
| Item type / weapon type | 9 / 8 |
| Model | `Prefab/Item/Equip/Weapon/Sword/knife` |
| Stack limit | 1 |
| Attack attribute | 9 |
| Equipment buff | 30200087 |
| Durability maximum / decrement | 100 / 0 |
| Equipment operation | 2 |

The attack durability code subtracts `durabilityFactor` (`ItemObj.cs:1639–1642`), so applying the generic tool decrement of five to this knife would be incorrect. Its row has zero `durDec`.

The source prefab `Assets/Resources/Prefab/Item/Equip/Weapon/Sword/knife.prefab` is a binary serialized Unity asset. It was read with the existing isolated UnityPy installation, without launching Unity. Its `PeSword` component contains:

| Serialized property | Value |
|---|---|
| Stamina cost, attack mode 0 | 5 |
| Animation speed | 1.37 |
| Low stamina thresholds | 50, 20 |
| Stationary skill | 20110011 |
| Moving skill | 20110014 |
| Sprinting skill | 20110013 |
| Air skill | 20110012 |
| Water skill | 20110005 |
| Held/stowed bones | `mountMain` / `mountWaist` |
| Hold/stow animation parameters | `AxPutOn` / `AxPutOff` |

The prefab's attack-mode metadata has range `(0,1.8)`, switching range `(0,3)`, angle `[-30,30]`, frequency 0.5, and metadata damage 33. The database `WeaponInfo.ItemID=33` instead has frequency 0.9 and metadata damage 9; the remaining listed ranges match. `ItemData.cs:909–919` parses the database row into `ItemProto.weaponInfo`, whereas `PeSword.GetAttackMode`, `Attack`, and `IsInCD` access the prefab's `m_AttackMode`. No ordinary-knife assignment replacing that array from the database was found; creation-weapon construction has separate replacement code.

Neither metadata damage field is the skill's hit damage. Likewise, `AttackMode.maxRange` is not sufficient to reconstruct the animated collision volume or the exact time a player swing can hit. `AttackMode.IsInRange` uses strict minimum/maximum comparisons, and `IsInCD` compares elapsed time `<= frequency` (`PEWeapon.cs:13–42`); consumers of these metadata methods must be distinguished from the player action/skill path.

The recovered `AssetBundles/Item/knife.unity3d` visual bundle was built with older Unity 4.3 behavior components. Its static mesh may be reused, but its legacy skill component must not replace the newer source prefab's `PeSword` mechanics.

## Player attributes and actual damage formula

The first `initprop` row, player ID 1, defines HP 500/500, stamina 100/100, base attack 10, defense 0, critical probability 0, and `SwordMultiple=1`. Creation initializes these attributes; they are not inferred from UI bars. `PESkEntity.cs:121–156` clamps HP and stamina against their maxima through attribute pairs.

Equipment buff 30200087 has this expression for attribute 25 (attack):

```text
target.buffPreAdd += caster.sums[25] * target.sums[77]
```

Attribute 77 is `SwordMultiple`. With the baseline player and intact knife, attack is therefore `10 + 9×1 = 19`, before other combat modifiers. It is not 9, 33, or an invented fixed damage value.

All four relevant stationary-swing trigger rows, `skTriggerEvent` 493, 502, 503, and 237, use the same HP expression:

```text
A*A / (targetDef + A)
  * (target.EnableShieldBlock == 1 ? 1-target.ShieldMeleeProtect : 1)
  * (RandIn(caster.CritPercent) ? 2 : 1)
```

They also assign rigid attribute 29 to -1 and invoke `TryGetHurt(damage, 0.1f)`. The second parameter is **experience**, not a cooldown, random variance, or minimum damage. `SkExpFuncSet.cs:50–108` then:

1. Applies the contact's positive `damageScale`.
2. Checks faction/reputation/damage permissions.
3. Subtracts HP, updates damage timestamps, and emits hurt callbacks.
4. Awards the supplied experience separately.

Projectile shielding is a separate branch and is not the knife's melee path. Baseline critical probability zero means no baseline critical hits. Target defense and hit-part multiplier are required inputs; the hare's randomized scale changes its effective defense and HP, and Easy difficulty additionally halves its HP/attack/defense. Its source head multiplier is two while its body/limb multipliers are one (enemy audit). A reproducible damage helper can implement this numeric layer without claiming animated melee-hit parity.

## Attack sequencing and the missing animation contract

`Motion_Equip.cs:221` enables stamina spending for the main player's sword action. `Motion_Equip.SwordAttack` (`849–862`) requires the sword to be the current held hand-change equipment and no two-hand weapon. Merely owning or inventory-equipping a knife does not establish every held-weapon animation state.

`Action_Sword.cs:95–115,184–207` selects a skill according to air/water/movement state. The source field defaults are movement threshold 4 m/s and sprint threshold 6 m/s; these must still be checked against the live serialized player action before adopting them as final native-controller constants.

`skMain` for all five knife skills has a 0.2-second own cooldown, shared type 1 with shared time zero. The stationary skill has four stages, `CondTstFunc,SwordAttack` continuation checks and trigger rows `493;502;503;237`. Effect 8 cycles animation parameters `attack6,attack7,attack8,attack9`. Moving/sprinting first stages use `RunSwordAttack` / `SprintSwordAttack`; air and underwater skills have separate animation paths. The database's six-second main/post-main values are waits that animation events interrupt; treating six seconds as the swing duration is incorrect.

`Action_Sword.cs:290–298,358–380,497–518,530–599` establishes:

- Each attack start/combo spends the serialized five stamina multiplied by the player's reduction attribute. There is no hard low-stamina gate in this action's `CanDoAction`; low stamina slows animation.
- Animation speed starts at 1.37 and is multiplied by 0.9 for each reached threshold (50 and 20), then clamped to `[0.5,1.5]`. A short hit-response animation-speed curve can alter it further.
- Active attacks refresh the stamina-consumption timestamp each update, suppressing regeneration until the normal recovery delay after the action ends.
- `StartAttack_<bone>` ends preparation and enables the named hit trigger. First-/third-person-prefixed events are mode-filtered.
- `WeightInputStart`/`WeightInputEnd` delimit combo input; `EndAttack` disables collision after at least two frames since activation and advances the skill/combo.
- `EndAction`/`OnEndFullAnim` cancels the skill only when at least 0.3 seconds have passed since attack start, avoiding stale combo events.
- Motion follows animation root displacement while attacking; current training movement cannot reproduce that by continuing its normal walk velocity unchanged.

The knife prefab's attack trigger contains a capsule of radius 0.3, height 1.5, local Z axis, offset `(0,0,-0.1)`, attached to its animated transform. `PEAttackTrigger.cs:194–231` intersects attack capsules with enabled enemy defense capsules and passes the selected contact's damage scale; its hit bookkeeping prevents repeated frame-by-frame damage to the same contact during a window. A static forward ray or fixed 1.8-meter sphere would be an explicit approximation, not a recovered original rule.

**Recovery update:** [PLAYER_ANIMATION.md](PLAYER_ANIMATION.md) and its inventory tool now recover the installed player Animator mapping, authored clip event times, Avatar records and static hand mounts. Still required for faithful interactive attacks: dynamic humanoid weapon/hand bone transforms during each hit window, verified animation playback/root-motion behavior, and hit-response curve evaluation. Static knife art alone does not supply these. Combat should not be silently implemented as “click every 0.5 seconds for 19 damage.”

## Herbal Juice and health foundation

The exact source chain is `PrototypeItem916.skillId=20150023` → `skTriggerEvent46` → buff30200009. The skill has no prepare/main/ending delay, cooldown ten seconds and shared cooldown type 11/time ten seconds. The buff has stack limit one, duration ten seconds, interval one second, and:

```text
if (target.raws[Hp] > 0) target.raws[Hp] += 30;
```

It does not change stamina and does not resurrect a zero-HP entity. HP clamps at its maximum. `SkBuffs.cs:158–182` applies the effect on its first evaluation, then at most once per update when `elapsed > next_interval`, then checks expiry with `elapsed > 10`. Tick-before-expiry order permits a final tick just after ten seconds; an exact source port must preserve that ordering instead of assuming ten delayed ticks or replacing it with continuous 30 HP/second integration. Reapplying the same stack-limit-one buff replaces the old instance (`SkBuffs.cs:111–139`). Baseline full health means the tutorial's first juice can be consumed without a visible HP change.

## Native implementation and remaining work

`pe_training.c/h` and `pe_tutorial.c/h` now implement the first three items below. Acceptance is explicit and idempotent, knife ownership is conserved across equip/unequip, inventory counts exclude equipped items, and the live player must have an open inventory with no active dialogue to equip. Mission746 remains unsupported after its introduction; no attack, fabricated damage, or death-completion API was added.

Healing uses integer fixed-step ticks to preserve strict equality boundaries: immediate+30, subsequent ticks61/121/…/601 at60Hz, final tick601 before expiry, cooldown ends tick600. Reuse at600 replaces the old buff before its final tick. Headless regressions cover these boundaries, replacement, cooldown, no revival, HP clamp, no passive healing, and no repeated mission749 completion. Strict C11, ASan, UBSan and leak checks passed for the controller/tutorial suites.

The bounded contract is:

- `pe_training_accept_equipment_lesson`: mission 745 only, grant one unique knife exactly once, queue source entry dialogue. Keep ownership separate from equipment state so duplicate acceptance/equip/unequip cannot duplicate the item.
- `pe_training_equip_knife` / `pe_training_unequip_knife`: actual equipment mutation; only a newly accepted equip event during the active lesson completes objective 4104. Retain the narrower knife-only limitation until other weapon instances exist.
- Add HP/max HP initialized to 500 and explicit juice buff/cooldown state. Generalize the existing quickslot use guard to allow subsequent valid uses without replaying mission 749 completion. Process exactly the sourced buff timing above in fixed ticks; do not invent passive HP regeneration.
- Remaining: add a pure verified damage calculation accepting effective attack, target defense, shield state, critical decision, and contact multiplier. Contact generation remains a separate dependency. Test body/head results, zero critical baseline, shield factor, finite arguments, and HP clamping.
- Implemented: enter746 with its authored dialogue only after the real equip event. Remaining: request the source enemy spawn. A qualifying prototype-13 death may complete target 1025 once, queue 300230, and then stop at 719 until that lesson is implemented. Do not expose a UI button that fabricates death or completes combat.
- Defer player hit generation and exact wall-clock timing until the remaining animation pose/playback contract is recovered, or obtain an explicit decision to implement and label an approximation. Do not use metadata attack frequency as evidence for exact animation timing.

Regression coverage should include repeated acceptance, wrong/out-of-order equipment events, equip/unequip ownership conservation, weapon durability staying at 100 with decrement zero, HP500 initialization, immediate and strict-boundary juice ticks, ten-second cooldown, buff replacement, no zero-HP revival, target death idempotency, and separation between mission completion and the following mission's entry grants.

## Headless stationary knife contact resolver

`rebuild/src/pe_knife_contact.c/h` now connects the existing `PeCombat` event window and per-guide hit list, recovered `PeHitboxes` defence geometry, the pure damage expression, and a target HP value. `pe_knife_contact_resolve` accepts one actual world-space weapon capsule and one explicitly supplied target actor/clip/time/root pose. It does not create a weapon pose, find enemies, invent permissions, operate the training frontend, or advance a mission.

### Which overlapping body part wins

`PEAttackTrigger.CheckHit` (`Assets/Scripts/Attack/PEAttackTrigger.cs:194–231`) loops attack parts, then defence parts, in their serialized array order. Although its accumulator is named `mostCross`, the replacement test is strict `mostCross.distance < result.distance`. Every successful capsule collision sets `result.distance=0` (`PECapsuleTrigger.cs:100–119`), including calls made through its historical sampling wrapper. Consequently the **first intersecting enabled defence record wins**, rather than the nearest capsule, greatest penetration, smallest axis distance, or highest damage multiplier. The original knife has one attack capsule. Hare `Head1` precedes its body records, so a contact intersecting both selects the head multiplier even if the body intersection is deeper.

The C resolver iterates PEHB records in that order, skips attack-kind and disabled records, and retains the first strict overlap. It validates all enabled defence poses before committing, so a later invalid pose cannot leave a partially applied hit. Across separate defence-trigger objects, Unity broadphase/callback arrival order and the reverse pending-trigger iteration determine dispatch order. This API intentionally processes one host-ordered target per call instead of inventing a nearest-target ordering.

### Eligibility, successful contacts, and health

The attacker input supplies a stable nonzero entity ID, actual capsule, effective attack, an already resolved critical decision, and host eligibility. The target supplies its stable ID, actor/hitbox pair, evaluated pose, effective defense, HP/max HP, melee-shield attributes, and host eligibility. Self-contact, zero HP, a closed/inactive `PeCombat` window, a duplicate target or an exhausted native hit-list capacity cannot deal damage.

Eligibility is a host contract, not a substitute for source checks. It includes a living attacker with the held knife and compatible action; a collision-enabled skill guide; active defence trigger and matching layers; any skill-specific target restriction; and the reputation, faction-conflict and DamageID permissions in `SkExpFuncSet.cs:128–154`. The source trigger's `m_HitTriggers`/`m_EffectedTriggers` bookkeeping also belongs to the external dispatch layer. In particular, `CheckHit` can mark a trigger effected even when `OnHit` later refuses damage; this is distinct from `SkInst._hitInGuide`, which records the target only after a skill event executes successfully (`SkInst.cs:297–309`). The new resolver preserves the latter through `PeCombat`; it does not silently replace the former with its hit list. A host must not retry an otherwise suppressed trigger merely because this helper left its successful-hit list unchanged.

On a valid contact, the resolver uses the selected record's scale with `pe_combat_damage`, prepares the complete result, and then atomically commits the successful-guide target entry and reduced HP. A zero-damage shielded contact still counts as a successful contact, matching a successfully executed source skill event. Invalid values, invalid geometry, misses and ignored contacts leave action, target and result unchanged. HP is clamped at zero; the result distinguishes formula damage from actual HP loss.

`result.died` is true only for `hp_before>0 && hp_after==0`, matching the transition checked by `PESkEntity.CheckAttrEvent` (`Assets/Scripts/PeEntity/Cmpt/PESkEntity.cs:455–480`); the source attribute pair clamps HP into its valid range (`:110–131`). Repeated calls on a dead target cannot emit another transition. This provides evidence for the ordinary death-event dispatcher: the host still owns prototype identity, active mission/target checks, single-dispatch bookkeeping, loot and dialogue. No synthetic kill button or mission-completion call is present.

### Verified scope and limitations

This module performs **instantaneous** capsule contact. `PECapsuleTrigger.CheckCollision` also samples previous/current weapon endpoints with parent-center correction (`:146–183`), which remains outside the module. Even when a captured pose is exact, sampling it only once per frame does not establish swept-hit parity; fast movements can be missed. Its returned geometric contact normal is not the source motion-derived attack direction. Source hit reactions, rigid changes, XP, durability, shields beyond the melee factor, network authority, and callbacks are not implemented here. Those omissions are explicit; the helper does not invent replacement behavior.

`tests/test_knife_contact.c` exercises first-head versus deeper-body selection, disabled/attack-record exclusion, strict tangency, body/head/shield/critical damage, animated and transformed target poses, per-guide duplicates, actual death/overkill clamping, zero-damage contact consumption, permissions, dead/self/window gates, capacity, and atomic nonfinite/overflow/late-geometry failure. The resolver, existing combat tests and existing hitbox tests pass a strict C11 build with AddressSanitizer, UndefinedBehaviorSanitizer and leak checking. No new graphical or original-runtime combat validation was performed for this module.

### Combat death receipt and next lesson

The native training controller now exposes `pe_training_monster_died(training, prototype_id, entity_id)`. This is an authoritative host event boundary: the host must call it only after a real positive-to-zero HP transition. The function does not manufacture damage or death. It accepts prototype13 and a nonzero entity identity once, only while mission746 is active and its300229/4299 introduction has been acknowledged. It intentionally does not require fixed spawn261 or a player killer: source Quest_KillMonster1025 specifies `MonsterID=13_1_1`, and the audited source mission listener matches a prototype death.

The receipt marks target1025 complete but keeps mission746 active while presenting its source completion line300230. Only an acknowledgement matching that visible line transitions to719, which then presents4072 followed by300231. Herb gathering remains a separate headless ledger/host integration; no resource grant, herb depletion or later mission progress occurs here. Duplicate receipts, wrong prototypes, zero IDs, premature events and stale dialogue acknowledgements leave state unchanged. Earlier inventory, HP and stamina are conserved. The original24-byte tutorial snapshot still rejects these extended states; no persistence coverage is implied.

The three added English lines come directly from `localData.bytes`: Scenario IDs300230/4072/300231 map to Translation IDs3300230/3004072/3300231, all speaker9041. Training/tutorial regressions including ordered dialogue, wrong/premature/duplicate death events and the719 boundary passed strict compilation and ASan/UBSan with leak detection. These tests validate dispatch behavior, not an observed original-game kill or a completed native graphical combat playtest.
