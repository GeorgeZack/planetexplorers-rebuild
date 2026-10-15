# Hare skill guide clock

October5,2026. **Source-derived and implemented in C:** [`pe_hare_skill`](../../rebuild/src/pe_hare_skill.h) schedules the five ordinary hare melee skills independently of Animator capsule windows. It does not claim measured Unity coroutine/frame ordering. The attempted combat observer has not yet established a valid live timing trace; see [combat observation](ADVENTURE_HUNT_COMBAT_OBSERVER.md).

## Recovered waits

[`export_hare_skill.py`](../../rebuild/tools/export_hare_skill.py) reads the original database through a read-only SQLite connection, exports source `Single` values without rounding them to convenient frame counts, and pins database plus scheduler-source hashes in [`pe_hare_skill_source.json`](../../rebuild/generated/pe_hare_skill_source.json). Generated files must be regenerated with the exporter.

| Attack / skill | Guide pre / main / post seconds | Ending post seconds |
| --- | --- | --- |
|0 /30100003|0 /1 /0|0|
|1 /30100004|0 /1 /0|0|
|2 /30100008|.36 /.15 /0, then .24 /.24 /0|.51|
|3 /30100046|0 /1 /0|0|
|4 /30100047|0 /1.0666 /0|0|

All prepare waits and ending-pre waits are zero. The table prints source decimal spellings; the generated data preserves their actual binary32 values. Attack2 uses `loopcnt,2`; the others use `False`. Array access repeats the last available authored entry, matching the supported source arrays. The exporter fails on unsupported loops, effects, triggers or invalid waits rather than silently accepting a changed contract.

[`SkInst.Exec`](../../Assets/Scripts/SkillSystem/SkInst.cs) registers each guide, advances the guide counter, resets its three skip flags and waits through pre. It then clears the per-guide hit list and enables external collision execution for main. It disables collision execution before post, unregisters the guide and tests the loop condition. Ending waits follow the final guide. [`WaitTimeSkippable`](../../Assets/Scripts/SkillSystem/SkUtils.cs) continues while elapsed time is strictly less than its duration and its skip flag is false. Prepare and ending use ordinary waits, not the skippable guide waits.

## C contract and explicit timing adapter

Initialize, begin with an attack index0..4, and call `pe_hare_skill_step` exactly once for each scaled60Hz simulation tick. The snapshot exposes elapsed seconds, phase-local seconds, zero-based guide index, phase, active/completed and `contact_enabled`. It contains no animation clock and does not change HP, cooldowns or quest state. Invalid begins/steps preserve the entire caller object.

Positive waits consume complete fixed ticks; when a phase changes, its successor starts at zero and prior overshoot is discarded. Zero-duration phases advance immediately. This is an explicit deterministic replacement for Unity's frame-resumed coroutines. In this adapter the two attack2 guides consume22 pre ticks,10 main ticks,15 pre ticks and15 main ticks, followed by31 ending ticks. Source `.15f` is slightly greater than nine double-precision60Hz ticks. These93 ticks are **C behavior**, not an original measured attack duration.

The host must require both `contact_enabled` and an open authoritative Animator capsule window before spatial contact can damage a target. Use the current skill guide index for per-entity deduplication; a second Animator window does not independently create a second guide. Source Attack2 has both two guides and two animation windows, whose clocks and boundaries remain separate. Both forearm capsules share the current guide's hit list.

Explicit pre/main/post skip requests latch for the current guide and reset when a new guide starts. Ending waits cannot be skipped this way. The source `BTMelee` action does not issue Sword/Gun skip flags, so its host calls step with0. `MonsterEndAttack` is an Animator signal, not a skill skip request. Skill completion does not implicitly terminate Animator playback or reset `pe_hare_attack` cooldowns. Source `BTMelee.Tick` remains running while either the skill or `entity.IsAttacking` is true; the host must account for both before releasing movement. Actual interruption cancels the guide clock as well as the action; cancellation immediately disables contact.

## Admission boundaries outside this module

[`BTMelee`](../../Assets/Scripts/Behave/Action/BTAttack.cs) requires its selected attack branch, ready skill, authored bounds-gap0..0.5, angle±10, obstruction check and **self** HP fraction in inclusive[0,1]. The hare XML leaves those HP defaults unchanged. Its `Inside` angle bypass also depends on overlapping bounds and center XZ distance<.5. General `pe_hare_engagement` currently uses its explicit facing gate; it does not reproduce this bypass.

The source target path adds active/view/alive target checks, positive hatred, no ignore flag, follow availability, recent ability to attack and no vehicle target. Negative initial threat yields zero hatred and follows a separate escape path; permission to collect an enemy is not by itself permission to start melee. `TargetCmpt.CanAttack` also checks attack enablement, serious injury, daytime restrictions and escape. Land compatibility uses the source water/air classification, not a universal equivalence to grounded status. The source registers up to three melee attackers on the target and can place additional attackers in the Threat group. Weighted pounce-versus-melee selection, that coordination, general hatred/follow state and water/air classification are outside the guide scheduler. A bounded controller must identify any substitutes explicitly.

Within a selected ordinary melee branch, runnable skills are selected uniformly, while the whole branch's weight is.5. [`pe_hare_attack`](../../rebuild/src/pe_hare_attack.h) owns the strict behavior cooldown, individual/shared deadlines, evaluated Animator windows and per-guide contact deduplication. The guide scheduler only provides the source skill phase gate; it must not silently replace these checks or turn sensor detection into damage.

The bounded controller currently starts its transient clock at zero on preparation, imposing a first strict one-second readiness wait; the source uses the global `Time.time` epoch. Modal/cursor ownership suspends incoming creature clocks and contact in the frontend while ordinary player gravity/metabolism may continue. Both are explicit host adapters. Pounce remains a separate unimplemented action; observing original pounce does not establish its timing or justify substituting a stationary melee strike.

## Verification

The separate [encounter helper](../../rebuild/src/pe_hunt_encounter.h) focused optimized and ASan/UBSan/leak checks also passed, including all six pinned positions supported on the actual route, exact source XZ preservation, staged rollback, real blade contact deaths, external prototype13 death dispatch, duplicate rejection, four-kill cleanup and partial-progress fresh-cohort reload. These are headless tests, not a physical six-hare combat playtest or a claim of complete AI parity.

The subsequent [`pe_hunt_combat`](../../rebuild/src/pe_hunt_combat.h) integration tests also pass strict optimized and ASan/UBSan with leak detection, using the actual route, authored hare/knife capsules and installed19-clip player defence actor. They verify vision from the animated source head, hearing/collection cadence, an intervening block ray, real incoming forearm contacts, a later lethal forearm hit with immediate clock cancellation, atomic rollback after a malformed blade frame, and four real outgoing blade-contact deaths with owned-cohort cleanup. Low-current-HP/coincident-target arrangements exist only in explicit tests. The controller accepts the exact non-melee player pose and replaces it with the newly advanced blade pose for incoming contact whenever melee is active, matching the snapshot subsequently rendered by the host. This fixes the possible one-tick discrepancy from testing incoming damage against the previous blade snapshot. No original live frame timing or native physical gameplay result is inferred from these tests.

Subsequent optimized regressions also cover authoritative zero-HP cancellation despite a stale alive input gate, and rejection of an otherwise valid actor/PEHB pair missing a supported tool defence clip. Readiness now requires all19 locomotion, knife, shovel, axe and pickaxe poses; older ten-clip defence groups fail preparation instead of causing a later runtime error when a tool is used. The final readiness/life-gate additions have their own optimized coverage; the earlier focused sanitizer result predates these small additions.
