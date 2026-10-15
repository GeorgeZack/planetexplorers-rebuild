# Build and run

Run commands from the repository root. Requires a C11 compiler, make, Python 3 and raylib development files. The [Makefile](Makefile) defines dependencies and targets. Original assets are recovered locally; they are not included in Git.

```sh
mkdir -p rebuild/build/compiler-tmp
TMPDIR="$PWD/rebuild/build/compiler-tmp" make -C rebuild -j"$(nproc)"
./rebuild/build/planet_explorers
```

The menu opens Adventure or Tutorial. Direct entry:

```sh
./rebuild/build/planet_explorers --sandbox --save /tmp/exploration.pesave
./rebuild/build/planet_explorers --load /tmp/exploration.pesave
./rebuild/build/planet_explorers --training-room rebuild/build/reference-assets
./rebuild/build/planet_explorers --help
```

Default runtime assets live beside the executable in `reference-assets`; explicit `--assets` takes precedence. Missing required groups fail closed. Do not delete `rebuild/build`: it also holds recovered assets, earned saves and captures that cannot be regenerated from this repository.

## Sandbox controls

| Input | Action |
|---|---|
| WASD / mouse | Move / look |
| Shift / Space | Sprint / jump |
| Z / middle mouse | Toggle walk/run / autorun; backward input cancels autorun |
| Wheel in third person | Zoom; UI owns its scrolling |
| F5 / Tab | Cycle free third person, captured third person, first person / cursor override; F5 clears the override |
| RMB, free third person | Short stationary release interacts/stows; 3 px motion or .25 s hold captures look |
| LMB, outside construction | Supported equipped tool or Knife action |
| R / RMB, outside interaction | Stow/cancel tool action |
| E / RMB on eligible NPC | Interact |
| 1–9 / keypad in conversation | Select visible response; Enter/arrow advances |
| 1–9 outside panels/construction | Use assigned quickslot |
| I / C / Q / Y | Inventory / character / missions / Replicator |
| Inventory RMB / drag | Use, learn, equip / move, exchange or assign shortcut |
| Inventory Shift+drop | Explicit stack merge; excess stays at source |
| Inventory Split / Sort / Delete | Split amount / consolidate and order category / confirmed whole-slot discard |
| Enter / Escape in edits | Confirm / cancel; Delete mode also exits with RMB |
| PageUp / PageDown, inventory arrows/wheel | Change inventory page |
| Quickslot RMB / drag | Use / rearrange |
| Character weapon RMB | Unequip |
| B | Construction mode |
| LMB / Alt+LMB or X, construction | Place / remove |
| Held RMB, construction | Turn camera |
| 1–7, construction | Cube / dirt / stone / wood / ramp / fence / ladder |
| V / T / G | Cube or ramp / rotate quarter turn / one or four cells |
| Ctrl+Z / Ctrl+X, construction | Undo / redo; toolbar provides the same commands |
| F9 / F10 | Save / load configured file |
| Escape | Close panel or pause/resume |

Panels block movement while simulation/crafting continue; Pause freezes simulation. Consumable shortcuts remain nonmodal. UI owns pointer input through release, including a panel's closing frame. Supported Knife branches are female grounded third person; Tutorial keeps its stationary attack adapter. Normal Sword/Shield use remains gated.

## Validation

| Stage | Command |
|---|---|
| Focused iteration | `python3 rebuild/tools/run_tests.py -k NAME` or `rebuild/build/test_core NAME` |
| Every code commit | `make -C rebuild check` |
| Before push; memory ownership changes | `make -C rebuild check-sanitize` |
| Exporter/generated/shared source-list changes | `make -C rebuild check-content check-sources` |
| CLI changes | `make -C rebuild cli-test` |
| Shared UI changes | `make -C rebuild ui-shared-test sandbox-ui-test` |
| Shared UI visuals | `make -C rebuild ui-shared-render-capture`; inspect both modes at normal/small sizes |
| Full supported Tutorial | `make -C rebuild training-building-smoke` (15,000 frames, mission 756 boundary) |
| Tutorial NPC survey | `make -C rebuild training-npcs-smoke` |

