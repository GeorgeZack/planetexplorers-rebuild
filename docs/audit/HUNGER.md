# Player hunger: drain, starvation and food

October 14, 2026. **Source-derived and implemented in C, connected in the
Adventure sandbox** ([below](#connected-in-the-sandbox)). No original-game
session or desktop input was used, and nobody has played with it yet.

## Why now

The town shops already sell eight foods (384–389, 391, 392) from the recovered
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
OxygenReducePercent +1. StaminaReducePercent multiplies every stamina cost
(`Action_HumanMove` sprint and jump, `Action_DigTerrain`, `Action_Fell`,
`Action_Sword`) and also the recovery pulse `raws[3] += sums[5] * sums[4]`.
With the starter values, costs double, a pulse adds (40 − 10) × 2 = 60 instead
of 40 but only after a delay 2 s longer, and comfort drains twice as fast.

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
| 391 | compressed biscuit I | +20 | Energy +2 |
| 392 | compressed biscuit II | +40 | Energy +4 |

Sashimi (390) and ration sticks (394) use the same pattern but have no row in
the rebuild's item catalog, so the exporter leaves them out.

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

## Connected in the sandbox

**Implemented in C, October 14, by user request.** Adventure only; Legacy
games and the tutorial have no hunger.

- **Clock.** `sandbox_health_frame` runs `pe_hunger_frame` after the stamina
  owner on the same attribute frame, with the display frame's scaled time and
  whether the player is alive. Pause and focus loss freeze it, as they freeze
  HP and stamina. Starting or ending starvation sets the HUD message line
  ("You are starving. Eat something." / "You are no longer starving."); the
  original's abnormal notices are not recovered.
- **Starvation's effect.** `pe_game_stamina_reduce_percent` returns 1 or 2 and
  replaces the constant at every stamina cost site: sprint drain, jump, tool
  stats, tool action charges and melee's setup (refreshed before each blade
  tick). The stamina owner's `PeStaminaVitals` gains the two ReducePercent
  pre-adds; the host also adds the recovery speed and delay pre-adds. Each
  frame reads the previous frame's condition.
- **Eating.** `pe_game_use_slot` eats a supported food in Adventure: one item
  is used and raw hunger rises, unless the player is dead or the 4 s cooldown
  is running. Inventory right-click and quickslots reach it through the new
  `pe_game_item_is_consumable`, and the quickbar shows the cooldown.
- **Save.** PECSv23 appends the summed hunger (f32) and the starving flag
  (u32). Older saves start at 300, not starving. A flag other than 0/1 or a
  hunger outside 0..500 fails without replacing the live game.
- **Display.** The character panel's existing Hunger row shows the value and
  "(starving)". The original's mid-screen hunger bar is not added; the shared
  HUD does not carry comfort either.

Verification: `core:hunger_game` (eating, cooldown, quickslot, dead player,
Legacy refusal, starvation doubling a tool cost, a starving stamina pulse of
60 and a doubled comfort drain, the v23 round trip, a literal v22 at 300 and
four malformed suffixes); `test_sandbox_health_host` (the drain on the first
attribute pass, starvation after a full second at hunger 40 with its message,
pause freezing the condition clock); every older save suite updated for the
8-byte suffix.

**Not checked:** anyone playing it. Hunger takes about 72 minutes of unpaused
play to fall from 300 to 50.
