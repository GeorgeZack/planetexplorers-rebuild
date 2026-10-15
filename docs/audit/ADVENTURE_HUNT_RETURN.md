# Bug Hunt return9127

October6,2026. **Source-derived and implemented in C:** strict return discovery, four reward scripts, two durable dialogue pages and a terminal boundary before Town Defense9077. The earlier original [manual slot7 archive](../reference/hunt-original-return.json) pins active9127 and its destination/reward/dialogue definitions. The later original physical return shows completion and a Jet Pack Script toast while the first ending reply is displayed, before its acknowledgment. The bounded probes ended before that arrival, so exact inventory counts and frame ordering remain unobserved. Native four-kill completion and partial-progress reload also remain distinct from automated tests.

## Original contract

`AdRandomQuest_List9127` has target4525, `AutoReply=1`, no demanded/removed items, reward592/593/594/595×1, `TalkED=4105,4106`, `AdvPlot=3_4` and `NextQuest=9077`. `AdQuest_EnterArea4525` encodes `DistPos=3_3_0_5` and `NotForDungeon=1`. The active original archive binds Naoko20018/role3 and an initialized destination at source(-266,25,136). This fixed discovery anchor is distinct from the NPC's animated or later moved position.

[`MissionManager.UpdateSearchMission`](../../Assets/Scripts/Mission/MissionScript/MissionManager.cs) uses strict XZ distance below5 for this random Adventure discovery, nonnegative original-world playerY and matching scene. The target does not require another interaction button. [`PlayerMission.CompleteTarget`](../../Assets/Scripts/Mission/PlayerMission.cs) automatically invokes completion. `CompleteMission` removes the mission, records completion, attempts item grants, pushes completion story and finally queues ending dialogue. Rewards therefore precede4105, rather than waiting for its final acknowledgment. The intentional native medicine final4100 acknowledgment policy does not transfer to this separate quest.

The four rewards are **scripts**, not four functional movement devices:

| Item | Script | Source learned formula IDs |
| --- | --- | --- |
|592|Jet Pack|187,42,41,51|
|593|Parachute|188|
|594|Glider|189|
|595|Rope Gun|190|

All have stack limit1 and neutral sex eligibility. Exporting the reward closure adds items592/594 and recipe output72, retained within the current341-item/108-formula closure. Granting a script does not automatically learn it. Existing inventory Use performs the source formula-learning transaction; this increment does not implement jetpack/glider/parachute/rope-gun movement.

`RewardSP=2` does not mean unconditional two-point currency. The source completion branch mounts its five-point buff only when Adventure skill trees are enabled. This native increment has no skill-tree implementation or invented substitute reward.

`AdvPlot=3_4` refers to completion story4. [`StoryRepository`](../../Assets/Scripts/Mission/StoryRepository.cs) parses its `NPCMove=4_1_0_5` as role4 moving toward the player's current position within radius5; [`StroyManager.UpdateAdvPlot`](../../Assets/Scripts/Mission/StroyManager.cs) requests Run and updates that NPC's fixed point. This story row does not itself create a defense wave. That movement request is not implemented here.

[`UINPCTalk`](../../Assets/Scripts/Chenzhi/NewUI/Scripts/PE_GameUI/NGUIScripts/UINPCTalk.cs) attaches a mission trigger only to the last ending page. Closing4106 invokes the completion-type branch of `SetGetTakeMission`; it skips a second completion if9127 is already recorded, then prepares9077's opening4107–4110. Native state6 intentionally stops before that next quest; it neither shows an unsupported defense briefing nor starts a fabricated wave.

## Original physical return, October6

