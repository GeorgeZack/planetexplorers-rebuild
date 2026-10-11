# Exporter `--check` coverage

October 7, 2026. Every exporter with a `--check` mode was run on the development machine after the repository split. No game was launched and no reference profile or installed asset was modified.

`make -C rebuild check-content` runs the checks that need only the ordinary interpreter. `make -C rebuild check-sources` runs the rest with the `build/unity-tools` environment: the per-feature `*-source-check` targets plus `exporter-check`, which covers the thirteen exporters that previously had no target.

## Wired in `exporter-check` (all pass)

| Exporter | Invocation |
|---|---|
| `export_actor_materials.py` | `--check` |
| `export_reference_materials.py` | `--check` for both source-bound settlement OBJ material sidecars; see [building materials](BUILDING_MATERIALS.md) |
| `export_adventure_population.py` | `--check` |
| `export_adventure_settlement.py` | `--check` |
| `export_adventure_settlement_assets.py` | `--check` |
| `export_adventure_terrain.py` | `--map-type 1 --check` (artifact-only diagnostic) |
| `export_hare_pounce_assets.py` | `--check` |
| `export_sandbox_world_assets.py` | `--check` |
| `export_torch_assets.py` | `--check` |
| `export_tree_collision.py` | `--check` |
| `export_sword_root_motion.py` | `--output build/reference-assets/player-sword-motion --check` |
| `export_running_knife_motion.py` | retained `root-06` capture, plan and probe; override the capture with `RUNNING_ROOT_CAPTURE=` |

These read local recovered assets, the Steam installation and reference profiles under `~/.local/share`. A failure caused by a missing local input is not a regression in the repository.

## Population labels

`pe_adventure_population.json` recorded the database, the route observation and `NpcCmpt.cs` by absolute checkout path, so its check became stale when the checkout moved. `read_reference_adventure.source_label` now records a repository-relative logical path for anything inside the repository and keeps absolute paths only for external profiles. The regenerated manifest differs only in those labels; the generated header is unchanged.

## Capture baselines exclude autosave slots

`export_adventure_terrain.py --capture` and `export_adventure_route.py --capture` compare the source profile with `source-save-hashes.json` recorded at capture time. Seven baseline files had changed, all under `GameSave/auto0`, which a later original-game session rewrote on October 6; the manual slots were unchanged. By user decision the comparison now ignores `GameSave/autoN/` entries: the documented checkpoints are the manual slots, and the client rewrites autosaves during any session. Manual slots and every other baseline file stay pinned.

A new installation records the number actually verified and `original_autosave_entries_excluded`. The two existing installations recorded the whole baseline as verified, which was true when they were made; `--check` accepts that literal record and nothing installed was rewritten. Any other difference in the manifest still fails.

## Route asset labels

`export_adventure_route_assets.py` now labels in-repository sources by logical path. The installed `route-manifest.json` records the database by the absolute checkout path it was installed from, and its hash is part of the historical run record in `docs/reference/annot-route-rebuild-headless.json`, so it was left untouched. `--check` accepts an installed manifest that differs from the regenerated one only by such an absolute label for the same in-repository file with the same hash; geometry, materials and every other value are compared exactly.

Both are wired into `exporter-check` through `SETTLEMENT_CAPTURE`, `ROUTE_CAPTURE` and `ROUTE_SAVE`.

## Admitted reaction qualification

`repulsed-source-check`, included in `check-sources`, regenerates-and-compares the admitted female Repulsed metadata and runs all17 source/prefab cases with the local UnityPy environment. It additionally binds the focused static angle evidence to the installed UnityEngine bytes. See [the qualification](REPULSED_ACTION.md).

## Not wired

- **`export_shield_controller.py`.** Its check verifies an existing stage directory, and no stage is retained locally. `tests/test_shield_controller_export.py` exercises the mode against a temporary stage.
