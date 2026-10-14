# Qualified camp repair and native service

October7,2026, US/Central. **Source-derived:** the existing starting-town machine, its camp binding, item transfer/material rules, timer and archive ownership. **Decoded archived evidence:** two empty repair jobs at the same machine. **Implemented in C:** the qualified machine now connects the exact-instance owner, modal service UI, sandbox interaction/display clock and PECSv20 persistence. No original game, Unity scene, desktop input or new gameplay recording was used for this increment. Source/headless checks do not establish visual acceptance.

[export_camp_repair.py](../../rebuild/tools/export_camp_repair.py) produces [the C contract](../../rebuild/generated/pe_camp_repair_source.h) and [complete metadata](../../rebuild/generated/pe_camp_repair_source.json). UnityPy reads serialized source and installed assets offline; no Unity dependency is added to the C runtime. This increment follows [the source progression audit](SOURCE_AUDIT_PROGRESSION_FOLLOWUP.md), replacing its unresolved machine-placement gate with existing qualified evidence.

## One real machine, already in the route

The archived starting lot contains entity20006, `EntityDoodad`, prototype145, using `Item/RepairMachine_Static.unity3d`, at source **(-613.5,29.5,404)** with180° heading. It lies inside the installed416×64×416 route. Its model, textures and collision already occur in the existing route manifest/geometry; no invented machine placement or new terrain is needed. The source and installed serialized root both contain enabled `OperatableItemRepairMachine` and `CSRepairObject`, with object type5, `m_ItemID=0`, power field0 and no work transforms. Its source box collider is retained in the metadata. These findings do not qualify animated machine presentation; the installed route uses the previously documented static rest pose.

[OperatableItem.OnStart](../../Assets/Scripts/Items/OperatableItem/OperatableItem.cs) resolves the parent `PeEntity` and passes its actual ID into `Init`. [OperatableItemRepairMachine.Init](../../Assets/Scripts/Items/OperatableItem/OperatableItemRepairMachine.cs) binds creator `CSConst.ciDefNoMgCamp=10000`, immediately or through its deferred PostInit callback. [CSEntityObject.Init](<../../Assets/Scripts/WuYiqiu/Colony System/Scripts/EntityObjects/CSEntityObject.cs>) passes that instance ID and serialized object type to the creator. [CSNoMgCreator.CreateEntity](<../../Assets/Scripts/WuYiqiu/Colony System/Scripts/CSNoMgCreator.cs>) selects `CSRepair`, assigns `CSInfoMgr.m_RepairInfo`, and creates/restores the record under the same ID.

Do not confuse three identifiers: **145 is the scene doodad prototype, 20006 is this machine's entity/job key, and1130 is the ordinary Repair Machine item/colony-info prototype.** The static component's item-ID field is0, as is its archived colony `ItemID`. The ordinary source `RepairMachine.prefab` independently binds `CSRepairObject` type5 to item1130. Source `ColonyIDInfo.COLONY_REPAIR` links the repair prototype to `CSInfoMgr.GetInfo/SplitInfo/LoadRepairInfo`; the `colonyinfo` row1130 supplies `_property=60`. Its `repairtime=300` is the machine's own maintenance timer, not the inserted equipment's repair duration.

## Archived job and lifecycle binding

The existing sandbox-reference save0 and route-reference save2 both contain a complete326-byte `MainAdventure/ArchiveKeyCSDataMgr` record, version16102100. The bounded decoder consumes every byte, including the separate managed creator and empty farming records; unknown schemas/owners and nonempty jobs are rejected instead of silently skipped.

| Field | Starting save0 | Route save2 |
|---|---|---|
|Creator ID/type|10000 / NoManaged(1)|10000 / NoManaged(1)|
|Repair data type/entity|5 /20006|5 /20006|
|Stored ItemID|0|0|
|Inserted item instance|−1|−1|
|Current/final job seconds|0 /−1|0 /−1|
|Machine durability|3500|3500|
|Colony `m_Alive` flag|false|true|

The position matches the independently decoded doodad transform. The two `m_Alive` values are recorded literally: the earlier false flag does not establish that the machine is destroyed or unusable. The source creator distinguishes record restoration and later object binding; its existing-instance rebind sets `BaseData.m_Alive=true`. This qualification does not import that scene lifecycle into C or override an actual destroyed machine.

