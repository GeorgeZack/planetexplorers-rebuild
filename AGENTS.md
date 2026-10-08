# Working on Planet Explorers

This is a C11 rebuild of Planet Explorers, developed against the original Unity5.2.4f1 source in the read-only `reference/unity` submodule. Menu-linked sandbox and tutorial hosts share recovered presentation; tutorial lesson behavior never automatically transfers to the sandbox.

Read [PROJECT_MAP.md](PROJECT_MAP.md) for ownership and navigation, [rebuild/README.md](rebuild/README.md) for commands, and [docs/audit/SANDBOX_ALIGNMENT.md](docs/audit/SANDBOX_ALIGNMENT.md) for the current sandbox work order. [docs/REPLICATION.md](docs/REPLICATION.md) defines replication scope; [docs/PLAYTEST.md](docs/PLAYTEST.md) distinguishes actual observations from source-derived behavior.

## How to work

Follow the [current source-first checkpoint audit](docs/audit/SOURCE_FIRST_CHECKPOINT_REVIEW.md) and [evidence workflow](docs/audit/SOURCE_FIRST_REBUILD.md): recover active C# call chains, database/XML/prefab contracts and existing archived state before planning another original-game session. Before a launch, identify the unresolved question, why available source/captures cannot answer it, and the smallest useful capture; batch compatible questions and reuse qualified poses. Engine-evaluated animation and final visual/integration checks remain valid runtime needs. This is an evidence workflow, not an extra user approval requirement. Keep the unqualified shield animation work separate from source-portable gameplay fixes.

Label findings as source-derived, observed in the original, implemented in C, or still provisional. Source code, screenshots and passing C tests establish different things. Record mode, settings, inputs and outcome for reference tests; do not infer exact timing, hidden IDs or durability from a still image.

Per-feature rules live in [docs/FEATURE_CONTRACTS.md](docs/FEATURE_CONTRACTS.md), not in this file. They are binding: read the section for an area before changing it, and update it with the linked audit when supported behavior changes.

