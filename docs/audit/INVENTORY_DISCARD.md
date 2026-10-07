# Inventory discard and confirmation

The sandbox inventory now exposes **Delete → select item → confirm**. It destroys the whole selected slot; use Split first to discard only part of a stack. This is a source-derived single-player bag operation, implemented and tested without a desktop or original-game session. It creates no ground item and adds no save fields.

## Source contract

`Assets/Scripts/Chenzhi/NewUI/Scripts/PE_GameUI/GameWnd/UIItemPackageCtrl.cs` owns this flow. `OnDeleteBtn` toggles operation2; a fresh left click retains the selected grid and opens yes/no message8000055. IDs whose integer quotient by10000000 is9 are rejected before the confirmation. `DeleteSelectedItem` additionally rejects an exact `ItemProto.category == "Quest Item"`; that category string is separate from the four bag tabs. In single player it calls `ItemMgr.DestroyItem(instanceId)` and clears that slot. There is no quantity field in the delete path. The source split operation is how a player first isolates a smaller quantity.

`ItemManager.cs::DestroyItem` sends its destruction event and removes the instance. `ShortCut.cs` resolves ordinary stack shortcuts by prototype and durable items by exact instance. Its original refresh removes exhausted stack shortcuts; the native rebuild deliberately retains its already established empty prototype shortcuts. Discard preserves that existing adapter while clearing shortcuts to a deleted exact instance. It never transfers or rebinds a shortcut to another same-prototype tool.

The source operates on package cells, while worn equipment lives under separate owners. Native selected tools/shields remain represented in the bag but are hidden there; consequently `pe_game_can_discard_slot` rejects selected primary, selected shield, worn-outfit aliases and deployed aliases. Unequip a supported selected item before discarding it. Unsupported outfit removal is not added by this operation.

[export_inventory.py](../../rebuild/tools/export_inventory.py) now exports the source Quest Item flag alongside all1743 prototype classifications and sort labels. Full source guards include the package UI, item manager and shortcuts. Prototype1332 demonstrates a protected source quest item even though no such quest item is currently in the supported runtime catalog. Unknown source prototypes fail closed. This does not implement the separate mission package.

## Transaction and UI ownership

`pe_game_discard_slot(game, slot, expected)` checks the complete selected slot snapshot, including prototype, count, exact instance and wear, before committing. It requires a living player, a valid inventory and unowned bag item. Any snapshot difference or invalid input leaves the whole game unchanged. Successful deletion zeroes the selected slot and only its exact-instance shortcuts. It preserves all other stacks/instances, the allocation counter, money, world, progression and RNG. Inventory-based quest readiness continues to be derived by its existing owner.

The UI retains a transient confirmation snapshot. Enter or a fresh confirmation click commits; Escape or Cancel closes only the confirmation. Item mutation cancels the stale confirmation, so sorting, consuming, changing wear or replacing a slot cannot delete a different item. The Delete mode remains armed after confirmation/cancellation, matching the source operation mode; its button or a right click cancels that mode. Changing tabs/operations and closing the inventory clear the mode. A right-click cancellation cannot also use or equip an item.

The confirmation owns digits, quickslots, save/load, tab changes and other panels. Its closing frame remains consumed, and the existing host pointer gate owns held/release frames afterward. Focus loss cannot confirm. Death and host panel cancellation clear the transient confirmation. The dialog wraps the item name and shows the whole-stack consequence; Split, Sort and Delete controls remain within the moved/resized inventory layout.

No full source mission-update dispatcher, multiplayer request, unrestricted item ownership migration or world-drop object is claimed. The confirmation snapshot is a native stale-selection safeguard.

## Verification

[test_inventory_discard.c](../../rebuild/tests/test_inventory_discard.c) exercises the real core and UI route headlessly: protected/unknown IDs; stack isolation then whole-slot deletion; unchanged allocation/prototype shortcuts; exact tool identity cleanup; equipped, shield, outfit and deployed guards; dead/nonfinite player state; count/wear changes; invalid slot/NULL request atomicity; confirmation/cancel; blocked shortcut/save/load/panel input; focus loss; stale selection; repeated confirmation; held-pointer release ownership; and narrow/moved control geometry. The existing inventory UI regressions and exporter checks remain part of validation. Physical presentation is not established by these checks.

The final focused optimized run and ASan/UBSan run with leak detection pass against the expanded341-item/108-formula catalog, including simultaneous save/load on both Delete-arm and confirmation-opening frames. Existing inventory UI regression, four exporter tests with zero skips, regeneration check and strict renderer syntax check also pass. Local logs are `rebuild/build/inventory-discard-final.log` and `inventory-discard-sanitize-final.log`.
