# Adventure Revival Shot lifecycle

October6, 2026. Source-derived rules and implemented C behavior, tested without running either game. This follows [the source-first review](SOURCE_FIRST_REBUILD.md). No new original observation, native graphical check or ordinary hunt completion is claimed.

## Source contract

[export_revival.py](../../rebuild/tools/export_revival.py) guards the actual database rows and active C# consumers, then produces [metadata and input hashes](../../rebuild/generated/pe_revival_source.json) and [C constants](../../rebuild/generated/pe_revival_source.h). The chain is `PrototypeItem937 → Consume/Property → skMain20150031 → skTriggerEvent53,59`, in that order. The effects subtract25 raw Comfort and assign HP to summed HPMax×`.333f`. Skill `_cond=False` ends looping; it does not reject the first execution. There is no private/shared cooldown. The descriptive skill name is not authority for full healing.

[UIRevive](../../Assets/Scripts/Chenzhi/NewUI/Scripts/PE_GameUI/NGUIScripts/UIRevive.cs) first starts the Revive action, then calls the dedicated [UseItemCmpt.Revive](../../Assets/Scripts/PeEntity/Cmpt/UseItemCmpt.cs) item path. Ordinary inventory/quickslot requests reject937 and dead users. Source action relation11 requires Death, excludes Fall/Ride, pauses Flashlight and immediately ends Death. [Action_Revive](../../Assets/Scripts/PeEntity/Cmpt/Motion/PEAction/Action_Live.cs) checks get-up readiness, floors HP to1, restores stamina/oxygen, freezes physics and dispatches State_Revive before deactivating ragdoll and defence. Its update finishes once ragdoll is absent, unfreezes physics and schedules defence reactivation1.5 seconds later.

State_Revive remounts initial buffs through [SkAliveEntity](../../Assets/Scripts/SkillSystem/SkAliveEntity.cs). The existing hurt/consumption timestamps and entity initialization timer survive. Buff evaluation occurs on the existing global eight-display-frame attribute gate; revival must not restart the load delay or synchronously apply another recovery pulse.

[BiologyViewCmpt.ActivateInjured](../../Assets/Scripts/PeEntity/Cmpt/BiologyViewCmpt.cs) stores the requested state immediately, but the defence trigger itself stays disabled until the delayed callback. Disabling it cancels the pending callback. Contact availability must therefore be distinct from being alive or from the target injured flag. This is collider grace, not immunity to every possible future damage system.

## Native ownership and host integration

[pe_revival](../../rebuild/src/pe_revival.h) owns the transient upright get-up/grace state. `pe_game_revive` stages only inventory, supported vitals and recovery/action state; it never shallow-copies the owning world. Admission requires Adventure, zero HP, the source relation/readiness gates, an available shot and both recovery owners updated at the supplied display frame. Invalid gates/state or missing inventory leave the game unchanged.

Success removes exactly one937, sets HP to33.3% maximum and stamina to maximum, charges Comfort25 and clears old movement/tool tails. It preserves location, world, equipment identities/wear, quickslots, learned recipes, consumable cooldowns and earned hunt progress. The ten starting shots are now usable through this dedicated flow. Ordinary quickslot use remains rejected.

The [sandbox UI](../../rebuild/src/pe_sandbox_ui_input.c) opens a death modal with `1. Revive (1 shot)` and `2. Load saved game`; clicking the corresponding buttons works too. Escape opens the existing pause menu. No shot leaves revival disabled. UI routing emits a request; the host commits it after updating the authoritative display clock. Save/load requests take precedence if supplied in the same frame. Death cancels open dialogue, town panels and drags. Closing input retains pointer ownership through held/release frames and cannot become a world attack or camera capture.

