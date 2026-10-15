# Fixed sandbox vegetation examples

The user requested foliage, trees and herbs that can be tested before procedural terrain generation. This patch provides a deterministic example area, not recovered original spawn locations or a biome generator. [The source audit](SANDBOX_VEGETATION_SOURCE.md) distinguishes actual wild prototype identities, meshes, materials and picking rules from native fixture policies.

## Supported behavior

Fresh sandbox sessions install two cuttable trees, six harvestable herbs (two each of original prototypes15,37,40), and two decorative foliage plants (prototype146) near the initial player. The installer scans fixed nearby offsets, settles them against current terrain and shared geometry, excludes archived residents and existing trees, and commits the entire patch together. It does not consume RNG or grant inventory. If the patch cannot fit, startup reports that failure.

Existing saves retain their contents. The explicit `--vegetation-examples` sandbox option installs one patch near the loaded player's current position. Repeating it leaves the patch and any depleted plants unchanged. Terrain replacement clears vegetation with the old world; normal clone/move/menu/save operations retain it. An explicit terrain import only gets examples when the option is supplied.

Approach a herb within2m horizontally, point at it and press E or use a short right click. Selection uses recovered renderer bounds, the source nearby vertical-cell limit and native line-of-sight checks. The nearest eligible herb competes with NPCs and deployed objects; automatic welcome and existing modal input take priority. Collection commits inventory, saved RNG, depletion and the gathered statistic atomically. Insufficient space in any required category leaves the herb and RNG intact and retryable. Collection consumes only action input, so movement and other actors continue. The depleted model disappears immediately.

[Source harvesting](SANDBOX_HARVESTING.md) now supplies the actual production rules: Shade Weed15 gives2 Shade Weed238 with an optional seed; Tulip37 and Maria Sunflower40 perform3/4 primary trials respectively, including Dye fallback and optional Nuts/seeds/Dye. Outcomes use the saved native RNG, without claiming Unity sequence equivalence. This supersedes the initial fixed-primary example policy. Prototype146 is decorative despite having source resource metadata. Plants do not become player/camera/tool walls; their source renderer bounds are picking targets only.

The two trees reuse the existing recovered `Grassland_tree_B` appearance, source capsule, axe action and saved tree owner. The asset is qualified as VETreeProtos48:255 initial HP, `.48571` damage multiplier and source Wood plus Rubber/Resin production. At unit scale and zero or Wood Axe `.01` bonus, felling grants16 Wood plus20 combined Rubber/Resin. Safety Axe1527 takes19 contacts from full health; Wood Axe26 takes12. “tree11” is the asset name, not the numeric vegetation prototype. Legacy retains its old bootstrap/yield. Later terrain edits do not uproot or resettle these fixed objects. No regrowth, forest generation, wind or full Unity foliage shader is claimed. Gathering is immediate; the source Gather animation/action timing remains unported.

## Runtime owners and persistence

- [pe_vegetation](../../rebuild/src/pe_vegetation.h) owns fixed placement, source-bounds picking, validation and atomic harvesting. Inventory remains owned by PeGame; tree state remains in its existing array.
- [pe_vegetation_view](../../rebuild/src/pe_vegetation_view.h) loads the optional recovered group and draws living plants with source alpha cutoffs. A saved patch requires complete valid visuals; a partial group fails closed. No graphics state is saved.
- [Source metadata exporter](../../rebuild/tools/export_vegetation_source.py) guards original identities and rules. [Offline asset exporter](../../rebuild/tools/export_sandbox_vegetation_assets.py) recovers actual wild prefabs into an ignored asset directory, preserving source hashes and IDs.
- [main.c](../../rebuild/src/main.c) installs examples on new sandbox routes or explicit CLI request. The tutorial retains its separate lesson gathering behavior.

The v18 vegetation extension, retained by every later version, preserves the v17 payload layout and appends648 bytes: installed flag and count (two32-bit words), followed by32 fixed20-byte records (prototype ID, XYZ floats, depleted flag). Unused records must be zero. Counts, flags, finite coordinates, supported prototype IDs, world bounds, duplicates and decorative depletion are validated. Versions1–17 infer no plant patch; their existing trees are retained. Save/load and invalid-load rejection are transactional. Loading never generates or replenishes vegetation.

## Try it later

Choose **New Sandbox** in the normal menu. To add the samples near your current position in an existing Adventure checkpoint while preserving that file:

```sh
rebuild/build/planet_explorers --load YOUR_CHECKPOINT.pesave \
  --vegetation-examples --save rebuild/build/vegetation-test.pesave
```

Move to open terrain first if the installer reports insufficient space. The fixed patch is installed only once per world. F9 saves, F10 reloads; there is no automatic save on exit.

A headless fixture can create a separate initial-route checkpoint without opening the desktop:

```sh
make -C rebuild build/vegetation_fixture
rebuild/build/vegetation_fixture rebuild/build/vegetation-check/examples.pesave
rebuild/build/planet_explorers --load rebuild/build/vegetation-check/examples.pesave \
  --save rebuild/build/vegetation-check/manual.pesave
```

The fixture retains the welcome conversation and normal starting inventory. After the introduction, gather each herb, use the Safety Axe on a tree, save/reload and verify depleted objects stay depleted. Also check leaf transparency, plant grounding, selecting nearby NPCs and movement while collecting. These visual/manual checks are pending; headless geometry and asset validation do not establish rendered appearance.

## Verification

Focused commands are `make -C rebuild vegetation-test sandbox-vegetation-host-test vegetation-view-test`; the view test performs CPU mesh/image validation without creating a window. `python3 rebuild/tools/export_vegetation_source.py --check` verifies generated source metadata. The initial vegetation increment passed full optimized/core/content/26 CLI checks and ASan/UBSan with leak detection; the later source-production increment has separate results in [harvesting](SANDBOX_HARVESTING.md). Focused source and installed-asset checks have no skips; the broader Python suite retains51 optional skips. See [PLAYTEST](../PLAYTEST.md) for executed results and remaining physical checks. Runtime assets, fixture saves and logs remain ignored under `rebuild/build`.
