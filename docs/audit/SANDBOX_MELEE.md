# Sandbox stationary knife melee

The C sandbox now supports drawing, stowing and stationary third-person swings with equipped Knife 33. The frontend passes **zero combat targets**. It does not damage residents, spawn creatures, increment a hunt counter or complete quest 9126. The reusable contact module can process up to six real targets, but its passing tests do not establish a playable six-hare encounter.

This audit records source-derived contracts and implemented C behavior. Original-game encounter observations and physical native checks belong in [PLAYTEST](../PLAYTEST.md) and the [post-medicine audit](ADVENTURE_POST_MEDICINE.md). The later physical draw/swing/stow/cancellation checks are recorded there; exact numerical clocks and six-target contacts remain automated evidence.

## Source and asset contracts

[pe_combat](../../rebuild/src/pe_combat.c) retains source stationary knife skill 20110011 events, combo windows, deduplication and stamina-dependent playback. [Knife collision](KNIFE_COLLISION.md), [animator baking](ANIMATOR_BAKE.md) and [training combat](TRAINING_COMBAT.md) document the underlying recovered evidence. This integration adds no guessed attack schedule.

[pe_sandbox_actor](../../rebuild/src/pe_sandbox_actor.c) loads the optional `player-knife-drawn/recording.peas` group. A supplied held group requires `player-knife-stowed` and matching `Attack6` through `Attack9` in the main `player` body. A stowed-only installation remains usable without melee. The held group validates source female 30 fps clips, matching body/tool frame counts, durations, looping flags and terminal timestamps before upload:

| Clip | Frames | Duration, seconds |
| --- | ---: | ---: |
| Attack6 | 34 | 1.0666667222976685 |
| Attack7 | 35 | 1.100000262260437 |
| Attack8 | 40 | 1.2666668891906738 |
| Attack9 | 31 | 0.9666671752929688 |

The frontend borrows the first body and held-knife actors through `pe_sandbox_actor_melee_assets()` and loads `player-knife-drawn/knife-drawn.pehb` against that exact knife actor. The module requires one enabled right-hand attack capsule of form 0 and nonlooping body/knife clocks covering all source mechanical events. Missing or incompatible supplied assets fail closed; no generic blade or stowed-pose substitution is used. The frontend owns the PEHB and frees it before actor teardown; the module owns no assets.

The fixed appearance remains the recovered female starter. PEAC/PEHB validation cannot independently establish mountMain attachment or gender provenance; the exporter/asset selection contract still matters. This is not general avatar support.

## Controls and admission

In Adventure, equip an intact Knife 33 from the inventory. The first fresh left press draws it. Release, then press again while stationary on the ground in third person to start a swing. A fresh press during the source combo window requests the next stage; holding the button never automatically starts or repeats an attack. An out-of-window continuation is rejected rather than queued for a later window. R or right press stows it; F5 selects the supported third-person camera mode.

The host uses the UI's pointer ownership/release gate. Closing a modal while its button is held does not supply a new world attack. Equipment replacement, loading, death, focus loss, opening a modal, Build mode, first-person mode and stow cancel pending/active attacks before rendering, including display frames with no simulation tick. Movement, jump, ungrounded state or physical horizontal speed above .05m/s also discard an unsupported pending press immediately and cancel an active swing; the held knife may continue in ordinary locomotion. Fixed-tick admission independently repeats those checks.

Ownership and drawing are bound to the exact equipped inventory instance, not merely item 33. The module records that identity at begin and cancels when it changes. The renderer rechecks the equipped item/instance and life before using an attack snapshot. A replaced Knife 33 cannot inherit the previous knife's animation.

Draw/stow is immediate and held locomotion uses the recovered Normal clips. These are explicit native adapters. Full Sword draw timing, moving/air/first-person attacks, animation transitions, root motion and source action-interruption semantics are not claimed. Knife 33 is excluded from the generic mining fallback in every mode.

## Clock, stamina and pose ownership

[pe_sandbox_melee](../../rebuild/src/pe_sandbox_melee.h) owns one `PeCombat` action. Each accepted step advances exactly one scaled 1/60 s tick, regardless of target count. Clip delta is that tick multiplied by the source stationary TP rate: knife speed 1.37, multiplied by .9 at stamina≤50 and again at≤20, clamped to [.5,1.5]. The combat core preserves the source event ordering, minimum contact updates, .3s action-end guard and stage transitions; it does not derive duration from item frequency.

