# Player hunger: drain, starvation and food

October 14, 2026. **Source-derived and implemented in C, connected in the
Adventure sandbox** ([below](#connected-in-the-sandbox)). No original-game
session was used. The food loop (purchase, bag and quickslot eating, cooldown
retries, Pause save/load) is observed with ordinary input in the native
runtime ([acceptance](#ordinary-native-acceptance-of-the-food-loop));
starvation in play and any original-game comparison are not.

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
poisoning, the body-weight presentation, oxygen and every other food. Of the
abnormal notices, only the starvation-start notice's text is exported (for the
host, [below](#hud-bar-and-starvation-notice)); the periodic hunger notices
and notice audio are not.

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
  HP and stamina. Starting starvation puts the original's notice text on the
  message line on every start ([below](#hud-bar-and-starvation-notice); the
  original shows it once per player lifetime); ending it says nothing, as in
  the source.
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
  "(starving)", and the shared HUD draws the original's hunger bar
  ([below](#hud-bar-and-starvation-notice)).

Verification: `core:hunger_game` (eating, cooldown, quickslot, dead player,
Legacy refusal, starvation doubling a tool cost, a starving stamina pulse of
60 and a doubled comfort drain, the v23 round trip, a literal v22 at 300 and
four malformed suffixes); `test_sandbox_health_host` (the drain on the first
attribute pass, starvation after a full second at hunger 40 with its message,
pause freezing the condition clock); every older save suite updated for the
8-byte suffix.

**Not checked:** starvation in play (hunger takes about 72 minutes of unpaused
play to fall from 300 to 50) or the original game. The food loop is observed
in [ordinary native play](#ordinary-native-acceptance-of-the-food-loop).

## HUD bar and starvation notice

**October 14, source-derived and implemented in C; rendered only in a staged
private-display fixture.**

*Source.* `UIMainMidCtrl.Update` calls `UpdatePlayerAtrbute` every frame,
which sets `mScHunger_5.sliderValue = Hunger / HungerMax` (0 when the maximum
is not positive), with no lerp; HP and stamina are the only lerped values, and
only comfort has a face icon and alpha curve. `mScHunger_5` is component
11489644 in `GameMain.prefab`; the recovered layout
([TRAINING_UI_LAYOUT.json](TRAINING_UI_LAYOUT.json)) gives its widgets under
`ButtomAnchor/MainMid/Content/CenterInfo/5_Hunger`:

| Widget | Sprite | Local | Size | Notes |
|---|---|---|---|---|
| Progress Bar | — | (−10,−2) under 5_Hunger (100,29) | — | rotated 180° about Z |
| Background 11435264 | `bloodlan_12bg` | (0,0.5), pivot left | 64×5 | depth 6, white |
| Foreground 11413214 | `bloodlan_12` | (0,0.5), pivot left | 64×5 | depth 7, white; filled sprite, horizontal, not inverted |
| Sprite (arr3) 11480874 | `tili` | (0,−0.5) | 12×13 | the drumstick icon |

The rotation puts the left-pivot widgets at screen x 666–730 (reference
1280×720) with their origin at the right end, so the fill grows leftward from
next to the icon. In the atlas `bloodlan_12` is `bloodlan_11` (comfort)
already turned 180°; after the prefab's rotation the two bars show the same
segment shape. The rebuild's earlier HUD drew `bloodlan_12bg` unrotated.

*Starvation notice.* `AbnormalNotice` has one row for condition 8: no timer,
trigger on the condition starting (`Alnormal.evtStart`), rate 1, no audio,
content 8000654. `PEAE_Content` registers it with `PeTipMsg` (normal level,
misc type, the player's face). Its English text, from `i18n.db`, is "I'm so
hungry, I can probably eat an entire cow!".

The notice is one-shot. `PEAbnormalNotice.ApplyEffect` runs its effects, then
calls `m_Triggers.Clear()`, which empties the trigger list (not one trigger),
so `Update`'s `triggerHit = m_Triggers.Count > 0` stays false for that
instance from then on. `MainPlayerCmpt.Start` starts `UpdateAbnormalNotice`
only outside the tutorial; it waits 5 s, creates one instance per notice row,
and updates them every frame for the component's lifetime. So in the
original, starvation shows the notice at most once per main-player lifetime
(a load recreates the player), and a start in the first 5 s, including a
loaded starving player's `Deserialize` → `ApplyEffect`, shows nothing because
the notice has not subscribed yet. Two further hunger notices (hunger
0–10 every 60 s at rate 0.5–0.2, content 8000653; comfort 0–150 with hunger
6–20, content 8000656) are random periodic rolls and are not implemented.

*Implemented.* `pe_training_ui_layout` has a `hunger` rectangle (666,625,64,5,
bottom-centre anchored). `pe_ui_hud_draw` draws `bloodlan_12bg` there rotated
180° (`pe_ui_draw_sprite_flip`, both axes) in both hosts, replacing the
unrotated decoration, and, only when the host's snapshot sets
`hunger_available`, draws `bloodlan_12` rotated and clipped to the right
`hunger/max` of the track. The sandbox sets it in Adventure from
`PeGame.hunger`; Legacy and the tutorial leave it unset and show the empty
track. `export_hunger.py` pins the notice row and exports its text; the
sandbox puts it on the message line when starvation starts.

Adapters, not original behavior:

- `UISlider` and the filled sprite live in the compiled NGUI plugin
  (`Assembly-CSharp-firstpass.dll`), which was not decompiled. The fill uses
  the same horizontal scissor clip as the existing health and stamina bars;
  a `fillAmount` clip or a left-pivot scale would both grow from the same end.
- The message line stands in for `PeTipMsg`'s face-and-text tip list.
- **The rebuild repeats the notice on every starvation start**, not once per
  player lifetime, and has no 5 s window. Matching it would need a new
  transient owner reset on load, new game and menu round trips; it is not
  claimed. A starving save loads without a message, as in the original, only
  because loading sets the condition without a start event.
- No hover text is added; the source shows none for hunger.

Verification:

- `ui-shared-test` (`test_ui_hud`): at four sizes, the rotated track always
  draws and the fill never does without host data; at 250/500 the fill's clip
  starts at half the track and ends at its right edge; above the maximum it
  fills the track; 0, NaN or a zero maximum draw no fill; the 1280×720
  rectangle is (666,625,64,5).
- `ui-shared-render-capture` on private Xvfb `:110`: the tutorial and Legacy
  sandbox HUDs still match exactly (93,170 / 23,320 / 5,936 pixels). The
  Adventure HUD with hunger 150/500 matches the tutorial everywhere outside the
  track (92,850 / 23,224 / 5,902 pixels) and its teal fill covers columns
  710–729 of 666–730 at 1280×720 (355–364 of 333–365 at 640×480, 177–181 of
  166.5–182.5 at 320×240), against 30% expected. Captures:
  `rebuild/build/ui-shared-captures/sandbox-hud-hunger-*.png` beside
  `tutorial-hud-*.png`.
- `test_sandbox_health_host`: the starvation message is the exported notice;
  ending starvation leaves an unrelated message untouched; a later start shows
  the notice again (the adapter above).
- `test_hunger_export.py`: the notice row's rate, content list and audio, the
  `PeTipMsg` call, the one-shot `m_Triggers.Clear()`, and the main player's
  5 s, non-tutorial start are pinned; `--localization` with the default
  database checks and a missing one fails. `make cli-test` covers only the
  game executable, not exporters.

**Not checked:** an original-game screenshot of the hunger bar or readability
at small sizes (at 320×240 the track is 16×1.7 pixels). The bar in ordinary
play is now observed ([below](#ordinary-native-acceptance-of-the-food-loop)).

## Ordinary native acceptance of the food loop

**October 14, observed in the native production runtime with ordinary input;
no original-game comparison.** Evidence: [manifest, journals and lossless
crops](../reference/food-native-20261014/checkpoints.json). Full captures and
saves stay under `rebuild/build/food-private-20261014-0{1,2}`.

*Setup.* Runtime `6939ae0`; the runtime C sources are unchanged since (the
uncommitted work is the save inspector, which is C, and the Python input
helper, neither part of the game binary), binary SHA256 `9ee96016…3d01`. Baseline: the earned October 7
completed hunt save (`hunt-private-20261007-03/completed.pesave`, SHA256
`9a28960b…e0d2`, the hash [recorded there](HUNT_ORDINARY_ACCEPTANCE.md)). It
predates PECSv23, so its hunger is the loader default 300. The
[private-input supervisor](NATIVE_PRIVATE_INPUT.md) owned the display; no
fixture, cheat, teleport, save edit or grant was used.

*Session 01* (`:111`) was aborted by the supervisor's watchdog before any
purchase or save: `input-watchdog-abort`, reason `invalid input lease action`.
The supervisor had loaded the helper before the `wheel` action was added, so
the new action's lease failed its check; the game was killed (−9), baseline
unchanged. Nothing from it is claimed.

*Session 02* (`:112`), all observed:

| Step | Input | Result |
|---|---|---|
| Talk, shop | right-click Xing Zhang (20028), Shop, wheel 5 notches | buy list rows 6–13 of 57 |
| Buy | select Compressed Biscuit I (391), + twice, Buy; select Biscuit II (392), Buy | "Bought 3 … for 54", "Bought 1 … for 34"; money 325→271→237, stock 35→32 and 20→19 |
| Save | Pause → Save | `purchase.pesave`: hunger 292.713318, 391 ×3, 392 ×1 |
| Bag eat | right-click 391 in the bag | "Compressed Biscuit I eaten.", 391 3→2 |
| Bag retry | right-click 392 0.40 s later (wall clock) | nothing used: 392 stays 1 |
| Quickslot | drag the 391 stack to quickslot 4 | quickslot 4 = item 391 |
| Ignored | key 4 with the inventory open | no effect: the rebuild drops quickbar digits while any panel is open; not a cooldown test |
| Quickslot eat | inventory closed, key 4 | cooldown overlay appears on quickslot 4 |
| Quickslot retry | key 4 again 0.75 s later (wall clock) | nothing used |
| Save | Pause → Save | `eaten.pesave`: hunger 325.773621, 391 ×1, 392 ×1, quickslot 4 = 391, money 237, stock 32/19 |
| Reload | Resume ~15 s, Pause → Load, Save while paused | in-game clock back to the saved 16:09; `reloaded.pesave` byte-identical to `eaten.pesave` (SHA256 `40e47365…e114`), identical inspection |

The saved values are consistent with the source arithmetic: 300 − 21 × 0.347
= 292.713 for the purchase save, and 292.713318 + 2 × 20 − 20 × 0.347 =
325.773318 for the eaten save (0.0003 from f32 summation). The 21 and 20 pulse
counts are inferred from that arithmetic; the drain events themselves were not
observed. Had either retry eaten, 392 would be 0 or 391 would be 0 and
hunger 20–40 higher. The wall-clock gaps only show that each retry fell inside
4 s of its eat; they are not a measurement of the engine's cooldown clock.
The cooldown is transient and resets on load, so the saves do not carry it.

The live bar tracked hunger. The fill (colour `#41b9b9`, row 625) starts at
x 691, 692, 690, 688, 688 and 688 across the start, purchase, bag eat,
quickslot eat, retry and pre-reload captures, against 730 − 64 × hunger/500 =
691.6, 692.5, 690.0 and 688.3. This is a pixel measurement of screenshots, not
saved state.

*Observed, not changed here.*
- The quickslot eat showed no message: the HUD message line re-shows text only
  when it differs from the last message (`pe_sandbox_ui.c`), and the text was
  identical to the bag eat's. This applies to any repeated identical message.
- In the source, quickbar keys are gated by `PeInput.InputEnable` (focus,
  creation editor, plot animation) and `Key_wordMgr.enableQuickKey` (cleared
  only by `UIBuildBlock`), not by the inventory window. The rebuild's panel
  gate therefore differs from the original. It is outside this goal and
  unchanged.

*Tooling.* The read-only save inspector (schema 3) adds `hunger` (value,
starving) and `food_offers` (offer index, vendor, item, price, limit and saved
stock for every supported food). `native_private_input.py` gains bounded
`drag` and `wheel` actions ([contract](NATIVE_PRIVATE_INPUT.md)).

**Not checked:** starvation in play, other vendors or foods, food on the
tutorial or Legacy hosts, or the original game.
