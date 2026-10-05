# Archived settlement shops and Annot arrival

This increment implements a bounded, source-derived vendor catalog and mission9032 completion in C. It is not an original-client observation. Graphical verification belongs in the playtest record after the integrated frontend is exercised.

## Vendor identity and catalog

The preserved reference archive binds Cherish Mercer/entity20002 to RandomNpc141, Andre Dale/entity20014 to146, and Virginia Cannan/entity20011 to151. [UINpcWnd.cs](../../Assets/Scripts/Chenzhi/NewUI/Scripts/PE_GameUI/NGUIScripts/UINpcWnd.cs) resolves a random NPC's prototype through `NpcMissionData.m_Rnpc_ID`, then [StoreRepository.cs](../../Assets/Scripts/Items/Shop/StoreRepository.cs) selects `NPCstore.adven_single`. Westina and Eitai are not vendors. The visible shop building is not itself the owner of these inventories.

[export_town_interaction.py](../../rebuild/tools/export_town_interaction.py) checks the archived entity/prototype bindings and exports seventeen retained starting-town offers plus267 Annot offers. It keeps source store IDs, item IDs, prices, limits, refresh intervals and input hashes in [the manifest](../../rebuild/generated/pe_town_offers.json). The selected order is a persistence contract; expanding or reordering it needs a save migration. The starting-town catalogs remain bounded; all five archived Annot catalogs are included. Selling and buyback remain unimplemented.

| Vendor | Supported source items |
| --- | --- |
| Cherish | Wood Sword1, Wood Shovel12, Wood Pickaxe19, Wood Axe26 and their four scripts424/406/412/418 |
| Andre | Wood Shield34 and its script433 |
| Virginia | Revival937, Herbal Juice916, Cycad234, Tulip235, Grassland Plant238, Sunflower239, Lotus Seed241 |

The fresh Annot archive additionally binds five source stores. Their `NPCstore.adven_single` lists are exported in source order; every included Annot row has `PreQuestLimit=0` and `extra_demand=0`. The twelve new residents were checked by actual prototype, not by their names or nearby buildings. The other seven generated Annot residents and role3/4 quest actors have no source shop row.

| Annot vendor | Entity / prototype | Offers |
| --- | --- | --- |
| Xing Zhang, supplies |20028 /153|57|
| Sherley Hansen, furniture |20031 /158|31|
| Abe Okorie, groceries |20032 /163|79|
| Ntombifikile Petros, weapons |20033 /143|58|
| Tammana Chu, armor |20037 /148|42|

The first seventeen stock indices remain unchanged. Annot records append in the table's vendor order, with each vendor's original source row order. Lookups use `(entity_id, store_id)`, so a source store row reused by another NPC cannot share stock accidentally. [The content exporter](../../rebuild/tools/export_content.py) closes purchased item definitions and learnable formulas over their inputs/outputs:314 item definitions and93 formulas in total. Buying an item does not enable missing combat, equipment, deployment or consumption behaviors.

These are inventory transactions; a supported item definition does not establish that every item's equipment or consumable behavior has been ported. The supplied original names are obtained from the native content catalog when rendered.

[PlayerTypeLoader.cs](../../Assets/PeLauncher/PlayerTypeLoader.cs), `InitNew`, explicitly enables digital money for Adventure. [ShopRespository.cs](../../Assets/Scripts/Items/Shop/ShopRespository.cs) consequently selects `sale_price2`; buying one Wood Shovel costs158 from the actual player balance. Stock is finite per archived entity and offer. A staged inventory must accept the entire purchase before money and stock are debited. Insufficient money, stock, capacity, wrong vendor identity and invalid quantities preserve all three. Buying durable items uses the existing inventory instance allocator.

[UIShopWnd.cs](../../Assets/Scripts/Chenzhi/NewUI/Scripts/PE_GameUI/NGUIScripts/UIShopWnd.cs), `UpdataShop`, has two details preserved here. Refresh uses strict `GameTime.Timer.Second - CreateTime > refresh_time` (46800 game seconds). Merely opening a nonempty entry resets its creation timestamp, even before refresh; an exhausted entry keeps its prior timestamp. [StroyManager.cs](../../Assets/Scripts/Mission/StroyManager.cs), `InitBuyInfo`, creates full stock on first opening. The native host performs this lazily on shop opening and persists remaining stock and creation time in PeGame. No wall-clock/offline refill is invented.

The Wood Sword offer1183 carries `PreQuestLimit=1:2,3,4,8`. The inspected `UpdataShop` initializes `bPass=true` and never sets it false for type1 limits, so this row is exposed even without those completed missions. Type2 gated rows are not in the bounded export. This records the actual inspected source path rather than correcting its behavior speculatively.

