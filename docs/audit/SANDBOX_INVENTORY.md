# Sandbox inventory categories and editing

October 6, 2026. This increment is source-derived and implemented in C; it does not add an original-game or native physical playtest. The preceding [systems audit](SOURCE_AUDIT_SYSTEMS_FOLLOWUP.md) identified the original capacity difference and the retained save-prefix constraint.

## Recovered contract

[PlayerPackageCmpt](../../Assets/Scripts/PeEntity/Cmpt/PlayerPackageCmpt.cs) passes its `PkgCapacity=420` to [PlayerPackage](../../Assets/Scripts/ZhangShunBo/PlayerPackage.cs). That constructs an [ItemPackage](../../Assets/Scripts/Items/ItemPak/ItemPackage.cs) with four independent 420-cell lists: Item0, Equipment1, Resource2 and Armor3. `ItemPackage.GetSlotType` reads `ItemProto.tabIndex`, populated from `PrototypeItem.tab` by [ItemData](../../Assets/Scripts/Items/ItemData.cs). The current recovered catalog contains no Armor-tab items; the source capacity remains represented without inventing armor content. These four category lists are separate from the original mission package, which this increment does not introduce.

The single-player [inventory UI](../../Assets/Scripts/Chenzhi/NewUI/Scripts/PE_GameUI/GameWnd/UIItemPackageCtrl.cs) permits a split from1 through `stackCount-1`; `ItemPackage.Split` subtracts then creates one new stack in the category's first empty cell. Native split preflights that destination, preserving the entire inventory on failure. This deliberate transaction policy avoids copying the source's subtract-before-add failure behavior.

Source dragging to a vacant cell moves the item. Dragging onto an occupied same-category bag cell exchanges the two objects, including when their prototypes match. Native ordinary drag follows that behavior. Explicit merge is a native convenience; it transfers only the available destination stack capacity and retains any remainder. It never merges durable instances.

`ItemPackage.Sort` first calls [SlotList.Reduce](../../Assets/Scripts/Items/ItemPak/SlotList.cs), consolidating stackable prototypes, then sorts ascending by `sortLabel`, prototype ID and stack count. The labels come from `PrototypeItem.sort`. C follows those keys and consolidation, with stable ordering for equal keys. Original unstable tie behavior has no meaningful item-order guarantee; C's stability preserves worn-instance order rather than manufacturing one.

[export_inventory.py](../../rebuild/tools/export_inventory.py) guards the source files and exports every numeric prototype's category and sort label with database/file hashes. [pe_inventory_source.json](../../rebuild/generated/pe_inventory_source.json) records the contracts and adapters; the generated header supplies the runtime sort lookup. The exporter is independent of selected content so additional recovered harvest items already have sorting metadata.

## Native ownership

[pe_inventory.c](../../rebuild/src/pe_inventory.c) owns add/remove/count, active capacity, stable category migration, exact-slot transfer, split, exchange, merge and sorting. `PeInventory` remains a value object with no new heap ownership. Adventure uses four420-cell segments in a1680-cell array; Legacy and the tutorial retain their established32-cell layout. `PE_INVENTORY_SLOTS` is deliberately still32 for the literal historical save prefix. General gameplay consumers use `pe_inventory_capacity` and category bounds.

Ordinary acquisition stacks within the item's source category, then fills its first empty cells. Filling Equipment cannot block Resource or Item acquisitions. Each durable instance retains its exact ID and wear through editing; the shared next-instance counter never rewinds. Equipment and shield references remain instance keyed, while stack quickslots retain their existing prototype-keyed behavior. Sorting/splitting never grant extra items or duplicate an instance. Vendor buyback and deployable returns use the same exact-transfer owner.

The UI exposes physical category cells and pages, including holes, so moving a stack has a stable visible destination. Equipped items continue to have a single bag owner and are hidden from ordinary bag presentation; those reserved cells cannot be used as drag destinations. Modal inventory controls retain input through release, preventing split, sort or drag clicks from attacking or mining.

## Persistence

PECSv19 retains the complete v18 payload layout and appends26888 bytes: layout/count words and1680 fixed16-byte item/count/instance/durability records. Current Adventure requires the categorized layout; Legacy requires its32-cell layout and zero unused tail. The retained32-cell prefix projects the first32 occupied category cells for Adventure, while Legacy retains its original positions. V19 verifies that projection against its authoritative complete inventory before accepting a load. It does not acquire duplicate items from the prefix. Truncating the suffix of a save containing more than32 objects is not a supported downgrade. This retained prefix is structural compatibility, not a downgrade feature: truncating a current save with more than32 occupied records cannot produce a complete historical inventory.

Pre-v19 saves are validated under their original flat32-cell contract, then Adventure migrates each existing occupied cell into its category in stable original order. Identities, wear, counts, quickslots and the next-instance counter survive; the migration does not merge stacks or reset equipment. Existing world and quest migrations remain separate. Current snapshots preserve all category holes and ordering. Malformed records, wrong categories, duplicate identities, noncanonical unused cells and invalid envelope lengths are rejected before replacing the live game.

## Verification scope

