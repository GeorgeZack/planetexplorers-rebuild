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