## Annot discovery and medicine content

[The prior target audit](ADVENTURE_COLONY_MARKERS.md) establishes fixed discovery4503 at original coordinates(-284,25,134), strict XZ distance<15 and a separate60m tracking radius. [pe_town_interaction.c](../../rebuild/src/pe_town_interaction.c) checks actual source-world coordinates, finite living player state, original-world Y>=0, accepted supplies and active9032. Arrival sets the saved `annot_complete` flag once. It grants no reward and leaves all inventory, money and tools unchanged. The source9032 row has `AutoReply=1`, `RewardItem=0`, `NextQuest=0`. The persisted mission ID remains9032 with the separate completion flag for the bounded native tracker.

Arrival unlocks the real role3 Naoko Ueda question4095 and reply4096 through [pe_adventure_dialogue](../../rebuild/src/pe_adventure_dialogue.h). These are quest9071 (`PreLimit=1:9032`) and QC32; the reply explains the illness delaying supplies. Dialogue remains modal in the host and grants nothing. Medicine progression extends this encounter; its acceptance, return and completion contracts are owned by the medicine quest module and its tests.

Quest9072 source content grants script877 on acceptance, requires five item917 Herbal Drinks, removes those five at completion and rewards925×10 plus554. Script877 learns formulas337/388/390. It has `_typeId=0`, so the old type38-only recipe-book classification would reject it. The export now follows `_function` into `item_class` containing `ReplicatorFormula` (classes28/74), matching the original component construction; this also supports compound scripts876/878. Item554 learns formula149.

Xing sells every ingredient for all three Herbal Drink formulas. Formula337 consumes Sunflower239×1, Tulip235×2, Lotus Seed241×2, Grassland Plant238×1 and Euphorbia233×1. Each costs3 currency, so buying the35 ingredients for five drinks costs105. The same timed native replicator owns crafting and item delivery; no ingredients or medicine are invented as quest gifts. Role4 Sajiva's wider chain remains unported. Missing starting-town names must not be fabricated from Annot's name; the source question contains a separate `Town000` binding.

## Verification

[Core tests](../../rebuild/tests/test_town_interaction.c) exercise vendor identity, debit, stable inventory allocation, finite stock, strict refresh boundary, exhausted/nonempty clocks, capacity failure, malformed persisted stock, strict arrival boundary, below-world rejection, idempotence and the source4095/4096 flow. [Exporter tests](../../rebuild/tests/test_town_interaction_export.py) check archived bindings, catalog closure, digital prices, regeneration, Adventure-mode membership and unsupported gates/cost rejection. The current roundtrip test includes both retained and appended vendor stocks, currency and stock clocks. Legacy v8 migration is validated separately by the save tests. The headless real-game UI routing test covers shop purchase, quantity, modal shortcut consumption, generic close-only menus, drag/resize clamping and the Naoko introduction. These are headless C/source checks, not a claim of graphical or original-game parity.

The native shop panel uses recovered UI frames, fonts and item icons. Its compact catalog layout is a native adaptation, not a reconstruction of the full Unity shop prefab. Header dragging stores only transient UI offsets and clamps the panel within the viewport; quantity and Buy use the same geometry for drawing and routing. Escape, Back, close, dialogue and drag release consume the current input frame. The host retains responsibility for resolved NPC identity, physical interaction eligibility and canceling modal state on load or loss of the encounter.

## Headless archived placement observation

The focused [population fixture](../../rebuild/tests/test_adventure_population.c) passed with the recovered original64×32×128 settlement terrain and building collision. All ten in-crop archived NPCs retain exact XZ and yaw with supported physical capsules. Nine retain archived Y exactly at float precision; Westina settles upward0.003098m. Cherish and Andre both remain supported at their archived shop positions; Virginia also remains at her archived Y. Each placed actor passes the sex-aware interaction query. A separate query fixture at local(350,25,350) distinguishes the source male0.6m Defence radius from female0.5m without the old64m pose restriction.

Jailekha20015, Naoko20018 and Sajiva20019 fall outside the old crop and were explicitly omitted from placement coverage. The expanded-route terrain/collision were unavailable during this focused run, so that branch reported an asset-dependent skip. It is not an all-thirteen placement pass or graphical evidence. The same fixture surveys every metadata entry inside the expanded route when both recovered files become available.

## Final graphical checkpoint

Original Annot arrival and the Haleiwa-bound Naoko introduction were observed. Physical native shop purchase/window/reload and Annot arrival/dialogue checks passed with explicit fixture and process boundaries in [PLAYTEST](../PLAYTEST.md) and [graphical evidence](../reference/town-route-graphical.json). That earlier checkpoint predates the medicine and Annot vendor extension; it does not establish live behavior for this extension.
