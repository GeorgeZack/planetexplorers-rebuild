# Working on Planet Explorers

## Start here

Read [PROJECT_MAP.md](PROJECT_MAP.md) for ownership and navigation, [rebuild/README.md](rebuild/README.md) for commands, and [docs/audit/SANDBOX_ALIGNMENT.md](docs/audit/SANDBOX_ALIGNMENT.md) for the current sandbox work order. [docs/REPLICATION.md](docs/REPLICATION.md) defines replication scope; [docs/PLAYTEST.md](docs/PLAYTEST.md) distinguishes actual observations from source-derived behavior.

This is a C11 rebuild alongside the original Unity 5.2.4f1 source. The main menu connects separate sandbox and tutorial hosts. A feature working in the tutorial is not automatically integrated into the sandbox. The October 5 integration shares recovered UI assets and actors, adds Adventure starter/supply/recipe persistence, mesh collision, shared shovel/pickaxe/axe action clocks and a construction toolbar. The supplier milestone replaces the crate with the archived role2 NPC Eitai and source dialogue; the bounded world and local NPC placement remain provisional. Supplier graphical verification passed; consult the latest playtest record for its bounded scope.

## Boundaries and ownership

- Put new runtime code in `rebuild/src`, exporters/reference tools in `rebuild/tools`, tests in `rebuild/tests`, and evidence/specifications in `docs`. Preserve original `Assets`, `ProjectSettings`, databases and world fixtures as reference inputs unless the user explicitly requests changes to them.
- Preserve the original terms in root `README.md`, original numeric content IDs and asset provenance. Plugins are secondary; the native rebuild must not acquire a Unity/plugin dependency just to implement core gameplay. C or C++ libraries are permitted, while the present runtime is C11.
- Keep simulation and parsing independent of raylib where practical. The Makefile's `CORE` and `FRONTEND` lists define the current boundary. Do not put GPU state, window input or transient job pointers in saved gameplay state.
- Edit an exporter and regenerate its outputs instead of hand-editing `rebuild/generated`. Keep input hashes and source IDs. Use available `--check` modes to verify regeneration.
- Preserve one owner for inventory, equipped instance identity, durability, learned recipes and action clocks. Generalizing tutorial systems must remove lesson grants, fixed spawns, completion flags and cleanup behavior from the general host.
- Adventure, Single Player Build and B-key construction are distinct. Adventure has supplies/recipe progression and resource debit. Tested Single Player Build uses stockpiles, known formulas, `LockStackCount` and a frozen world clock; its availability checks are not equivalent to an unrestricted god-mode bypass.
- Geometry changes must keep render, ray, movement, placement and save-spawn validation consistent. Tool animation and edit/contact events need the same authoritative clock. UI-consumed input must not also attack, dig or build.

## Evidence and verification

Label findings as source-derived, observed in the original, implemented in C, or still provisional. Source code, screenshots and passing C tests establish different things. Record mode, settings, inputs and outcome for reference tests; do not infer exact timing, hidden IDs or durability from a still image.

The general tool clock is `pe_sandbox_actions`; its renderer only reads snapshots. Exact items12/19/26/1527 use separate source timing metadata. Wood Pickaxe19 requires the complete held/stowed/Mine body/tool group; never substitute Spade poses or run the generic mining path for the same input. Live tree capsules now participate in Adventure movement and save-spawn validation through `pe_tree_collision` and the sandbox collision snapshot. The headless constants and visual PESC must be generated from the same source capsule; verify `export_tree_collision.py --check` with UnityPy after source/export changes. Keep camera, movement and tool query masks separate: the inspected original camera excludes tree layers. `pe_sandbox_camera` owns desired distance, velocity and the verified scalar SmoothDamp clock; call it once per display frame, not once per gameplay catch-up tick. The full CameraForge graph remains unported.

Adventure welcome uses archived Westina (entity20008, RandomNpc17, role1), exported with `export_adventure_supplier.py --npc welcome`. It auto-opens source1949→4081..4088 nearby, resumes acknowledged pages and starts Arrival only after4088. Welcome and supplier input must remain modal. Both NPCs use provisional supported local placements; source settlement generation and plot markers remain unported. See [welcome flow](docs/audit/ADVENTURE_WELCOME.md).

The Adventure supplier uses archived Eitai Takayama (entity20016, RandomNpc22, role2), recovered by `export_adventure_supplier.py`; this is one reference appearance, not procedural population recovery. `pe_adventure_npc` owns transient supported placement and the immutable initial arrival anchor, while `pe_adventure_dialogue` owns transient menu/pages. Arrival uses strict XZ distance<5 to the integer-truncated starting anchor. Only the final4094 acknowledgment calls the existing atomic/idempotent `pe_game_accept_supplies`; opening, canceling or advancing earlier pages must not grant items. Keep inventory/progression in `PeGame`, reset transient encounter/dialogue state on load, and preserve source role bindings. Source physical player/NPC layer10 self-collision is disabled; do not add NPC player blockers. See [encounter/query audit](docs/audit/ADVENTURE_NPC.md) and [archived appearance](docs/audit/ADVENTURE_SUPPLIER_APPEARANCE.md). Check generated dialogue with `export_adventure_dialogue.py --check` and source capsule constants with `export_adventure_npc_collider.py --check`; preserve the original archives and existing recovered output directories.