Set `TMPDIR` as above for compiler work. `check` runs the full registry in parallel and reports all failures/skips; `check-sanitize` gives each core suite its own leak-scanned ASan/UBSan process. Optional asset skips are not passes. Register core suites in `tests/test_core_list.h`; link `libpe_core.a`, not another CORE source list. Documentation-only work needs link/claim checks. Graphics fixtures do not establish earned progress or original parity.

## Content recovery

```sh
make -C rebuild unity-tools
make -C rebuild content
make -C rebuild check-content
make -C rebuild check-sources
```

UnityPy tools use `rebuild/build/unity-tools/bin/python`. Run an exporter's `--help` for its current arguments and `--check` where supported. [Exporter checks](../docs/audit/EXPORTER_CHECKS.md) lists regeneration coverage and install-time gates.

Generate assets from the read-only reference inputs; never hand-edit `generated` or recovered groups. Stage and validate complete groups, preserve newer clips, back up installations and retain source hashes. Old single-leaf/copy recipes must not overwrite a newer installed body/equipment/defence group.

| Recovery area | Entry points and contract |
|---|---|
| Room and static tools | `export_reference_assets.py`; [asset recovery](../docs/audit/ASSET_RECOVERY.md) |
| Actor meshes/rigs | `export_reference_actor.py`, `export_reference_player.py`; [actor recovery](../docs/audit/ACTOR_RECOVERY.md) |
| Player clocks/poses/equipment | `reference_animator_bake.py`, `export_directional_player.py`, complete group exporters; [bake contract](../docs/audit/ANIMATOR_BAKE.md) |
| Source-bound materials | `export_actor_materials.py`, `export_reference_materials.py`; [materials](../docs/audit/ACTOR_MATERIALS.md) |
| Terrain textures | `export_terrain_assets.py`; [terrain materials](../docs/audit/SANDBOX_TERRAIN_MATERIALS.md) |
| Settlement and route | `export_adventure_settlement_assets.py`, `export_adventure_route.py`, `export_adventure_route_assets.py`; [route](../docs/audit/ADVENTURE_ROUTE.md) |
| Residents and dialogue | `export_adventure_population.py`, `export_adventure_supplier.py`, `export_adventure_dialogue.py`; [population](../docs/audit/ADVENTURE_POPULATION.md) |
| Tutorial UI/herbs/building | `export_training_ui.py`, `export_training_gathering_assets.py`, `export_training_build_assets.py`; matching audits |
| Trees, plants and collision | `export_sandbox_world_assets.py`, `export_tree_collision.py`, `export_harvest.py`; [harvesting](../docs/audit/SANDBOX_HARVESTING.md) |

Route installation requires complete observed chunks and source town archives. Partial coverage inspection is read-only:

```sh
python3 rebuild/tools/export_adventure_route.py --capture /absolute/capture.pevx --coverage
```

Original terrain crops can be imported with `--terrain PATH`; see CLI help and [terrain audit](../docs/audit/TERRAIN_PARITY.md). New sessions prefer the installed 416×64×416 route; old saves retain their worlds.

## Original game reference

Read [reference runtime](../docs/REFERENCE_RUNTIME.md) before launching. `launch_reference.py` prepares isolated settings and retains Steam initialization. The pose observer and isolated pose-bake tools have different side effects: read [pose capture](tools/REFERENCE_POSE.md) and [bake contract](../docs/audit/ANIMATOR_BAKE.md).

For original desktop input, use `reference_input.py --pid CURRENT_PID ...`; never reuse a historical PID. Only one operator controls the desktop. Native acceptance can use the owned private display via [native private input](../docs/audit/NATIVE_PRIVATE_INPUT.md); never adopt an unrelated display or use Xvfb `-displayfd`.

## Navigation

[Project map](../PROJECT_MAP.md) locates owners. [Feature contracts](../docs/FEATURE_CONTRACTS.md) define current behavior, [PLAYTEST](../docs/PLAYTEST.md) bounds acceptance, and [HANDOFF](../docs/HANDOFF.md) sets next work. The [replication scope](../docs/REPLICATION.md) lists supported and incomplete systems. Makefile targets and tool `--help` replace historical command transcripts.
