# Source-derived wild harvesting

This increment replaces the Adventure example patch's provisional tree damage and fixed primary-only herb yields with the original resource calculations. Placement stays fixed. It uses offline C#, database and serialized-prefab evidence; no original-game session or new visual acceptance is claimed.

## Identity and ownership

The existing recovered `sandbox-world/tree.obj` is `tree11_grassland`, whose **VETreeProtos index is48**, not11. Source GUID `2055b9405382acf4793ddceef4234dda` binds that index to the prefab. `export_harvest.py --check-identity` decodes the512-entry original list with UnityPy and calls the existing qualified mesh/capsule exporter to verify that the displayed tree has the same identity. Ordinary `--check` pins the complete original list and tree-prefab bytes. Neither check starts Unity.

The original mesh, material and capsule are unchanged. All currently instantiated trees and herbs remain unit width/height, so rendering, collision, interaction rays and source resource calculations agree. General species selection, random placement, nonunit runtime geometry and regrowth remain deferred. The pure calculation tests cover other scales without enabling mismatched runtime geometry.

[pe_harvest](../../rebuild/src/pe_harvest.h) owns production and hardness calculations. [pe_game_cut_tree](../../rebuild/src/pe_game.c) applies them to Adventure trees; [pe_vegetation](../../rebuild/src/pe_vegetation.c) commits herbs through the same production owner. Existing tree HP and depletion, plant depletion and the single `PeGame.rng_state` remain the only persistent owners. There is no new harvesting save field. Older saves retain their current tree HP/depleted state and use the new resource calculation on later events. The old `PeTree.remaining` field controls availability/visibility; its positive value no longer overrides source Adventure loot. Legacy worlds retain their earlier bootstrap/8-Wood behavior.

## Active source call chain

`Action_Fell` validates its axe and the selected type9 object, then its qualified animation event drives the existing native tool clock. In the original, `SkEntitySubTerrain.onAlterAttribs` reads damage, resource bonus and return-resource from the skill target attributes, calls `DigTerrainManager.Fell`, and on HP exhaustion removes the instance and asks `GetTreeResouce` for loot. Fresh HP is255; database `durability` is a **damage multiplier**, not maximum HP:

```text
newHP = HP − damage × resource.durability / (heightScale × widthScale)
```

Source prototype48 has multiplier`.48571`. Native damage is player base plus the exact equipped axe's `CutDamage`. At unit scale Safety Axe1527 (`CutDamage=28`) takes19 contacts from full health; Wood Axe26 (`46`) takes12. Existing action clocks still own durability wear, stamina and event timing. Capacity rejection at the final contact preserves tree HP, inventory, RNG, tool wear and that contact's stamina debit. The earlier action-start stamina charge remains paid.

`Action_Gather` picks type10 collider-free objects; its original animation event starts skill20110003. Native herbs still collect immediately with action-only input ownership. This increment ports their production distribution, not the Gather animation/event lifecycle. Decorative prototype146 remains deliberately nongatherable despite its source resource row.

## Production rules

`NaturalRes` reads the first production segment as ordered **cumulative cutoffs**; do not sum them a second time. `GetTreeResouce` computes:

```text
quantity = fixed_num > 0 ? fixed_num : self_num × width² × height × (1 + bonus)
quantity = max(1, quantity)
primary trials = truncate(quantity)
optional gate = Random.value < quantity × optionalChance
optional trials, if admitted = ceil(quantity × optionalChance)
```

Each trial draws an integer in `[0,100)` and selects the first cutoff greater than the draw. The gate is drawn even when its threshold exceeds1. The optional loop compares its integer counter against a float; using truncation here would incorrectly lose a drop. The separate rare group resets quantity before applying its own gate; runtime rare enablement remains off because the skill-tree owner is not ported. All selected source rare groups are empty.

| Source object | Primary production | Optional production |
| --- | --- | --- |
|Tree48|`16 × width² × height × (1+bonus)` Wood279 trials,100%|Chance multiplier1.2; Rubber979 up to67, Resin1017 up to100|
|Shade Weed15|2 trials, Shade Weed238 at100%|Gate`.06`; one Shade Weed Seed943 if admitted|
|Tulip37|3 trials, Tulip235 below70,Dye1583 otherwise|Gate`.3`; one Nuts388 below50, Tulip Seed940 below75,Dye1583 otherwise|
|Maria Sunflower40|4 trials, Maria Sunflower239 below70,Dye1583 otherwise|Gate`.4`; one Nuts388 below50, seed944 below75,Dye1583 otherwise|

Unit Tree48 with no bonus therefore grants16 Wood and20 combined Resin/Rubber, not the old8 Wood. Wood Axe26's`.01` bonus is honored: at unit scale the primary result remains16 and the optional total remains20 after the distinct floor/ceil operations. Source item940/943/944 metadata is added to the catalog, bringing it to323 items and95 formulas; seeds can be stored/traded but no planting or farm behavior is implied.

The native implementation stages the entire inventory and RNG. Even when primary resources fit, a full Items category may reject an optional seed, leaving both categories, the plant and RNG unchanged. Retrying after making room uses the same outcome. This is an explicit atomic-capacity adapter to the original removal-before-pack order.

`PeGame.rng_state` uses the existing xorshift32 stream. Integer trials use `%100`; float gates use24 bits over`16777215` to include the source endpoints. Branch/draw ordering and resource formulas are source-derived; this does **not** reproduce Unity's exact random sequence or promise identical outcomes for a Unity seed. No new placement randomization is introduced.

The original postproduction `GetSpecialItem.PlantItemAdd` can append mission-specific drops using another RNG. Its mission registration system is outside the supported quests here and remains unported; the native owner does not invent those mission rewards.

## Regeneration and automated evidence

```sh
python3 rebuild/tools/export_harvest.py --check
rebuild/build/unity-tools/bin/python rebuild/tools/export_harvest.py --check --check-identity
python3 rebuild/tools/export_content.py --check
python3 -m unittest discover -s rebuild/tests -p test_harvest_export.py -v
```

[Generated metadata](../../rebuild/generated/pe_harvest_source.json) retains complete database rows and input hashes. Source method-body guards include `NaturalRes`, `Fell`, `GetTreeResouce` and `SkEntitySubTerrain.onAlterAttribs`; the existing vegetation export separately preserves admission/picking contracts. Negative export tests alter hardness, quantity, production and type and require rejection.

Focused C tests cover the species table, scaled damage, fixed/scaled quantities, fractional floor/ceil differences, primary/fallback/optional seed paths across600 deterministic streams, invalid inputs, late rollback and deterministic retry, including independent category saturation. Actual `pe_sandbox_actions_step` tests cover19/12-contact tree depletion, resulting grants, stamina/tool wear, event timing, final-contact capacity rejection and RNG conservation. Actual CPU-only host tests retain melee/tool admission, modal precedence, pointer ownership, continued movement/AI admission and complete source herb grants. These establish headless integration rather than visual acceptance.

Executed for this increment: the focused optimized kernel/vegetation/action suite passed with0 failures; the same focused suite passed ASan/UBSan with leak detection. The CPU-only actual sandbox vegetation host passed with no asset skips. Source/export suites passed6 harvesting,13 vegetation,17 catalog and11 trade-price tests; all source-content checks passed after regenerating the expanded price catalog. The UnityPy prototype/mesh/capsule identity check also passed. Logs are retained locally under ignored `rebuild/build/harvest-check`. The existing recovered UI atlas includes all three seed icons and their source overlays; no new image recovery was needed. These checks do not establish on-screen acceptance or full Gather animation parity.