Current rebuild saves are PECS v6. The appended welcome_page (0..9) persists acknowledged source replies; pages0..8 require no active Arrival/supplies state, and page9 enables the existing9031/9032 progression. Versions4/5 Adventure infer page9 to preserve their existing quests; Legacy infers0. Version 4 Adventure loads may use the bounded, geometry-swept tree-overlap migration; version 5 must already satisfy full collision validation. Versions 1–3 retain their Legacy migration. Do not generalize this compatibility relocation into ordinary movement, discard saved velocity/clocks, or commit a failed migration.

Run checks relevant to the change from the repository root:

```sh
make -C rebuild
make -C rebuild test
make -C rebuild sanitize
make -C rebuild check-content
make -C rebuild cli-test
```

Choose the relevant subset; documentation-only changes need link/claim checks, not an automatic full graphics rerun. Some checks use recovered assets under `rebuild/build/reference-assets`, and asset-dependent skips are not passes. Graphics changes need an appropriate smoke/manual check. The full supported tutorial progression uses `make -C rebuild training-building-smoke` (15,000 frames); the NPC survey uses `training-npcs-smoke`. Consult the Makefile and README for smaller fixtures and prerequisites.

On this machine `/tmp` has previously hit a user quota despite apparent free space. For compiler temporary files, create `rebuild/build/compiler-tmp` and pass its absolute path as `TMPDIR`. Diagnose current errors before assuming the same cause. Do not delete unrelated temporary files to make room.

## Original game and desktop testing

Follow [docs/REFERENCE_RUNTIME.md](docs/REFERENCE_RUNTIME.md). The verified reference is the Steam-installed native client through `rebuild/tools/launch_reference.py` with isolated game and engine preferences. Ordinary Steam Play/Proton startup has not been verified fixed. Preserve normal user saves and keep Steam's normal initialization.

The sandbox reference profile is `/home/admin/.local/share/planet-explorers-sandbox-reference`: manual slot 1 is Adventure `audit`, slot 2 is Build `buildaudit`. Preserve these checkpoints or deliberately create another slot/profile. Adventure reload was observed; Build reload was not. The documented `TMPDIR` override addresses a failed temporary SQLite copy on this machine.

Use the bounded `reference_input.py` driver with the actual current PID; never reuse a historical PID from an audit. Only one operator may control the desktop at a time. Original-game launch/capture tools and instrumented pose probes have different side effects; read their documentation before running them. Do not launch graphics or inject desktop input merely to update documentation/indexes.

When a user supplies a stopping deadline, check its date/timezone, stop long-running jobs and agents before it, and leave a checkpoint with completed and outstanding work. The October 3 deadline was session-specific, not a standing daily schedule.

## Repository hygiene and navigation index

The working tree may contain substantial uncommitted/untracked rebuild and documentation work. Inspect status; do not reset, clean, overwrite or discard it. In particular, `make clean` removes `rebuild/build`, including expensive recovered assets, local captures and fixtures. Use targeted cleanup of known disposable outputs when needed.

The local structural graph is an aid to navigation, not semantic authority:

```sh
python3 rebuild/tools/index_project.py build
python3 rebuild/tools/index_project.py find pe_training_dig
python3 rebuild/tools/index_project.py neighbors rebuild/src/pe_game.h --direction in
```

Rebuild after edits before trusting line numbers/relationships. The SQLite output is ignored under `rebuild/build/knowledge`. Binary Unity dependencies, C/C# symbol resolution and runtime call relationships are not decoded. Consult [docs/KNOWLEDGE_GRAPH.md](docs/KNOWLEDGE_GRAPH.md) for coverage and query instructions.

Keep this file focused on durable working rules. Update the project map when modules move, and update the relevant audit/playtest when supported behavior changes. User instructions take precedence over this guidance; routine reversible implementation within the authorized task does not require another permission round.

## Git remotes and attribution

The private rebuild repository is [GeorgeZack/planetexplorers-rebuild](https://github.com/GeorgeZack/planetexplorers-rebuild). Keep `origin` pointing to the original [pathea-games/planetexplorers](https://github.com/pathea-games/planetexplorers) reference; `rebuild` is the writable project remote, with `main` as the rebuild branch. Preserve [NOTICE.md](NOTICE.md) and the original Pathea Games terms in README. Do not commit recovered runtime assets, installed Steam binaries, local player saves or build outputs; source provenance and bounded reference screenshots belong in docs.
