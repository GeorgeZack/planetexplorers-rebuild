# Qualified camp repair and disconnected native owner

October7,2026, US/Central. **Source-derived:** the existing starting-town machine, its camp binding, item transfer/material rules, timer and archive ownership. **Decoded archived evidence:** two empty repair jobs at the same machine. **Implemented in C:** a separate value/inventory owner, not an enabled town service. No original game, Unity scene, desktop input or new gameplay recording was used. PECS, playable UI and the sandbox host remain unchanged.

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

## Disconnected C implementation and next integration boundary

[pe_camp_repair](../../rebuild/src/pe_camp_repair.h) supplies a separate repair owner and ordinary-inventory transactions. It is deliberately absent from `PeGame`, PECS and the playable host. The owner holds one detached exact `PeSlot`, a running flag and remaining ticks. Its required read-only external-owner guard rejects instances still bound to equipment, outfits, deployments, buyback or another conflicting owner. Bag membership, stable identity chronology and source item/material compatibility are independently validated.

Insertion, replacement, material debit and return stage their complete changes before commit; failed checks preserve both bag and machine. This stronger atomic policy is a native adapter. Its initial bag-only admission is another explicit boundary; source equipment-origin drops can unequip. The value snapshot API restores already-owned job state without charging materials or replaying completion, into an empty destination after the corresponding inventory/other owners are restored. It is not an on-disk decoder.

The next bounded integration can expose **this one actual camp machine**, with source material quotes, a single reserved-instance slot,60-second job, Cancel and Take. Before enabling it, add:

1. A saved native authoritative owner, versioned migration to an empty job, duplicate-instance/quickslot/equipment/other-owner validation, and atomic failed-load behavior.
2. A real interaction against the existing machine geometry, with qualified source input/range rules, modal press-through-release ownership and UI state reflecting the actual job.
3. One enabled display-clock update independent of fixed-step catch-up; explicitly define native pause/focus/menu/load ordering and completion notification without freezing unrelated NPCs or player actions.
4. Integration tests for close/reopen, full bag, missing resources, stale selection, cancel/restart and save/load. Defer visual acceptance until desktop testing is available; do not claim an animated repair cycle from the static model.

Worker colonies, arbitrary placed repair machines, creation repair, LifeLimit, source save importing and full machine animation remain outside this increment. Knife33 must continue to be rejected; adding an instant inventory-wide repair command would not match the recovered camp flow.

## Verification

The exporter guards complete source files, the source/installed serialized machine bindings, the read-only database and both exact archive profiles. It regenerates the63 C material profiles and retains full evidence for exclusions and empty jobs. Ten focused Python tests pass with zero skips in the existing isolated UnityPy interpreter. They cover exact regeneration; entity/type/creator/duration bindings; catalog and material closure; source/prefab/database/bundle drift; all326 possible archive truncations plus trailing bytes; corrupted owners/timers/flags; malformed materials; and missing-material rejection. No reference input was changed.

```sh
rebuild/build/unity-tools/bin/python rebuild/tools/export_camp_repair.py --check
rebuild/build/unity-tools/bin/python rebuild/tests/test_camp_repair_export.py
```

The disconnected C-owner tests pass optimized and ASan/UBSan with leak detection. They cover all63 profile quotes, full/broken and malformed instances, required external-owner guards, atomic missing-cost rollback, busy insertion/return rejection, no-refund cancellation and paid restart, same-category replacement with a full bag, failed return capacity, and snapshot ID collision/malformed-state rejection. Clock tests include paused/subtick/invalid/overflow input,3600 display intervals of float1/60 leaving2400 ticks, and the final.024-second update producing exactly one completion edge. Source/UI's potentially repeated idempotent `Repair.Do` is represented by one equivalent native maximum-durability completion.

All63 current profiles are category1 and have one repair material. The generic cross-category and multi-material transaction branches therefore have no current source-profile fixture coverage; their implementation must not be described as exhaustive tested parity. No zero-maximum profile is emitted. These source/headless checks do not establish playable service UI, original partial-job replay or new physical acceptance.