The operator loaded preserved manual slot7/save6 and approached through ordinary movement. At displayed map position `P:-265,134`, the [arrival screenshot](../reference/hunt-return-original-arrived.png) shows **Mission Completed: Bug Hunt**, a **Jet Pack Script ×1** acquisition toast and the player's4105 reply, before acknowledgment. One ordinary acknowledgment revealed [Naoko's4106 reply](../reference/hunt-return-original-4106.png); the next revealed [Sajiva's incoming-monsters opening](../reference/hunt-return-original-next.png), consistent with the source9077 follow-up. These observations establish visible completion/reward notification before the first ending-page acknowledgment and the displayed reply sequence. They do not establish exact player XYZ, the strict distance boundary, all four inventory counts, or subframe ordering.

Both observation captures ended at their120-second limits before the final approach:2528 and2746 frames respectively. The [bounded negative reports](../reference/hunt-return-negative-observations.json) show active9127, unchanged zero592–595 player counts, no completion transition and no visible4105/4106 within either recording. Neither satisfies `--require-return`; the later physical screenshots must not be described as captured field transitions. See the [observer audit](ADVENTURE_HUNT_RETURN_OBSERVER.md). The original was closed through Quit without a new manual save. No post-completion archive was produced by this run, and the preserved active-return checkpoint remains the source fixture.

## Native transaction and persistence

[`export_adventure_hunt.py`](../../rebuild/tools/export_adventure_hunt.py) checks the source rows and original return capture, preserving database/capture/archive hashes in the generated manifest. Missing return evidence disables its placeholder destination. Changing a target, Naoko binding, completion prerequisite, reward list or ending chain causes export validation failure.

[`pe_adventure_hunt_return_step`](../../rebuild/src/pe_adventure_hunt.c) requires Adventure, living player, valid completed medicine/hunt prerequisites, four real saved kill credits, state3, supported route and the strict source discovery rule. It stages all four rewards in a temporary inventory and commits the inventory plus state4 together. A failed inventory addition preserves inventory identity counters and progression for retry. Calling it again after completion cannot grant twice. It does not alter money, RNG or learned formulas.

**Capacity retry is an explicit native adapter.** The inspected original completion path comments out its package-capacity check and ignores individual `AddToPkg` failures after recording completion; [`PlayerPackageCmpt.Add`](../../Assets/Scripts/PeEntity/Cmpt/PlayerPackageCmpt.cs) returns false when an item will not fit. The C rebuild deliberately retains all-or-none, retryable grants rather than reproducing potential lost rewards. No original full-bag outcome has been physically established.

The existing hunt-state field carries durable presentation progress:

| State | Meaning | Kill count |
| --- | --- | --- |
|0|Unaccepted|0|
|1|Travel9075|0|
|2|Active9126 encounter|0–3|
|3|Return9127 discovery pending|4|
|4|Rewards already granted;4105 pending|4|
|5|Rewards already granted;4106 pending|4|
|6|Reported;9077 unavailable|4|

`pe_adventure_hunt_return_talk` exposes the pending source talk ID. `pe_adventure_hunt_return_advance` requires the expected ID and changes4→5→6 without giving items. Reopening, canceling and loading must resume the saved page. A stale4105 acknowledgment cannot skip4106, and a repeated4106 cannot grant or start another mission. Durable dialogue resumption is native presentation policy, not a claim that the original serializes its talk queue the same way.

PECSv12 changes the supported state values while preserving **every byte and length of the v11 payload**; it appends no data. Current v13 retains that payload and adds a separate four-byte medpack cooldown. Pre-v12 saves reject state4–6 even if their checksum is valid. V11 state0–3 retains its previous meaning, and versions1–10 keep their previous default/migration behavior. Actor HP, threat and clocks remain transient. The completed hunt state does not imply that reward scripts remain in the inventory: the player can consume or discard them afterward without corrupting progression.

## Verification and physical test sequence

For a future instrumented return observation that fills the remaining inventory/timing gap, load preserved manual slot7/save6 in the isolated hunt profile. Record inventory592–595 and active9127 before approaching the archived destination, then approach through ordinary movement. Record the transition into4105 and inventory change before acknowledging it, the4105→4106 transition, any role4 movement, and9077's subsequent opening order. Save a new slot rather than overwrite the active-return checkpoint. Compare archive completion/reward/active-quest records with the screenshots. A minimal observation component can read these values but must not advance dialogue, set quest state or grant items.

For native combat verification, reuse an unchanged ordinary equipped-Knife33/full-health checkpoint in a distinct save path. Starter Herbal Juice916×10 and medicine-reward Portable Medpack925×10 are supported through normal inventory/quickslot use. Medpack heals350 immediately with its own10-second cooldown, independent of juice; see [consumables](SANDBOX_CONSUMABLES.md). Juice can also be purchased from the supported20011/store338 or20028/store1406 vendors. Do not reduce hare HP, modify the kill counter or create a combat-completion save by changing fields.

The current noncritical attack19 gives approximately6.43–8.43 body damage against the six captured defenses, with source head multiplier2. Full-health targets need roughly21–43 body contacts or11–22 head contacts. These arithmetic bounds do not guarantee that all swings touch a particular capsule. [Bounded grounded running attacks](SANDBOX_RUNNING_KNIFE.md) are implemented; missing stagger and broader combat behavior still make directly tanking several attackers difficult. Ordinary retreat, healing and positioning remain necessary. Save after a real first or second credited death, reload through the menu, verify the count survives and a fresh pinned full-HP cohort appears, then continue to four actual deaths. Record cleanup and state3 separately from the return transaction. No save fixture with altered creature HP was prepared.

## Full-HP combat-to-return regression, October6

[`test_hunt_lifecycle.c`](../../rebuild/tests/test_hunt_lifecycle.c), registered in the core test suite, now connects actual full-HP knife-resolver deaths to earned1/4 and3/4 save/reload checkpoints, fresh pinned cohorts and fourth-kill cleanup before exercising this return transaction. It does not start by assigning state3/kills4. Each exercised small hare required11 real guide hits; captured HP/defence remain unchanged. The fourth death supplies return state3 through the same completion result consumed by the frontend.

After saving/reloading that return state, the fixture checks that the distant hunt position grants nothing, explicitly relocates and settles at the archived return anchor, and obtains exactly one each of592–595. It saves/reloads4105,4106 and completed state, preserving inventory/instance identity and checking that repeated arrival or stale acknowledgments cannot grant again. This extends the isolated return tests with a complete transaction chain; it does not replace their exact distance, capacity and old-version rejection cases.

The contact player poses are deliberately arranged against recovered hare capsules, AI/retaliation is not stepped, and anchor relocation is explicit. The player stays at500HP and retains all ten medpacks. Thus this is headless persistence/damage coverage, **not physical four-kill completion, a survival test or evidence of original nonzero-kill reload behavior**. Full optimized core and the focused lifecycle ASan/UBSan run with leak detection both exited0 without diagnostics or fixture skips. Exact final local logs, registration and child leak-check details are recorded in the [combat audit](ADVENTURE_HUNT_COMBAT.md#full-hp-lifecycle-regression-october6).

## Native physical return segment, October6

A separately staged state3/kills4 fixture supplied only the return precondition; it had zero592–595 items. Ordinary native movement then entered the strict radius and automatically displayed4105. Cancel/save/load resumed4105, then4106 after its acknowledgment, and retained final completion after another save/load. [Native-loader evidence](../reference/hunt-return-native-inspection.json) confirms each of the three checkpoints has exactly one of every reward, states4/5/6 and talks4105/4106/0, with no duplicate grants and unchanged money/learned recipes. [4105 reload](../reference/hunt-return-native-4105-reloaded.png), [4106 reload](../reference/hunt-return-native-4106-reloaded.png) and [completion](../reference/hunt-return-native-complete.png) record the displayed flow. The C process exited0 through its menu. This verifies the return UI/persistence segment; it does not establish a physical four-kill playthrough or partial-combat reload. Full optimized/content/CLI and sanitizer checks pass; strengthened transaction/reload tests also pass separately. See [PLAYTEST](../PLAYTEST.md) for exact commands, optional skips and sanitizer limits.