[CSDataInst.Export/_readRepairData](<../../Assets/Scripts/WuYiqiu/Colony System/Scripts/CSDataInst.cs>) records the repair item's instance ID, current counter and final counter after the common machine fields. [ItemMgr.Export/Import](../../Assets/Scripts/Items/ItemManager.cs) separately preserves the actual item objects. The job record therefore references an item owner; it is not a duplicate repaired item or merely a prototype ID. [CSRepair.CreateData](<../../Assets/Scripts/WuYiqiu/Colony System/Scripts/Entities/CSRepair.cs>) resolves that ID through ItemMgr and calls `StartCounter(current,final)`; a negative final time does not start a counter.

Both available jobs are empty. Source establishes the restart path, but **no original partial-repair save/reload has been observed**, and no Unity archive importer is implemented. Missing stored objects, scene streaming and exact ordering between separate MonoBehaviour updates are not resolved by these snapshots. A native single-owner job with transactional load validation can be implemented without pretending those observations exist.

## Active camp UI, not the generic repair helper

[UIRepairWnd.OnDropItem](../../Assets/Scripts/Chenzhi/NewUI/Scripts/PE_GameUI/NGUIScripts/UIRepairWnd.cs) rejects a busy machine, hotkey-origin/self-origin drops, creation items, a missing Repair component, and a null/empty `repairMaterialList`. It transfers the **same item object** from the former owner. [SelectItem_N.RemoveOriginItem](../../Assets/Scripts/Chenzhi/NewUI/Scripts/PE_GameUI/Widget/SelectItem_N.cs) clears a bag slot or requests equipment removal; source equipment-origin insertion is possible. A native bag-only slice must explicitly label its stricter admission rather than presenting it as source prohibition.

An idle occupied machine accepts same-category replacement by exchanging exact instances through the incoming grid. For different categories, the UI first checks that the previous item can return to the player package, returns it, and then inserts the new item. Right-click return uses `CanAdd` and leaves the machine item intact when the bag is full. Insertion, replacement and return are disabled during a running job. Completion does not automatically return the item.

For each base repair material, the active UI independently computes:

```text
required = ceil(base_count * 1 * (1 - current_durability / maximum_durability))
```

All material counts are checked before the debit loop. `Repair()` also rejects `PeFloatRangeNum.IsCurrentMax()`: source Linux uses an absolute deficit below `PEMath.Epsilon` (approximately2^-23) as full. This is distinct from a cost-factor epsilon. The generic `Repair.GetRequirements/AddRequirements` helper has an additional small-factor early return; **the active camp UI does not call that helper**. Do not apply its epsilon to make tiny nonzero damage free.

The source UI debits materials, starts the counter, and retains the same item. Reset calls `StopCounter`: it destroys the timer, resets saved current/final to0/−1, retains the item and **does not refund materials**. Starting again pays a new quote. `OnClose` clears the UI binding only; it neither cancels the job nor returns the item. Distance strictly greater than8m closes the window. The old unused `SqrOperateMaxDistance=6*6` constant is not an established active6m admission check: actual picking passes through MousePickable's collider/range path. Interaction geometry must be qualified at host integration.

At completion, `CSRepair.OnRepairItemEnd` invokes an optional UI callback and then `Repairing→Repair.Do`. The UI callback may also call `Do`, an idempotent set-to-maximum operation. There is no second item or reward. `Repair.Do` can restore both LifeLimit and Durability; the bounded native contract below supports ordinary Durability only.

## Time and catalog closure

`CSRepair.CountFinalTime` uses `baseTime * pow(.82,workerCount) * product(1-workerRepairSkill)`. The qualified nonmanaged camp has no assigned personnel/work transforms, so its unmodified duration is **60 seconds**, regardless of how much durability is missing. `StartCounter` explicitly calls `CounterToRunning`; implementing an entire staffed player colony is unnecessary for this bounded camp path.

