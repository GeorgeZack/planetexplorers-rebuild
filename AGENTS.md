# Working on Planet Explorers

C11 rebuild against the pinned, read-only Unity 5.2.4f1 source in `reference/unity`. Sandbox and Tutorial share presentation; lesson behavior never transfers automatically.

Read [PROJECT_MAP.md](PROJECT_MAP.md), [build commands](rebuild/README.md), [current handoff](docs/HANDOFF.md) and [sandbox work order](docs/audit/SANDBOX_ALIGNMENT.md). Before editing an area, read its binding section in [FEATURE_CONTRACTS](docs/FEATURE_CONTRACTS.md); update that section and its audit when behavior changes. [REPLICATION](docs/REPLICATION.md) owns scope; [PLAYTEST](docs/PLAYTEST.md) owns observations.

## Communication and evidence

Use the local [Caveman skill](docs/skills/caveman/SKILL.md) for terse chat by default. “Stop caveman” or “normal mode” disables it. Persisted code, comments, docs and commits use normal prose.

Follow [source-first evidence workflow](docs/audit/SOURCE_FIRST_REBUILD.md): inspect active C# call chains, database/XML/prefab contracts and existing archives before planning original-game sessions. Before a launch, state the unresolved question, why source/captures cannot answer it and the smallest useful capture. Batch compatible questions; reuse qualified poses. Engine-evaluated animation and final integration remain valid runtime needs. This is not an approval requirement.

Label claims as source-derived, observed in the original, implemented in C or provisional. Record mode, settings, inputs and outcome. Screenshots, source and C tests establish different things; never infer hidden IDs, exact timing or durability from a still. Native scheduling, placement and arbitration substitutions must be labelled adapters. Headless results do not establish visual, physical or difficulty acceptance. Keep unqualified shield animation separate from portable gameplay fixes.

## Standing decisions

- Keep user-selected differences: medicine commits on final 4100 acknowledgment rather than original 4099 selection; numbered conversation responses; session-only debug cheats default OFF; `camera_facing_attacks` default OFF; fixed vegetation examples. Ignore original autosave slots in capture baselines.
- Keep normal Sword/Shield readiness/input, Town Defense 9077–9084, hare pounce/aggression and hit-reaction stun/knockback disconnected. Assets, clocks or passing tests do not enable gameplay. Native quests stop before 9077.
- Saves are append-only PECSv23. Preserve earlier literal payloads and documented defaults; never fabricate history/wear/progress. Malformed loads leave live state unchanged. Never save action clocks, camera, capsule/buyback/build histories, modal/GPU state or transient job pointers.
- Transactions commit inventory, wear, HP, rewards and quest state together. Capacity failures stay retryable; rewards never duplicate. Exact item instances retain durability and cannot alias bag/equipment/outfit/deployment/repair/quickslots. Never reconstruct an owned item from its prototype.
- Regenerate recovered assets through exporters. Stage complete groups, retain newer exports, back up installations. Missing optional metadata uses the documented adapter; malformed present metadata fails closed.

## Ownership

Runtime belongs in `rebuild/src`, tools in `rebuild/tools`, tests in `rebuild/tests`, evidence/specifications in `docs`. Never edit `reference/unity`, its root asset links or the submodule pin without explicit permission. Preserve source IDs, logical `Assets/...` labels, hashes, [NOTICE](NOTICE.md) and Pathea's README terms. Native core gameplay must not require Unity/plugins; C/C++ libraries are permitted.

- Shared UI has one owner: `pe_ui_draw` primitives/wrapping, `pe_ui_hud` HUD/minimap/tracker, `pe_ui_history` history, `pe_training_ui_layout` geometry, `pe_ui_input` shortcuts/pointer admission. Hosts supply read-only snapshots and own transactions. Draw/hit-test identical layout fields; connect common changes to both hosts. See [shared UI](docs/audit/SHARED_UI.md).
- UI-consumed input cannot also attack/dig/build. A modal owns its press through held frames and release, even after closing. Delay gameplay capture until all buttons release; only a fresh world press starts an action.
- Nonlegacy worlds own heap storage. Use world/game clone/move/destroy; never shallow-copy owners. Initialize destinations and destroy an existing owner before reinitializing. General hosts use dimension/data accessors rather than legacy arrays.
- Keep simulation/parsing independent of raylib where practical; Makefile CORE/FRONTEND defines the boundary. Preserve one owner for inventory, equipment identity, wear, recipes and action clocks.
- Geometry changes keep rendering, rays, movement, placement and save-spawn validation consistent. Tools and contact events use one authoritative clock.
- Generalizing Tutorial systems removes lesson grants, fixed spawns, flags and cleanup from general hosts. Adventure, Single Player Build and B-key construction differ: Adventure debits resources/progression; tested Build uses stockpiles, formulas, LockStackCount and frozen time, not unrestricted god mode.
- Edit exporters and regenerate `rebuild/generated`; retain hashes/IDs and use `--check`. Source labels use `read_reference_adventure.source_label`, never absolute checkout paths. Use `os.path.abspath`, not symlink-resolving `Path.resolve`, when deriving logical labels.