Each accepted start or combo stage charges `5 * pe_player_defaults.stamina_reduce_percent`, clamped to zero. With current default reduction 1, four stages charge 20 stamina. Low stamina is allowed. Starting an unsupported action charges nothing. The host suppresses ordinary recovery throughout an active tick, including its ending tick; the existing game recovery delay applies afterward. Cancellation does not refund an already paid stage. Current item/default values give attack 19 from player base 10 plus Knife 33 attack 9. Buffs and equipment modifier evaluation remain outside this slice.

The frontend passes `game->player`, scale 1 and yaw `180 - game->yaw * RAD2DEG` to both blade posing and body/tool rendering. After the general tool pose setter, `pe_sandbox_actor_set_melee()` copies the authoritative stage and `clip_time`. Redraws, locomotion updates and UI previews never advance combat time. Preview shows held idle rather than an active strike. No action pointer or transient clock is added to the save; loading cancels the action and stows the knife.

## Multiple-target helper and encounter boundary

`pe_sandbox_melee_step()` accepts up to six host-ordered `PeKnifeContactTarget` records. Each must contain a stable unique entity ID, real current HP/maxHP, defence attributes, matching actor/PEHB, and the same clip/time/root/yaw/scale used for its visible pose. The host must supply attacker/target permission and already-resolved critical decisions. Geometry alone does not authorize damage. The present frontend supplies no records and false attacker eligibility; its reserved transient player identity 1 is not a recovered population binding.

The helper validates every target before committing. Clock, stamina, output and all target HP remain unchanged on invalid data, including a bad last target after earlier targets could have overlapped. Successful steps mutate only HP in the caller's records. The encounter host must persist those HP values; `PeHare.max_hp` is not current HP and the melee module owns no duplicate HP pool.

One shared skill-guide list deduplicates each target per stage. A later stage can hit it again. The resolver uses source-ordered enabled defence capsules and instantaneous strict overlap; Unity trigger ordering and swept history are not reconstructed. Death is emitted only for an actual positive-to-zero HP transition, once when the caller retains current HP. A valid death still requires the separate source objective membership/permission dispatcher before any quest change. The helper owns no spawning, lessons, reward grants or kill counters.

General reputation/faction/skill-guide continuation, hostile perception, hare retaliation, damage reactions, corpse behavior and encounter persistence/integration remain separate work. Existing hare foundations and source capture tools do not by themselves make those features playable here.

## Automated verification

[tests/test_sandbox_melee.c](../../rebuild/tests/test_sandbox_melee.c) uses loaded synthetic PEAC/PEHB fixtures to exercise:

- One clock update across six targets, six contacts per stage, 24 contacts across a four-stage chain, and one stamina debit per stage.
- Per-target permission rejection, misses, dead targets, six one-time deaths and low-stamina admission.
- Atomic invalid final target, duplicate IDs, wrong defence geometry and excessive target count.
- Cancellation before a would-be contact on modal closure of the action gate, unequip, ownership loss, stow and exact-instance replacement.
- Zero-target ticks and recovered body/knife/hare fixtures through all four stages without manufacturing a hit.

[tests/test_sandbox_actor.c](../../rebuild/tests/test_sandbox_actor.c) checks complete-group admission, exact source clocks, malformed-group cleanup, borrowed actor access, snapshot time through redraw/pause/preview, and instance/death/cancellation fallback while retaining mining and Torch coverage. Focused strict builds and ASan/UBSan with leak detection pass. The full optimized core also passed after module registration with the generalized hare changes. These tests establish implementation contracts, not physical source-game parity.

For a graphics initialization check, launch the rebuild with a copied Adventure checkpoint and `--smoke 2 --screenshot PATH`. Smoke mode does not enable world pointer input, so it cannot verify draw/swing/combo controls. A live native check should cover first-click draw without stamina charge, second-click swing, timed combo, R/right stow, inventory cancellation, movement rejection and absence of world edits/quest progress. The exit diagnostic reports swing stages and the zero-target boundary.
