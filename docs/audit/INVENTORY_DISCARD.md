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

## Ordinary native acceptance

**October 14, observed in the native production runtime with ordinary input;
no original-game comparison.** This was checked in the [inventory editing
session](SANDBOX_INVENTORY.md#ordinary-native-acceptance), from the sorted
state (Wood 193 in Resource cell 842). Evidence:
[manifest](../reference/inventory-native-20261014/checkpoints.json).

- **Isolate:** Split was used to make a Wood ×10 stack in the first empty
  Resource cell, 843, leaving 183.
- **Cancel:** I pressed Delete and clicked the 10. The prompt read "Discard
  this item? Wood x10 / Destroys the whole stack. Split first to keep some."
  Clicking Cancel closed only the prompt. Both stacks stayed, and the mode
  stayed armed ("Select an item to discard"), as documented.
- **Confirm:** I clicked the 10 again and then Discard. The message read
  "Item discarded."; the 10-stack disappeared and 183 remained.
- I disarmed the mode with its Cancel button before closing the inventory.
- **Save:** `discarded` (`baa57245…705a`) has Wood 183 in total, in cell 842 alone.
  The only item total that changed is Wood (193 → 183). Every instance ID and
  wear, money 261, the equipped Knife and the quickslots are unchanged.

The whole selected stack was destroyed and nothing else was, matching the
source. No world drop is modeled; the absence of a ground item was not
separately inspected. The Cancel step is shown by the UI only, because no save
was made between Cancel and Confirm. Not exercised in this session: discarding
a durable tool and Escape or Enter on the prompt (both observed in the
[next session](#durable-tool-with-a-quickslot-session-01-october-15)), the
protected Quest Item rejection (no supported quest-category prototype exists)
and the equipped-item guard (the equipped primary is hidden and cannot be
selected).

### Durable tool with a quickslot (session 01, October 15)

**Observed in the native production runtime with ordinary input; no
original-game comparison.** Session `rebuild/build/discard-private-20261015-01`
on `:122` started from the earned `equipment-sorted` save (`ca79fffb…866c`,
unchanged). Evidence:
[manifest](../reference/discard-native-20261015/checkpoints.json).

- **Bind:** the inventory window was dragged up by its title so it no longer
  covered quickslot 5; drops onto a covered quickslot are refused. Dragging
  the Wood Pickaxe (prototype 19, instance 11, wear 3553) from Equipment cell
  425 onto quickslot 5 bound that exact instance and left the tool in its
  cell. Save: `bound` (`194b0b0d…3649`).
- **Escape:** Delete, then a click on the pickaxe, opened "Discard this item?
  Wood Pickaxe x1". Escape closed only the prompt: the inventory stayed open,
  no pause menu opened and the mode stayed armed ("Select an item to
  discard"). After disarming with the mode's Cancel button, `escaped`
  (`b7f677ab…1ff1`) matched `bound` in every inventory, equipment, outfit, quickslot, money and
  repair field recorded in the manifest.
- **Enter:** Delete, the pickaxe again, then Enter. The message read "Item
  discarded."; cell 425 and quickslot 5 emptied at once and the mode stayed
  armed. `discarded` (`8583830d…9b56`) lacks only instance 11 and the
  quickslot 5 reference. Every other cell, instance and wear, quickslots 1–4,
  the equipped Knife (instance 1), outfit, money 261, Wood 183 and the empty
  repair machine are unchanged.

This confirms in play that discard destroys the exact durable instance and
clears only shortcuts to it, and that the prompt's Escape and Enter keys act
as documented. No other Wood Pickaxe existed, so not rebinding to a
same-prototype tool was not exercised. The Quest Item and equipped-item guards
remain unobserved.
