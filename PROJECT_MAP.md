# Planet Explorers project map

C11 runtime in `rebuild`; pinned Unity 5.2.4f1 reference in read-only `reference/unity`. Sandbox and Tutorial share presentation, not lesson behavior.

| Need | Owner |
|---|---|
| Working rules | [AGENTS.md](AGENTS.md) |
| Binding behavior and compatibility | [Feature contracts](docs/FEATURE_CONTRACTS.md) |
| Commands and controls | [Build guide](rebuild/README.md) |
| Current goal and next gates | [Handoff](docs/HANDOFF.md) |
| Replication scope | [Specification](docs/REPLICATION.md) |
| Observations and acceptance limits | [Playtest](docs/PLAYTEST.md) |
| Original launch and profile safety | [Reference runtime](docs/REFERENCE_RUNTIME.md) |
| Local dependency navigation | [Structural index](docs/KNOWLEDGE_GRAPH.md) |

## Directory map

| Path | Purpose |
|---|---|
| `rebuild/src` | Runtime; Makefile `CORE` and `FRONTEND` define the CPU/graphics boundary |
| `rebuild/generated` | Exported source tables; regenerate, never hand-edit |
| `rebuild/tools` | Exporters, reference probes, bounded input and analysis |
| `rebuild/tests` | Registered core suites, Python checks and graphics fixtures |
| `rebuild/build` | Ignored assets, captures, staged groups, saves and disposable binaries; never delete wholesale |
| `docs/audit` | Feature evidence and source contracts |
| `docs/reference` | Committed bounded evidence, screenshots and provenance |
| `reference/unity` | Read-only original source; root asset links point here |

## Runtime ownership

Read the matching [feature contract](docs/FEATURE_CONTRACTS.md) before editing. Find exact files with `rg --files rebuild/src` or the structural index.

| Area | Owners |
|---|---|
| Application and hosts | `main`, `pe_app_menu`, `pe_game`, `pe_sandbox_view`, `pe_training_view` |
| Terrain and geometry | `pe_world`, `pe_mesh`, `pe_legacy`, `pe_block`, `pe_scene_collision`, `pe_sandbox_collision`, `pe_tree_collision` |
| World storage and persistence | `pe_save`, `pe_world_clone/move/destroy`, `pe_game_clone/move/destroy` |
| Archived settlement and route | `pe_settlement`, `pe_adventure_population`, `pe_adventure_npc` |
| Inventory and equipment | `pe_inventory`, `pe_outfit`, `pe_sword`, `pe_deployable`, `pe_camp_repair` |
| Construction history | `pe_build_history`; Tutorial retains separate lesson owners |
| NPCs, shops and quests | `pe_adventure_*`, `pe_town_interaction`, `pe_town_wallet`, `pe_town_trade` |
| Recovery, food and revival | `pe_passive_recovery`, `pe_stamina_recovery`, `pe_hunger`, `pe_revival` |
| Tools and vegetation | `pe_sandbox_actions`, `pe_harvest`, `pe_vegetation` |
| Blade actions and contacts | `pe_sandbox_melee`, `pe_attack_playback`, `pe_melee_facing`, `pe_capsule_history`, `pe_sword_motor` |
| Bounded hare encounter | `pe_hunt_combat`, `pe_hare_skill`, `pe_hunt_loot`, `pe_hunt_loot_combat` |
| Actors and locomotion | `pe_actor*`, `pe_sandbox_actor`, `pe_player_locomotion` |
| Presentation IK | `pe_ik_pose`, `pe_ik_drive`, `pe_grounder_host`, `pe_halt_host` |
| Camera | `pe_camera_geometry`, `pe_camera_query`, `pe_camera_projection`, `pe_sandbox_camera` |
| Shared UI | `pe_ui_draw`, `pe_ui_hud`, `pe_ui_history`, `pe_ui_input`, `pe_training_ui_layout` |
| Mode UI adapters | `pe_sandbox_ui*`, `pe_training_ui*`; transactions stay mode-owned |
| Settings and cheats | `pe_preferences`, `pe_ui_settings`, `pe_debug` |
| Disconnected foundations | `pe_shield_action`, `pe_hit_reaction`, `pe_repulsed`, `pe_whacked`, `pe_animfollow*`, `pe_ragdoll_action`, `pe_adventure_mission_events`, `pe_town_defense*` |

## Original source landmarks

`Assets/Scripts` holds gameplay; `Assets` also holds prefabs, controllers and database inputs. `ProjectSettings` records engine/physics settings. `CustomGames`, `TutorialMode`, `RandomTownArt`, `ConfigFiles`, `CustomSounds`, `config` and `i18n.db` are logical links into the reference submodule. Exporters retain these logical labels and exact hashes.

The [Makefile](rebuild/Makefile), [contracts](docs/FEATURE_CONTRACTS.md) and per-feature audits are authoritative for current ownership. The structural index does not resolve C/C# calls or binary Unity dependencies.