[CounterScript.Update](../../Assets/Scripts/WuYiqiu/Colony/CounterScript.cs) uses an independent [UTimer](../../Assets/Scripts/Global/UTimer.cs), `ElapseSpeed=-1`, and `Time.deltaTime`. Each enabled display update adds the truncation-toward-zero integer of the source float expression `-1 * dt *100000` to remaining ticks. It completes at remaining ticks≤0 and destroys the one-shot counter. The accelerated world calendar is not used. `CSRepair.Update` enables/disables the counter according to the machine's running state and records current/final time. An enabled job may advance while its UI is closed; no catch-up for real time spent outside the application is established.

At the341-item catalog checkpoint,71 items have Repair components:

| Classification | Count | Scope |
|---|---:|---|
|Ordinary Durability, nonempty materials, complete native material closure|63|Generated source profiles; raw/unstrengthened instances only|
|No material list|3|Knife33, RopeGun67, SafetyAxe1527: source camp UI rejects|
|Creation-associated parts|3|1473/1474/1475: creation classification/effects remain outside this slice|
|LifeLimit towers|2|365/370: unmodeled condition owner;365 additionally needs absent material1111|

Every material needed by the63 ordinary profiles is already in the runtime catalog. No expansion is necessary. Examples: Wood Sword1, Wood Shovel12, Wood Pickaxe19, Wood Axe26 and Wood Shield34 each have a base requirement of Wood279×4; starter clothing95/131 uses Cloth1006×2 and shoes192 uses Cloth×1. Source definitions do not by themselves enable every item's equipment or action. In particular, a repaired item must not bypass the existing shield-presentation or unsupported-weapon gates.

## Native owner and playable integration

[pe_camp_repair](../../rebuild/src/pe_camp_repair.h) supplies the value/inventory owner; [PeGame](../../rebuild/src/pe_game.h) now holds its one detached exact `PeSlot`, running flag and remaining ticks. Its required read-only external-owner guard rejects instances still bound to equipment, shields, outfits, deployments or buyback. Bag membership, stable identity chronology and source item/material compatibility are independently validated. Successful insertion clears exact-instance quickslot references; taking the item does not recreate shortcuts.

Insertion, replacement, material debit and return stage their complete changes before commit; failed checks preserve both bag and machine. This stronger atomic policy is a native adapter. Bag-only insertion is another explicit boundary: source equipment-origin drops can unequip, whereas the native panel requires unequipping first. It cannot repair the unsupported Knife33 or bypass other item/action admission gates.

[Repair interaction](CAMP_REPAIR_INTERACTION.md) qualifies the existing machine's serialized cube and input/range call chain. The host uses the shared collision snapshot and present matching settlement model, selects the nearest valid NPC/Torch/herb/machine target and admits `E` or short stationary RMB interaction. The native panel opens only within inclusive8m root distance, the source close threshold; the original underlying picker has a broader19m collider range. This stricter opening adapter avoids opening a panel that must immediately close. Missing geometry/model, unsupported worlds, death/get-up or leaving range close the modal while retaining the item/job.

[The repair UI](../../rebuild/src/pe_sandbox_ui.h) shows six eligible bag items per page, current/maximum durability, the reserved item, material costs and remaining time. Choose a row and press Insert/Enter, or drag it onto the machine slot. Selection retains an exact instance/wear snapshot; changes invalidate the selection before the same input can be reinterpreted as another action. Listing uses a cheap shared advisory profile/ownership filter; every insertion and start rechecks the complete authoritative transaction. Paging never copies inventory or creates an item owner.

Repair opens a confirmation before charging materials; Cancel repair opens a separate confirmation explicitly stating there is no refund. Enter confirms and Escape backs out of a prompt. Take returns the idle/completed item, with full-category failure left retryable. The window close button or Escape outside confirmation retains the item and job. The title bar moves the panel; all rows, buttons and confirmation geometry share the same clamped scaling/hit layout. These controls and the confirmation layout are native presentation/usability adapters built with existing recovered UI assets, not a claim of exact source window reproduction.

The modal consumes inventory/quickslot/conversation shortcuts, save/load keys and its closing frame. Pointer ownership persists through held buttons and release; forced host closure uses the same discard/input gate. Focus loss cancels item/window dragging without transferring an item. Load replaces transient UI bindings after successful authoritative state restoration. The repair job itself is independent of window state.