| Before changing | Owners | Read |
|---|---|---|
| Saves, migrations, anything persisted | `pe_save`, `PeGame` | [Save format](docs/FEATURE_CONTRACTS.md#save-format) |
| Player/NPC rendering, materials, locomotion clips, body facing | `pe_actor*`, `pe_sandbox_actor`, `pe_player_locomotion` | [Actor presentation and locomotion](docs/FEATURE_CONTRACTS.md#actor-presentation-and-locomotion) |
| Camera collision, activity, projection | `pe_camera_geometry`, `pe_camera_query`, `pe_camera_projection`, `pe_sandbox_camera` | [Camera](docs/FEATURE_CONTRACTS.md#camera) |
| Tool actions, tree collision, F5 camera modes, RMB gesture | `pe_sandbox_actions`, `pe_tree_collision` | [Tools, trees and camera modes](docs/FEATURE_CONTRACTS.md#tools-trees-and-camera-modes) |
| Knife/sword attacks, root motion, facing, contact playback, blade history | `pe_sandbox_melee`, `pe_attack_playback`, `pe_melee_facing`, `pe_capsule_history` | [Melee, playback and capsule history](docs/FEATURE_CONTRACTS.md#melee-playback-and-capsule-history) |
| Hare hunt, drops, death and revival | `pe_hunt_combat`, `pe_hunt_loot`, `pe_revival` | [Hunt encounter, loot and revival](docs/FEATURE_CONTRACTS.md#hunt-encounter-loot-and-revival) |
| Clothing and sword durability | `pe_outfit`, `pe_sword` | [Equipment wear: outfit and sword](docs/FEATURE_CONTRACTS.md#equipment-wear-outfit-and-sword) |
| HP/stamina recovery, bandage, medpack, juice, quickslot input | `pe_passive_recovery`, `pe_stamina_recovery` | [Health, stamina and consumables](docs/FEATURE_CONTRACTS.md#health-stamina-and-consumables) |
| NPCs, dialogue, shops, wallets, medicine and hunt quests | `pe_adventure_*`, `pe_town_*` | [Welcome, supplier, population, town and quests](docs/FEATURE_CONTRACTS.md#welcome-supplier-population-town-and-quests) |
| Quest events 9077–9084, Town Defense | `pe_adventure_mission_events`, `pe_town_defense_clock` | [Disconnected foundations: mission events and Town Defense](docs/FEATURE_CONTRACTS.md#disconnected-foundations-mission-events-and-town-defense) |
| Bag operations, discard, build undo/redo, repair machine | `pe_inventory`, `pe_build_history`, `pe_camp_repair*` | [Inventory, construction and camp repair](docs/FEATURE_CONTRACTS.md#inventory-construction-and-camp-repair) |
| Torch, plants, tree felling and yields | `pe_deployable`, `pe_vegetation`, `pe_harvest` | [Deployables, vegetation and harvesting](docs/FEATURE_CONTRACTS.md#deployables-vegetation-and-harvesting) |
| Settlement crop, route terrain, world dimensions | `pe_settlement`, `pe_world` | [Settlement and route](docs/FEATURE_CONTRACTS.md#settlement-and-route) |
| Debug cheats | `pe_debug`, `pe_ui_settings` | [Debug tools](docs/FEATURE_CONTRACTS.md#debug-tools) |
| What earlier increments verified and did not | — | [Project status and increment history](docs/FEATURE_CONTRACTS.md#project-status-and-increment-history) |

## Standing decisions and closed gates

These recur across features. The contract sections hold the detail; do not reverse any of them without a new user instruction.

- **Native adapters are labelled, not parity.** Where the rebuild substitutes its own scheduling, clocks, placement or arbitration, the contract calls it an adapter. Never describe an adapter as source or original behavior, and never widen a claim from headless checks to physical, visual or difficulty acceptance.
- **User-selected differences from the original stay.** Medicine delivery commits on the final 4100 acknowledgment, not the original's earlier 4099 selection. Numbered `1`–`9` conversation responses, the debug cheats (session-only, default OFF), the `camera_facing_attacks` preference (default OFF) and the fixed vegetation example patch are requested native additions. Capture baselines ignore the original client's autosave slots.
- **Closed gates stay closed.** Normal Sword/Shield readiness and shield input, Town Defense 9077 and the 9077–9084 mission-event reducer, hare pounce or added hunt aggression, and hit-reaction stun/knockback are disconnected foundations. Tables, clocks, staged assets or passing tests for them are not permission to enable gameplay; the native quest flow deliberately stops before 9077.
- **Saves are append-only.** The current format is PECSv22. Each version preserves the previous literal payload and appends a fixed suffix; older saves infer documented defaults and never fabricate history, wear or progress. Malformed input fails without replacing the live game. Never serialize transient state: action clocks, camera, capsule history, buyback pools, build history, modal or GPU state.
- **Transactions are atomic and identities exact.** Inventory, wear, HP, rewards and quest state commit together or not at all; a capacity failure stays retryable. Items are exact instances with their own durability: never recreate one from a prototype, alias one across bag, equipment, outfit, deployment, repair or quickslots, or grant rewards twice.
- **Recovered assets are never patched by hand.** Regenerate through the exporter, stage complete groups, back up what an installation replaces, and never overwrite a group with an older export. Missing optional metadata keeps the documented legacy adapter; malformed present metadata fails closed.

## Boundaries and ownership

- Common UI has one implementation: `pe_ui_draw` owns text/frame/item primitives, `pe_ui_hud` owns normal HUD/minimap chrome/quest tracker, `pe_ui_history` and its pure state own history presentation/scroll dragging, `pe_training_ui_layout` owns geometry, and `pe_ui_input` owns chrome shortcuts and held/release pointer admission. Tutorial and sandbox supply read-only presentation snapshots; never fork these visuals or wrapping algorithms into a host. Keep gameplay/transactions mode-owned, draw and hit-test the same layout fields, and connect every common change to both hosts. Run `make -C rebuild ui-shared-test sandbox-ui-test` and, for visual changes, `make -C rebuild ui-shared-render-capture`; inspect both modes at normal and small sizes. Specialized content may differ only for supported mode behavior. See [shared UI contract](docs/audit/SHARED_UI.md).
- Put new runtime code in `rebuild/src`, exporters/reference tools in `rebuild/tools`, tests in `rebuild/tests`, and evidence/specifications in `docs`. The original `Assets`, `ProjectSettings`, databases and world fixtures live in the `reference/unity` submodule and are read-only reference inputs.
- Preserve the original terms in root `README.md`, original numeric content IDs and asset provenance. Plugins are secondary; the native rebuild must not acquire a Unity/plugin dependency just to implement core gameplay. C or C++ libraries are permitted, while the present runtime is C11.
- Worlds with nonlegacy dimensions own heap storage. Use `pe_world_clone/move/destroy` and `pe_game_clone/move/destroy`; never shallow-copy an owning object. Destinations must be zeroed/initialized, and an existing object must be destroyed before fresh initialization. Use dimension/data accessors rather than direct legacy-array indexing in general hosts.
- Keep simulation and parsing independent of raylib where practical. The Makefile's `CORE` and `FRONTEND` lists define the current boundary. Do not put GPU state, window input or transient job pointers in saved gameplay state.
- Edit an exporter and regenerate its outputs instead of hand-editing `rebuild/generated`. Keep input hashes and source IDs. Use available `--check` modes to verify regeneration.
- Preserve one owner for inventory, equipped instance identity, durability, learned recipes and action clocks. Generalizing tutorial systems must remove lesson grants, fixed spawns, completion flags and cleanup behavior from the general host.
- Adventure, Single Player Build and B-key construction are distinct. Adventure has supplies/recipe progression and resource debit. Tested Single Player Build uses stockpiles, known formulas, `LockStackCount` and a frozen world clock; its availability checks are not equivalent to an unrestricted god-mode bypass.
- Geometry changes must keep render, ray, movement, placement and save-spawn validation consistent. Tool animation and edit/contact events need the same authoritative clock. UI-consumed input must not also attack, dig or build. A modal owns its pointer press through all held frames and release, even if the modal closes immediately; retain that ownership and defer gameplay pointer capture until all buttons are released. Only a fresh world press may start an action.

## Evidence and verification

Match the checks to the stage of work, run from the repository root. Skipping the later tiers while iterating is expected; skipping the commit tier is not.

| When | Run | Typical time |
|---|---|---|
| While iterating on a feature | `python3 rebuild/tools/run_tests.py -k NAME` for the suites being touched, or `rebuild/build/test_core NAME` | seconds |
| Before every commit | `make -C rebuild check` | 2–3 min |
| Before pushing, or after changing C code that allocates, copies or owns memory | `make -C rebuild check-sanitize` | ~4 min |
| After changing an exporter, a shared source list or anything under `rebuild/generated` | `make -C rebuild check-content` and `make -C rebuild check-sources` | up to ~5 min |
| After changing command-line handling | `make -C rebuild cli-test` | seconds |
| When a feature is otherwise finished | graphics smoke, private-display acceptance and original-game comparison, as its contract requires | long, partly manual |

The full `check` before each commit is the fixed rule: a failing or unrun suite must not become the baseline the next session builds on. If it fails, fix it or report it with the output before committing; never commit around it. Documentation-only changes need link and claim checks instead, not a test run. Build with `make -C rebuild -j"$(nproc)"`.

`check` is the parallel form of `make test` ([run_tests.py](rebuild/tools/run_tests.py)): it reports every failure and the skip total instead of stopping at the first error; `-k NAME` or `build/test_core NAME` runs one suite. `check-sanitize` runs the same registry under ASan/UBSan with one leak-scanned process per suite; `make sanitize` remains the single-process form. Sources compile once into `build/obj` and binaries link `libpe_core.a`, so never reintroduce `$(CORE)` source lists into a test rule. Register a core suite with one `PE_TEST(name)` line in `tests/test_core_list.h`. UnityPy checks use `build/unity-tools` (`make -C rebuild unity-tools`) when present; after changing an exporter or a shared `SOURCE_FILES` list, run `make -C rebuild check-sources`, which regenerates-and-compares every UnityPy-backed output that `check-content` skips; [exporter checks](docs/audit/EXPORTER_CHECKS.md) lists what it covers and the install-time gates it cannot. Label in-repository sources with `read_reference_adventure.source_label`, never an absolute checkout path.

Some checks use recovered assets under `rebuild/build/reference-assets`, and asset-dependent skips are not passes. Graphics changes need an appropriate smoke/manual check. The full supported tutorial progression uses `make -C rebuild training-building-smoke` (15,000 frames); the NPC survey uses `training-npcs-smoke`. Consult the Makefile and README for smaller fixtures and prerequisites.

On this machine `/tmp` has previously hit a user quota despite apparent free space. For compiler temporary files, create `rebuild/build/compiler-tmp` and pass its absolute path as `TMPDIR`. Diagnose current errors before assuming the same cause. Do not delete unrelated temporary files to make room.

## Original game and desktop testing

Follow [docs/REFERENCE_RUNTIME.md](docs/REFERENCE_RUNTIME.md). The verified reference is the Steam-installed native client through `rebuild/tools/launch_reference.py` with isolated game and engine preferences. Ordinary Steam Play/Proton startup has not been verified fixed. Preserve normal user saves and keep Steam's normal initialization.

The sandbox reference profile is `/home/admin/.local/share/planet-explorers-sandbox-reference`: manual slot 1 is Adventure `audit`, slot 2 is Build `buildaudit`. Preserve these checkpoints or deliberately create another slot/profile. Adventure reload was observed; Build reload was not. The documented `TMPDIR` override addresses a failed temporary SQLite copy on this machine.

Use the bounded `reference_input.py` driver with the actual current PID; never reuse a historical PID from an audit. Only one operator may control the desktop at a time. Native acceptance may instead use [native_private_input.py](rebuild/tools/native_private_input.py) on its newly owned private Xvfb display; follow [its contract](docs/audit/NATIVE_PRIVATE_INPUT.md), preserve baseline hashes, use nonwarping button input in captured-camera mode and retain the supervisor. Never adopt an unrelated display or use smoke fixtures to claim earned progress. Never use Xvfb `-displayfd` autodiscovery: it previously replaced the live Xwayland X0 socket. Both private tools must share explicit high-display selection, occupied socket/lock exclusion and exact child-listener verification; see [the incident](docs/audit/PRIVATE_DISPLAY_COLLISION.md). Original-game launch/capture tools and instrumented pose probes have different side effects; read their documentation before running them. Do not launch graphics or inject desktop input merely to update documentation/indexes.

When a user supplies a stopping deadline, check its date/timezone, stop long-running jobs and agents before it, and leave a checkpoint with completed and outstanding work. The October 3 deadline was session-specific, not a standing daily schedule.

## Repository hygiene and navigation index

The working tree may contain substantial uncommitted/untracked rebuild and documentation work. Inspect status; do not reset, clean, overwrite or discard it. `make clean` removes only objects, test logs and the binaries the Makefile defines. Everything else under `rebuild/build` is recovered assets, staged groups, fixtures and captured evidence that this repository cannot regenerate: never delete `rebuild/build` itself, and use targeted cleanup of known disposable outputs when needed.

The local structural graph is an aid to navigation, not semantic authority:

```sh
python3 rebuild/tools/index_project.py build
python3 rebuild/tools/index_project.py find pe_training_dig
python3 rebuild/tools/index_project.py neighbors rebuild/src/pe_game.h --direction in
```

Rebuild after edits before trusting line numbers/relationships. The SQLite output is ignored under `rebuild/build/knowledge`. Binary Unity dependencies, C/C# symbol resolution and runtime call relationships are not decoded. Consult [docs/KNOWLEDGE_GRAPH.md](docs/KNOWLEDGE_GRAPH.md) for coverage and query instructions.

Keep this file focused on durable working rules. Update the project map when modules move, and update the relevant audit/playtest when supported behavior changes. User instructions take precedence over this guidance; routine reversible implementation within the authorized task does not require another permission round.

## Git remotes and attribution

The private rebuild repository is [GeorgeZack/planetexplorers-rebuild](https://github.com/GeorgeZack/planetexplorers-rebuild). It contains only the rebuild, its tools and its evidence; `origin` is that writable remote and `master` its default branch. The original [pathea-games/planetexplorers](https://github.com/pathea-games/planetexplorers) Unity project is the read-only `reference/unity` submodule, pinned to the commit the recorded source hashes were taken from (`git submodule update --init` after cloning). Root links (`Assets`, `ProjectSettings`, `CustomGames`, `TutorialMode`, `RandomTownArt`, `ConfigFiles`, `CustomSounds`, `config`, `i18n.db`) point into it so documented source paths stay stable. Never edit, commit inside or re-pin the submodule without an explicit request, and never copy its files into this repository. Record sources by their logical path (`Assets/...`), not the resolved `reference/unity/...` location: use `os.path.abspath`, not `Path.resolve()`, before relativizing a source path. Preserve [NOTICE.md](NOTICE.md) and the original Pathea Games terms in README. Do not commit recovered runtime assets, installed Steam binaries, local player saves or build outputs; source provenance and bounded reference screenshots belong in docs.