```sh
make -C rebuild inventory-test sandbox-inventory-ui-test
python3 rebuild/tools/export_inventory.py --check
python3 -m unittest discover -s rebuild/tests -p test_export_inventory.py -v
```

Focused C tests cover independent category saturation, Legacy capacity, cross-category rejection, first-empty split, full-category split rollback, source exchange, partial/full merge, source consolidation/order, durable identity/wear, shortcut references, stable migration and exact transfers. Python checks cover source guards, metadata reproduction and current catalog classification coverage. The migration/save suite covers historical prefixes and the v19 authoritative payload. Final aggregate results and the remaining visual acceptance belong in [PLAYTEST](../PLAYTEST.md).

Source mission inventories, creation item categories, network replication, new-item highlights and the complete original drag/modal presentation remain outside this increment. One consolidated future playtest should inspect category pages, empty-cell rearrangement, split amounts, shortcuts and window readability; none of the source capacity or sort constants depends on that visual check.

## Ordinary native acceptance

**October 14, observed in the native production runtime with ordinary input;
no original-game comparison.** Evidence: [manifest, input journal and lossless
panel crops](../reference/inventory-native-20261014/checkpoints.json). Full
captures and saves stay under `rebuild/build/inventory-private-20261014-01`.
[Discard](INVENTORY_DISCARD.md#ordinary-native-acceptance) was checked in the
same session.

*Setup.*
- Runtime `7003add`, binary SHA256 `f5c07847…a8e`, private display `:121`,
  Claude as sole operator.
- Baseline: the earned `returned` save from repair session 02
  (`1a791bbf…55cc`), unchanged afterwards. Resource cells 840–842 held item
  229 ×400, Wood 279 ×193 and item 270 ×4. Money was 261, and the Knife
  (instance 1) was equipped.
- No fixture, cheat, save edit or grant. Each save was made with Pause → Save
  after closing the inventory.

Adventure category cells are 420-cell segments (Item 0–419, Equipment
420–839, Resource 840–1259). The panel shows six cells per row.

*Observed, in order:*

| Step | Input | UI result | Saved state (inspector) |
|---|---|---|---|
| Split | I, Resource tab, Split, click Wood, type 1 then 0 | "Split stack / Choose 1 - 192", value 10 | — |
| Confirm | Enter | "Stack split: 10 items."; 183 stays; 10 in the 4th cell | `split` (`75f9713f…e080`): 841 Wood 183, 843 Wood 10 (first empty Resource cell) |
| Move | drag the 10 to the empty first cell of row 2 | stack moved | — |
| Exchange | drag the 10 onto the 183 | the two stacks swap places; no merge | `exchanged` (`b4ac8d74…1f40`): 841 Wood 10, 846 Wood 183 |
| Sort | Sort | "Inventory category sorted."; Wood shown once as 193 | `sorted` (`b95afcb6…23ff`): 840 item 229 ×400, 841 item 270 ×4, 842 Wood 193 |
| Partial discard | Split 10 again, Delete → Cancel, then Delete → Discard; see the linked discard audit | only the isolated 10 removed | `discarded`: Wood 183; other item totals unchanged |
| Equipment sort | Equipment tab, Sort | "Inventory category sorted."; page 1 has one hole, the hidden equipped Knife | `equipment-sorted` (`ca79fffb…866c`): see below |

*Against the source.*
- **Split (observed match):** the new stack took the first empty cell in its
  category, and the total stayed 193.
- **Same-prototype exchange (observed match):** dragging one Wood stack onto
  another swapped them without merging, as source `ItemPackage` drag does.
- **Sort (observed match):** Sort consolidated the two Wood stacks into 193
  (source `SlotList.Reduce`). It then ordered the Resource cells by ascending
  source sort label: 229 (10079), 270 (10464), 279 (10472).
- **Equipment sort:** the Equipment cells were reordered by ascending source
  label: 1, 33, 34, 43, 12, 19, 26, 1527, 70, 228, 73. Every instance kept its
  exact ID and wear, for example shovel 10 at 3543, now in cell 424, and axe 12
  at 3553, now in 426. The equipped Knife moved from 420 to 421 and stayed
  hidden there. `equipped_instance` stayed 1, and quickslot 1 still refers to
  item 33, instance 1.

Across all saves the Item tab, the set of instance IDs, money, outfit,
quickslots and the empty repair machine stayed unchanged. Item totals matched
the baseline except Wood after the discard.

*Not exercised.* The explicit Shift+drop merge is a native adapter that needs a
key held during a drag; the input helper was deliberately not extended, so it
remains unobserved. Also unobserved: a split into a full category, split
values typed beyond the maximum, the small window and paging with real
content.

*Incidental input.* Two clicks used to move the pointer away landed on UI: one
on an empty Resource cell, and one on the last-page button. The second showed
page 10 until the first-page button was clicked. Neither changed saved state.

The session ended by explicit guarded stop (owned game SIGTERM, −15). The
supervisor, Xvfb and game PIDs were absent afterwards, and the baseline was
unchanged. Ten lossless 380×576 panel crops were checked pixel-exactly. No
runtime code changed.
