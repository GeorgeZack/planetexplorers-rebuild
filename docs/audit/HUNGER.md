# Player hunger: drain, starvation and food

October 14, 2026. **Source-derived and implemented in C as a disconnected
owner.** `pe_hunger` is not called by either host: no HUD bar, no eating, no
save field. No original-game session or desktop input was used.

## Why now

The town shops already sell ten foods (384–392, 394) from the recovered
catalog, but the rebuild has no hunger attribute for them to act on. The
[replication plan](../REPLICATION.md#survival-and-equipment--partial) names
hunger as the next survival attribute.

## Source rules

All values come from `Assets/Resources/DataBase/localData.bytes` and are pinned
by [export_hunger.py](../../rebuild/tools/export_hunger.py), which writes
[the header](../../rebuild/generated/pe_hunger_source.h) and
[metadata](../../rebuild/generated/pe_hunger_source.json).

**The attribute.** `AttribType` 15–18 are HungerMax, Hunger,
HungerReducePercent and HungerRecoverSpeed. Player `initProp` row 1 starts at
300 of 500, with percent 1 and speed −0.0694. Hunger and HungerMax are an
`AttribPair`: the summed value clamps to 0..max.

**Drain.** Initial buff 30200050 (type 65507, every 5 s, no lifetime) runs
`raws[16] += sums[18] * sums[17] * 5`: −0.347 per pulse, about 72 minutes from
300 to 0. It shares the passive-HP scheduler already recovered for
[HP](PASSIVE_RECOVERY.md) and [stamina and comfort](STAMINA_RECOVERY.md): the
eight-frame attribute pass, a first execution at mount, at most one overdue
interval per pass, and a reset on remount.

**Starvation.** `AbnormalType` 8 (饥饿) is read by `PEAbnormal_N`, updated every
frame by `AbnormalConditionCmpt.OnUpdate`:

| | While inactive | While active |
|---|---|---|
| Timer | `Trigger_Time` 1 s | `RT_Time` 0.1 s |
| Condition | `Hit_Attr 16,0,50,1,1`: hunger in [0,50], rate 1 | `RH_Attr 16,51,1000,1,1`: hunger in [51,1000], rate 1 |
| Also | the player is alive | — |
| Effect | mounts buff 30200113 | cancels it |

Each timer (`PEAT_Time`) is checked before this frame's `Time.deltaTime` is
added, and a check that passes resets it to 0. Starting or ending clears both
timers. Hunger strictly between 50 and 51 neither starts nor ends it. It is not
removed by death (`IsDeathRemove` 0), and an active condition is saved with its
buff. `Eff_BodyWeight` thins the character model, which is presentation.

**Starvation's effect.** Buff 30200113 pre-adds, into
`sum = buffMul·(raw + buffPreAdd) + buffPostAdd`: StaminaReducePercent +1,
StaminaRecoverSpeed −10, StaminaRecoverDelay +2, ComfortReducePercent +1 and
OxygenReducePercent +1. With the starter values, stamina costs double, a
stamina pulse adds 30 instead of 40 after a longer delay, and comfort drains
twice as fast.

**Food.** Each food's skill has a 4 s cooldown (shared group 12) and two
triggers. The first is an `exp` expression: `raws[16] += gain`, and its
nutrient attribute (45–49) `+= n`, capped at 100. The second (489) is
`rnd,1,1,30200105,0.01`: a 1% chance of 20 s food poisoning, −3 HP per second
while HP is above 3.

| Item | Food | Hunger | Nutrient |
|---|---|---|---|
| 384 | red mushroom soup | +30 | Bean +2 |
| 385 | fried lotus | +60 | Bean +4 |
| 386 | roasted meat | +80 | Meat +4 |
| 387 | puree | +20 | Fruit +4 |
| 388 | nuts | +30 | Fruit +6 |
| 389 | smoked sausage | +60 | Meat +3 |
| 390 | sashimi | +80 | Meat +4 |
| 391 | compressed biscuit I | +20 | Energy +2 |
| 392 | compressed biscuit II | +40 | Energy +4 |
| 394 | ration sticks | +30 | Energy +3 |

## The C owner

[pe_hunger.h](../../rebuild/src/pe_hunger.h) holds raw and summed hunger, the
drain phase, the starving flag, both condition timers and the food cooldown.
`pe_hunger_frame` takes the same before/after passive-lifecycle snapshots as
the stamina owner, plus the frame's scaled time and whether the player is
alive. `pe_hunger_eat` raises raw hunger for a supported food and starts the
cooldown. `pe_hunger_starvation` returns the five pre-adds while starving.

Adapters, not original behavior:

- The condition runs after the attribute pass within a frame; the script
  order between `AbnormalConditionCmpt` and `SkEntityAttribsUpdater` is not
  established.
- Unity's `Random.value` can return exactly 1.0, which would refuse a rate-1
  roll; that case is ignored.
- The food cooldown counts down scaled time, as the medpack's does.

Not owned here: nutrients and their decay buffs (30200149–152), food
poisoning, the body-weight presentation, the abnormal notices, oxygen and
every other food.

## Verification

- `core:hunger`: the fresh drain at mount, the delayed remount, the strict 5 s
  interval and the single overdue pulse; starvation starting on the check
  after a full second at hunger 0 and 50, never above 50 or for a dead player;
  ending on the 0.1 s check at 51 and not between 50 and 51; death not ending
  it; the pre-adds; food gains, the clamp at the next pass, the 4 s cooldown
  and refusals; invalid input leaving the state unchanged.
- `test_hunger_export.py`: deterministic regeneration with the originals
  unchanged, and rejection of changed player values, drain, starvation row and
  buff, food skills, gains and poisoning chance, and the condition's clock,
  range and roll source.

## Before connecting it

Connecting it needs decisions that change play and saves:

1. **Save:** PECSv23 appending the summed hunger and the starving flag; older
   saves would start at the prototype's 300, not starving.
2. **Stamina and comfort:** the starvation pre-adds must reach
   `pe_stamina_recovery` and every stamina cost owner.
3. **Use:** eating through the shared item-use path (inventory, quickslots),
   with the 4 s group-12 cooldown.
4. **HUD:** the hunger bar in `pe_ui_hud` for both hosts. The tutorial does not
   run Adventure survival, so it would stay without hunger.