## Verification

Run from repository root. Build with `make -C rebuild -j"$(nproc)"`. Compiler temp directory: create `rebuild/build/compiler-tmp` and set `TMPDIR` to its absolute path. `/tmp` has hit user quota; diagnose current errors and never delete unrelated temporary files.

| Stage | Required proof |
|---|---|
| Iteration | `python3 rebuild/tools/run_tests.py -k NAME` or `rebuild/build/test_core NAME` |
| Every code commit | `make -C rebuild check` |
| Before push, or memory allocation/copy/ownership edits | `make -C rebuild check-sanitize` |
| Exporter, shared source-list or generated edits | `make -C rebuild check-content` and `make -C rebuild check-sources` |
| CLI edits | `make -C rebuild cli-test` |
| Shared UI edits | `make -C rebuild ui-shared-test sandbox-ui-test` |
| Shared visuals | `make -C rebuild ui-shared-render-capture`; inspect both modes, normal/small sizes |
| Documentation only | Link and claim checks; no full test run required |
| Finished feature | Its contract's graphics/private-display/original comparison |

Full `check` before code commits is fixed: fix failures or report output before committing; never commit around failing/unrun suites. It reports all failures and skips. Asset skips are not passes. Sanitizers run a leak-scanned process per core suite; `make sanitize` remains single-process. Register core suites with one `PE_TEST(name)` in `tests/test_core_list.h`; tests link `libpe_core.a`, never a duplicate CORE source list.

UnityPy checks use `rebuild/build/unity-tools` (`make -C rebuild unity-tools`). [Exporter checks](docs/audit/EXPORTER_CHECKS.md) distinguishes content/source/install gates. Full Tutorial: `training-building-smoke` (15,000 frames); NPC survey: `training-npcs-smoke`. Consult Makefile prerequisites.

## Original game and desktop

Read [REFERENCE_RUNTIME](docs/REFERENCE_RUNTIME.md). Verified reference is the Steam-installed native client via `launch_reference.py`, isolated game/engine preferences and normal Steam initialization. Ordinary Steam Play/Proton startup is unverified.

Sandbox profile: `/home/admin/.local/share/planet-explorers-sandbox-reference`; manual slot 1 Adventure `audit`, slot 2 Build `buildaudit`. Preserve checkpoints or deliberately create another slot/profile. Adventure reload was observed; Build reload was not. Use documented TMPDIR override for the temporary SQLite-copy issue.

Only one desktop operator. Use bounded `reference_input.py` with the actual current PID, never a historical PID. Native acceptance may use `native_private_input.py` on a newly owned private Xvfb display: follow [its contract](docs/audit/NATIVE_PRIVATE_INPUT.md), retain supervisor/baseline hashes and use nonwarping captured-camera buttons. Never adopt unrelated displays or treat smoke progress as earned. Never use Xvfb `-displayfd`: it replaced live X0 before. Both private tools require explicit high display selection, occupied socket/lock exclusion and exact child-listener verification ([incident](docs/audit/PRIVATE_DISPLAY_COLLISION.md)).

Launch/capture tools and instrumented pose probes have different side effects; read their docs first. Never launch graphics or inject input to update documentation/indexes. User deadlines require checking date/timezone, stopping owned jobs/agents beforehand and leaving a checkpoint. The October 3 deadline was session-specific.

## Hygiene and navigation

Inspect Git status; preserve existing tracked/untracked work. Never reset, clean, overwrite or discard it. `make clean` removes defined disposable objects/logs/binaries only. Never delete `rebuild/build` itself: ignored recovered evidence, assets and saves are not regenerable.

The [structural index](docs/KNOWLEDGE_GRAPH.md) is navigation, not semantic evidence:

```sh
python3 rebuild/tools/index_project.py build
python3 rebuild/tools/index_project.py find pe_training_dig
python3 rebuild/tools/index_project.py neighbors rebuild/src/pe_game.h --direction in
```

Rebuild after edits before trusting lines/edges. Ignored SQLite output lives under `rebuild/build/knowledge`; binary Unity dependencies and C/C# calls are not resolved. Update PROJECT_MAP when owners move. Routine authorized reversible work needs no extra approval.

## Git and attribution

Writable `origin`: private [GeorgeZack/planetexplorers-rebuild](https://github.com/GeorgeZack/planetexplorers-rebuild), default branch `master`. Read-only original: pinned [pathea-games/planetexplorers](https://github.com/pathea-games/planetexplorers) submodule; initialize with `git submodule update --init`. Never commit inside, repin or copy original files into this repository without explicit instruction.

Do not commit recovered runtime assets, Steam binaries, local saves or build outputs. Bounded screenshots and source provenance belong in docs. Preserve original terms and NOTICE.
