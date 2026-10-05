# Stationary training melee host

[pe_training_melee.c](../../rebuild/src/pe_training_melee.c) connects source-derived stationary third-person knife action events to actual recovered blade and hare capsules. It charges stamina through the existing combat core, poses the knife on the same authored clock used for body rendering, resolves spatial contact, and stores the hare's current HP. It never emits an enemy death because an attack button was pressed or an animation ended: `contact.died` is true only when a spatially accepted hit changes positive HP to zero.

This is a bounded native host, not complete original melee parity. It omits swept/history capsule sampling, original Animator transition blending, attack root motion, hit response, general permission/skill continuation, AI reactions and critical RNG generation. Those limitations remain material even though pose data, source hit windows and numeric damage are recovered.

## Assets and source contracts

The host borrows three actors: one body part, the **drawn** rigid knife and the hare, plus matching knife/defence PEHBs. Their ownership remains with the frontend and must outlive the host. [Animator baking](ANIMATOR_BAKE.md), [knife recovery](KNIFE_COLLISION.md) and [hitbox recovery](HITBOX_RECOVERY.md) describe their provenance.

The current actual female assets are:

- `rebuild/build/animator-bake/player-female/all/player-part-0-material-0.peac`
- `rebuild/build/animator-bake/player-female/drawn-all/knife-drawn-part-0-material-0.peac` and `knife-drawn.pehb`
- `rebuild/build/reference-assets/hare.peac` and `hare.pehb`

Drawn export is mounted through source `mountMain`; `mountWaist` stowed data and observational wall-clock recordings must not be supplied as attack poses. PEAC contains neither attachment provenance nor gender, and a PEHB geometry fingerprint deliberately excludes animation palettes. Therefore the host cannot prove these semantic facts merely from a valid binary. The caller must select the verified exporter output and matching gender. Initialization checks all four named Attack6–9 clips, exact body/knife FPS/frame-count/duration/loop/terminal-timestamp agreement, nonlooping attacks, event schedules covered by each clip, one enabled right-hand sharp-metal attack capsule and valid ordered target defence geometry. Missing or incompatible data fails atomically; no proxy capsule or stowed-pose substitution is provided.

The numeric/event implementation remains [pe_combat](TRAINING_COMBAT.md) and [pe_knife_contact](../../rebuild/src/pe_knife_contact.h). Source default knife attack19 follows base attack10 plus item attack9. Normal scale1 hare stats are maxHP212 and defence29; scale/difficulty changes come from the actual `PeHare` spawn and are passed into setup, not guessed from capsule size. Current HP is separate from `PeHare.max_hp`.

## Host API

[pe_training_melee.h](../../rebuild/src/pe_training_melee.h) defines:

- `init(host,setup)`: validate borrowed assets, source gender, stable distinct player/target IDs and bounded numeric stats; retain four body/knife clip mappings and current HP.
- `begin(host,gate,&stamina)`: require alive, owned/equipped/drawn knife, grounded third-person view, no competing action, no requested movement and measured horizontal speed≤.05m/s. Successful source begin charges five stamina times the reduction attribute. Low stamina remains allowed and clamps to zero; unsupported scope rejects without mutation.
- `queue(host)`: pass a deliberate continuation request through the source combo-window gate. Requests outside the window do not queue a future automatic combo. The four-stage source core resets guide deduplication and charges each accepted next stage once.
- `step(host,frame,&stamina,&update)`: exactly one scaled1/60s update. It computes authored delta from the source stationary TP endpoint rate and stamina-dependent knife animation speed, advances PeCombat once, then poses the actual blade at `action.stage` and `action.clip_time`. The frontend renders body and blade using that identical clock/root transform. Target clip, seconds, transform and uniform scale come from the exact rendered hare pose. Source timing is not derived from weapon metadata frequency.
- `cancel(host)`: close contacts/combos without manufacturing a hit or refunding consumed stamina. A valid frame that becomes moving, airborne, first-person, unheld, unequipped, dead or action-incompatible also cancels before contact. This explicit restriction excludes unsupported moving/air/FP skills; it is not a claim that the original game forbids those attacks.

The step output contains action advancement, the posed weapon capsule, contact status and the resolver result. Invalid frame/pose data leaves host, stamina and output unchanged. Successful contact changes HP using the source damage formula and first overlapping enabled defence part in source order. Further overlapping frames do not damage the same target again within that skill guide. New stages may hit again. Zero-damage shielded contacts still consume their successful per-guide deduplication slot. A dead target cannot generate another positive-to-zero event.

The frontend must suppress ordinary stamina regeneration while `action.active`, consume an actual death result once through its source objective dispatcher, and preserve the mission's dialogue/acceptance ordering. The module contains no mission IDs, fake kill shortcut, inventory grant or automatic719 entry. A target moving away can cause a genuine miss even when the animation and hit window are valid.

## Permission and interaction limits

Frame input explicitly supplies attacker/target eligibility and a resolved critical flag. The source `PEUtil.CanDamage` also checks reputation, `ForceSetting.Conflict` and nonzero `DamageData` pair values. Original attack-trigger effected/hit lists are distinct from successful skill-guide deduplication. These systems have not been generally ported, and passing `true` is not proof they were evaluated. For the initial frontend slice, an explicit known tutorial player↔prototype13/spawn261 whitelist must be described as a bounded permission policy. It must not silently authorize arbitrary targets. Critical RNG is not generated by this host; a noncritical input is a deliberate deterministic limitation.

Source `Action_Sword` also rotates toward its supplied attack/aim direction and requests movement from animation root displacement. This host uses the caller's actual root/yaw for both render and capsule and does not invent either rotation or displacement. The frontend must expose its chosen facing policy. Source draw/stow animation timing, attacking while walking, FP hand rendering, full skill cooldown routing, damage reactions and corpse/death animation are separate work.

## Verification

[tests/test_training_melee.c](../../rebuild/tests/test_training_melee.c) exposes `pe_test_training_melee()` and passes strict C11 plus AddressSanitizer, UndefinedBehaviorSanitizer and leak checks. Synthetic **loaded PEAC/PEHB** fixtures test:

- Exact body/knife clock compatibility, v1/v2 timing mismatch and missing/wrong capsule rejection.
- Atomic gate failures for movement, ungrounded state, FP and undrawn knife, with no stamina charge.
- Actual time-varying capsule poses matching the action/render clock, closed-window suppression, one hit per stage and the four-stage chain ending at80 stamina from100.
- Separated-target misses, denied eligibility, exact numeric HP changes, a real positive-to-zero transition emitted once, and later swings that do not kill a dead target again.
- Low-stamina source acceptance, active movement/FP cancellation, explicit cancel, nonfinite target clocks and invalid clip atomicity.

When the actual recovered assets listed above are present, an additional headless fixture loads them, verifies the full four-clip pairing, chains all four stages against a deliberately distant hare, observes changing recovered blade geometry, and confirms zero hits/unchanged target HP and total20 stamina spent. This validates asset/clock integration without inventing a reference-game hit or tuning a target into the blade. Graphical/manual hit and kill evidence belongs in the separate root-controlled playtest record; this module audit does not claim it has happened.
