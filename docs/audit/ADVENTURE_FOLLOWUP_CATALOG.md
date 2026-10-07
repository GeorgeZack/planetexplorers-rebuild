# Catalog closure for Adventure9077–9084

October7,2026, US/Central. This is source-derived item/formula preparation for the disconnected follow-up mission events. It adds definitions and verifies the existing learning/crafting path; it does not accept those missions, grant their items in a playable session, spawn an encounter, place colony machines or enable vehicle creation. No game or desktop access was used.

## Exact source dependencies

`AdRandomQuest_List` has no accept/reward/remove item entries for9077,9078,9079,9081,9083 or9084. The two relevant source contracts are:

| Mission event | Exact item list |
| --- | --- |
|9080 acceptance|Assembly Core Script657×1, Power Plant Script662×1, Dwelling Bed Script660×1|
|9082 acceptance|Pigment Script597×1|
|9082 completion removal|Pigment1038×50|
|9082 completion rewards|Scripts478,490,696,465,474,473,481, each×1, in that source order|

Of the eleven referenced scripts,597 and474 were already present. Pigment1038 and its formula192 were also present. The missing nine scripts all use source component class28, `ReplicatorFormula,RecycleReplicate`; none needs a new learning component or altered item identity.

| Script | Original explicit learned formula IDs |
| --- | --- |
|657 Assembly Core|252,89,123,56,42,295,43,46,48,51|
|662 Power Plant|257,41,42,51|
|660 Dwelling Bed|255,328|
|478 Vehicle Engine Type01|73,41,42,51|
|490 Vehicle Cockpit Type01|85,89,41,42,51,123,56,328|
|696 Side Seat|291,42|
|465 Rubber Tire|60|
|473 Fuel Cell Type01|68,56,42|
|481 Vehicle Headlight Type01|76,123,56|

The largest script has10 formula IDs, within the existing16-entry item definition. All newly reached recipes use workspace0, positive source durations and the already supported ingredient/output representation. The exporter fails closed if these script classes, learning lists, mission item lists or workspace requirements change.

## Resulting catalog

The closure adds18 item definitions and13 formulas, taking the catalog from323/95 to **341 items/108 formulas**. The existing95 formula ordering remains an unchanged prefix; new formulas append after the existing book closure. Saves retain formula IDs rather than catalog indices, and108 is below the128 known-recipe capacity. There is no save layout change.

New item IDs are465,473,478,481,490,657,660,662,696,985,1013,1014,1018,1127,1128,1131,1165 and1180. Existing vehicle output items are reused where already in the catalog.

| New formula | Output | Time | Quantity |
| --- | --- | --- | --- |
|43|1018 Multilayer PCB|.5s|1|
|46|1013 Resistors|.5s|1|
|48|1014 Capacitors|.5s|1|
|60|1159 Rubber Tire|4s|1|
|68|1172 Fuel Cell Type01|5s|1|
|73|1162 Vehicle Engine Type01|5s|1|
|76|1165 Vehicle Headlight Type01|5s|1|
|85|1157 Vehicle Cockpit Type01|10s|1|
|252|1127 Assembly Core|10s|1|
|255|1131 Dwelling Bed|10s|1|
|257|1128 Power Plant|10s|1|
|291|1180 Side Seat|2s|1|
|295|985 Chipset(1GHz)|1s|1|

[export_content.py](../../rebuild/tools/export_content.py) derives ingredient/output closure from the original database and records the exact mission lists and source component bindings in [content_manifest.json](../../rebuild/generated/content_manifest.json). The generated C changes only the existing item/formula catalog; it adds no future mission grant arrays or execution branch. The starting inventory, supplies, current vendors and accepted quest state remain their existing source lists. The companion trade-price exporter regenerates price metadata for341 definitions, without adding shop offers.

Assembly Core1127, Power Plant1128 and Dwelling Bed1131 have source class52, `RecycleReplicate,Drag,Instantiate`. Their inclusion is **inventory/crafting metadata only**. Existing `pe_game_use_slot`, general placement and Torch deployment still reject them. Vehicle components likewise do not acquire construction, assembly or movement behavior through this catalog closure.

## Automated verification

[The focused C test](../../rebuild/tests/test_followup_catalog.c) uses the real `pe_game_use_slot` for each of the nine new scripts, verifies every declared formula, and retries a duplicate script to establish no extra consumption or learning. It then crafts each of the13 new formulas with exact ingredients through the real timed game update, checking missing-material rejection, no immediate output, complete ingredient debit and source output count. The colony-output negative tests verify unchanged inventory and no deployed object after unsupported use/placement attempts. No quest is advanced by these tests.

[Content export tests](../../rebuild/tests/test_content.py) verify all9077–9084 item lists, component/learning closure, preserved formula prefix, workspace0 and unchanged starter/supply grants. Negative tests alter acceptance quantity, Pigment rewards, unsupported item hooks, component class, formula list or workspace and require rejection. Trade-price tests use the regenerated341-entry catalog.

```sh
python3 rebuild/tools/export_content.py --check
python3 rebuild/tools/export_town_trade_prices.py --check
python3 -m unittest discover -s rebuild/tests -p test_content.py -v
python3 -m unittest discover -s rebuild/tests -p test_town_trade_export.py -v
```

Executed October7,2026: the focused C integration test passes in optimized and ASan/UBSan builds with leak detection enabled. Content tests pass19/19 and trade-price tests pass11/11, with no skips; both exporter checks pass. The installed recovered UI atlas already contains every icon and overlay referenced by the18 added items. These are source and headless checks, not physical gameplay acceptance.

This preparation removes missing item/formula definitions as a blocker for later mission-event transactions. It does not remove their geometry, encounter, travel, colony, vehicle or final native integration gates.