The [host](../../rebuild/src/pe_sandbox_view.c) does not rebuild the hunt on revival: surviving creatures retain their actual HP, locations, identities and encounter clock. It cancels stale player melee/tool/input, resets locomotion presentation and uses the ordinary sensor path to reacquire threats. The get-up modal owns player input while permitting hunt sensor/attack clocks to continue. Actual [incoming contact](../../rebuild/src/pe_hunt_combat.c) is suppressed during get-up/grace without committing damage, wear, hit notifications or guide deduplication. Attack histories continue updating; a later eligible contact in the same guide may hit after defence re-enables.

Player physics is frozen during death/get-up in the shared game step. Vitals/crafting clocks are separate; this is not a global simulation pause. Native pause/focus policy freezes the revival timer alongside the existing scaled recovery clock. Fixed catch-up ticks cannot advance get-up or grace.

## Recovery, save behavior and explicit adapters

The initial buff owners remount at the current display timestamp, preserving creation/init flags, hurt and consumption timestamps, previous summed stamina delay and the global frame gate. Pulse indices reset. Raw Comfort can briefly become negative from937; [the stamina owner](../../rebuild/src/pe_stamina_recovery.c) stages that raw value until the next attribute pass, calculates derived delay before clamping, and then consumes the pending value. Repeated deaths before that pass accumulate the raw cost instead of losing it through early clamping.

PECSv16 bytes remain unchanged. HP, stamina, clamped Comfort, inventory and mission progress persist through their existing fields. Get-up, collider grace and pending raw Comfort are transient. Loading a living save reconstructs ordinary defence and the existing delayed recovery initialization; it cannot consume another shot. A dead save remains dead. Failed load commits nothing. Terrain import resets get-up/grace with the other transient movement state.

Limitations are explicit:

- The native actor still has upright death and no ragdoll/get-up animation. The host follows the source model-present/no-ragdoll branch, with get-up ending on the next active display frame. Actual Unity component update order need not be exactly one rendered frame.
- Native preflight commits action/item effects atomically. Source UI starts the action first and ignores item-use failure; the native retry behavior is an adapter, not recovered source rollback. Unmodeled Oxygen is not fabricated.
- The1.5-second deadline uses the shared native float display timestamp; this is not a claim of exact Unity Invoke scheduling.
- V16 retains clamped saved Comfort. Saving before the pending raw negative value is evaluated drops that narrow intermediate value on reload; the source can serialize raw attributes. No undocumented payload/schema change is introduced.
- The death panel reuses recovered general UI assets in a native layout. Exact ReviveGui geometry, no-item town-return travel, corpse markers, mission cancellation associated with town return and follower revival remain unported.

## Verification

[Core regression](../../rebuild/tests/test_revival.c) covers actual starter inventory, shot debit, vitals, all admission gates, malformed-state rollback, repeat requests, source remount ordering, negative Comfort, repeated death, exact grace boundaries, v16 persistence and failed-load atomicity. Optimized and ASan/UBSan runs with leak detection pass without skips. Its two-kill save fixture explicitly stages the quest counter; it does not earn two kills through play.

[Host regression](../../rebuild/tests/test_sandbox_revival_host.c) compiles the actual production helpers and UI router with graphics discarded. It tests keyboard/click requests, movement/camera/pointer ownership, phase advancement, pause/focus, save/load priority and dead-save revival. Optimized and ASan/UBSan/leak checks pass without skips.

[Hunt regression](../../rebuild/tests/test_hunt_combat.c) uses recovered body/hare capsules to compare the same incoming contact with defence enabled/disabled. Protected ticks preserve HP, wear and guide eligibility while advancing AI/skill/history clocks; re-enabling permits a later same-guide hit. Its optimized run passes with the assets present. Arranged headless geometry is not ordinary native combat evidence.

The12 exporter tests pass with zero skips, including source/data mutation guards; `export_revival.py --check` passes. Full integration results are recorded in [PLAYTEST](../PLAYTEST.md). The remaining graphical/native acceptance check is death→revive→ordinary combat, then an actual partial-kill save/reload and four kills. The original-game replay is unnecessary for discovering the rules above.