The sandbox advances the job once per active display update, outside fixed-step catch-up. A repair modal does not stop its job. Closing the panel, player death or ordinary gameplay also leaves the clock active; focus loss, native pause and time outside the running sandbox host suspend it. A successful load skips the remainder of that display frame. These scheduling choices are explicit native lifecycle adapters; completion sets the same item to maximum exactly once and does not automatically return it. The machine continues to use the installed static model: no new repair animation is claimed.

### PECSv20 owner

[The save decoder](../../rebuild/src/pe_save.c) retains the literal v19 payload and appends32 bytes: machine entity20006 (or zero for empty), item prototype, count, instance ID, float durability, running word, and low/high remaining-tick words. Empty jobs encode all zeros; versions1–19 migrate to an empty machine. This owner is valid only in supported Adventure settlement worlds. Runtime and load validation reject bag/equipment/outfit/deployment/buyback/quickslot aliases, invalid profiles/material closure, malformed counts/wear, impossible timer/flag combinations and invalid instance chronology. Failed loads preserve the current game.

Save/load restores the owned item and remaining ticks without charging again, returning it, restarting a full minute or replaying a completed job. Clone/move retain the owner through the existing owning-object APIs. Replacing its world while the machine holds an item is rejected atomically. This native persistence is not a Unity colony-archive importer, and the original partial-job reload remains unobserved.

Worker colonies, arbitrary placed repair machines, creation repair, LifeLimit, source save importing and full machine animation remain outside this increment.

## Verification

The exporter guards complete source files, the source/installed serialized machine bindings, the read-only database and both exact archive profiles. It regenerates the63 C material profiles and retains full evidence for exclusions and empty jobs. Ten focused Python tests pass with zero skips in the existing isolated UnityPy interpreter. They cover exact regeneration; entity/type/creator/duration bindings; catalog and material closure; source/prefab/database/bundle drift; all326 possible archive truncations plus trailing bytes; corrupted owners/timers/flags; malformed materials; and missing-material rejection. No reference input was changed.

```sh
rebuild/build/unity-tools/bin/python rebuild/tools/export_camp_repair.py --check
rebuild/build/unity-tools/bin/python rebuild/tests/test_camp_repair_export.py
```

The original disconnected C-owner tests pass optimized and ASan/UBSan with leak detection. They cover all63 profile quotes, full/broken and malformed instances, required external-owner guards, atomic missing-cost rollback, busy insertion/return rejection, no-refund cancellation and paid restart, same-category replacement with a full bag, failed return capacity, and snapshot ID collision/malformed-state rejection. Clock tests include paused/subtick/invalid/overflow input,3600 display intervals of float1/60 leaving2400 ticks, and the final.024-second update producing exactly one completion edge. Source/UI's potentially repeated idempotent `Repair.Do` is represented by one equivalent native maximum-durability completion.

All63 current profiles are category1 and have one repair material. The generic cross-category and multi-material transaction branches therefore have no current source-profile fixture coverage; their implementation must not be described as exhaustive tested parity. No zero-maximum profile is emitted. These source/headless checks do not establish original partial-job replay, final UI rendering or new physical acceptance.

The focused [repair UI routes](../../rebuild/tests/test_sandbox_repair_ui.c) pass optimized and ASan/UBSan with leak detection headlessly. They exercise admission/modal shortcut isolation, paging through high bag indices, stale wear snapshots, focus-loss/drop behavior, explicit insertion, failed/confirmed starts, no-refund cancel/restart, full-category failed return and exact same-category replacement, completion while a cancel prompt is open, close/held/release ownership, and moved/resized hit geometry down to320×240. This is route/layout evidence, not a rendered screenshot or manual usability check. Integrated host/persistence verification is recorded separately in [PLAYTEST](../PLAYTEST.md).

## Ordinary native acceptance

**October 14, observed in the native production runtime with ordinary input;
no original-game comparison.** The question the source and headless proofs
could not answer was whether an ordinary player can reach machine 20006, open
its panel by interaction, and run the whole insert, quote, pay, wait, reload
and take loop through the rendered UI. Evidence: [manifest, input journal and
lossless crops](../reference/repair-native-20261014/checkpoints.json). Full
captures and saves stay under `rebuild/build/repair-private-20261014-01`.

*Setup.*
- Runtime `35124c7`, binary SHA256 `f5c07847…a8e`, private display `:116`.
- Baseline: the earned trade save `trade-private-20261014-01/reloaded.pesave`
  (SHA256 `21a80c12…1ac0`), unchanged afterwards.
- Starting state: Wood Shovel 12 as instance 10 in slot 425 at 3543 / 3553
  (worn by an earlier ordinary dig), Wood 279 ×194, money 261, Knife equipped.
- No fixture, cheat, teleport, construction, save edit or grant.

*Travel.* The player walked and sprinted from Xing Zhang (−292, 161) north and
west, using the traced route's middle legs in reverse, to below the lot at
(−618, 407).
- That save (`near-machine`) puts the player at height 22.95, while the machine
  root is at 29.5. The lot is a raised plateau with cliffs of about 6 m; the
  traced route's step from the lot to (−626, 429) is a one-way drop.
- *Candidate (analysis).* A read-only height grid from `pe_world_height` on
  that save (a scratch tool, not committed) suggested one ramp: z ≈ 399.5, from
  x −568 (23.3 m) to x −576 (28.1 m). It rises about 1 m per metre, an upward
  normal of about 0.707, which is above the native minimum walkable normal of
  0.65. The grid ignores static building collision, so it was a candidate
  only.
- *Observed.* The player walked round to that ramp and up onto the lot with
  ordinary input, then round the hotel to (−609, 401), about 5.5 m from the
  root.

*Observed, in order:*

| Step | Input | UI result | Saved state (inspector) |
|---|---|---|---|
| Open | free pointer, right-click the machine | REPAIR MACHINE panel; the bag list includes Wood Shovel ("36 / 36" display) | — |
| Insert | select Wood Shovel, Insert selected | "Item placed in the machine."; "Repair time: 60 seconds"; "Wood: 1 (bag 194)" | — |
| Confirm | Repair | "Start repair? Spend the listed materials … for 60 seconds? Cancelling will not refund materials." | — |
| Start | Repair (Enter) | "Repairing: 58.9 seconds"; "Materials paid Wood: 1 (bag 193)"; bag list locked | — |
| Partial save | close the panel; about 15 s later, Pause → Save | — | `partial`: running, 3,677,326 ticks left, instance 10 at 3543 in the machine, Wood 193, the shovel not in the bag |
| Advance | Resume, about 10 s, reopen | "Repairing: 25.4 seconds" | — |
| Load | close, Pause → Load, then Save while paused | — | `partial-reloaded`: byte-identical to `partial` (`f3e087ff…68bc`), identical inspection |
| Resume | Resume, reopen | "Repairing: 34.9 seconds" (later than the save, because the clock resumed) | — |
| Completion with the panel closed | close; walk about 14 m away; about 38 s; return; reopen | idle; "Required materials Wood: 0 (bag 193)"; Repair disabled; Take item enabled (UI only; completion rests on the two saves) | `finished-in-machine`: not running, 0 ticks, instance 10 at 3553 still in the machine |
| Take | Take item | "Item returned to your bag." | `completed`: instance 10 back in slot 425 at 3553; machine empty; Wood 193; money 261; quickslots unchanged |

*Against the source.*
- **Observed match:** the quote, ceil(4 × (1 − 3543/3553)) = ceil(0.011258) = 1
  Wood, is what the panel showed and what was charged. The panel also showed
  the source duration, 60 seconds. That is the source and panel value, not a
  measured runtime: the elapsed time was not measured exactly.
- **Lifecycle:** completion came while the panel was closed and the player was
  out of range. It restored the same instance to its maximum without returning
  it automatically, and Take returned it to its original bag slot.
- **Native adapters observed:** the Load restoring the saved tick count, and
  the clock running on after Resume. No original partial-job reload has been
  observed.
- **Timing:** wall-clock gaps and on-screen seconds are not measurements of the
  counter; only the saved tick counts are exact.

*Limits.*
- Cancel, replacement, a full bag on return, and death during a job were not
  exercised.
- The route found is an ordinary terrain ramp. Whether the original lot is
  reached the same way is unknown.
- The panel's "36 / 36" is the display's rounding; wear comes from the
  inspector only.

*Tooling.* The read-only inspector's schema 5 adds `repair` (occupied, item,
count, instance, durability, running, remaining ticks), with a formatter test
that includes a value above 2³² for output width. Real saved jobs are bounded
to 6,000,000 ticks.
