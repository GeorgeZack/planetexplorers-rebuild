# Content and platform source contracts

Static source/database audit; current native scope is in [REPLICATION](../REPLICATION.md). Unity source version5.2.4f1 and ConfigFiles content version1.1.0 are distinct from installed Linux reference V1.1.4. Build/runtime commands belong to [rebuild README](../../rebuild/README.md).

## Inventory and feasibility

The reference has binary Unity scenes/prefabs/assets, not text YAML; use a serialized-file reader, not string scraping. Original source omits Resources/Model, Resources/Sound, AssetBundles, VoxelData and Plugins inputs. No LFS pointer stubs were found in the initial scan; `git lfs pull` is not a fix. Installed-game recovery is a separate evidence lane ([asset recovery](ASSET_RECOVERY.md)). Missing model-path resolution signals a gap, not a full dependency proof.

EditorBuildSettings enables GameStart, GameRoleCustom, GameMainMenu, GameLobby, MLoginScene, PeGame, Intro, GameCredits and GameED under Assets/Scenes. GameTheEnd is disabled/absent. Other scenes can be examples. Old symbolic scene names in GameConfig are not stronger than active build settings/callers.

GameConfig/AssetsLoader distinguish AssetBundles, Resources, editor-relative paths and user documents/PlanetExplorers. Preserve explicit catalog identity/cache boundaries rather than reproducing path magic. Native runtime uses executable-relative assets and labelled adapters.

## Database contracts and pitfalls

`Assets/Resources/DataBase/localData.bytes`: SQLite, source SHA256 `c3d63ee865a7e10e309e11e9ee561c93feef39c379a584dcf68722a523cd4376`. External `i18n.db`: SHA256 `965648c34c5767aeeba8187c121ba128a24e5915eb532d18f666371e24f40299`. Open read-only. LocalDatabase copies bundled bytes to temporary SQLite and orders subsystem loading; the exporter need not initialize Unity.

| Table | Original audit rows / interpretation |
|---|---|
| PrototypeItem |1744 raw/1743 numeric; metadata id `max=1743` |
| synthesis_skill |589 formulas |
| PrototypeMonster / PrototypeNPC |221 /70 definitions |
| Quest_List |1070 raw/1069 numeric; nonnumeric metadata header |
| Quest_KillMonster |42 targets, not complete missions |
| sound |5330 definitions, not available audio clips |
| plant / mineral / resource |36 /11 /475 |
| PrototypeTree / skilltree |346 /236 |
| vc_part / vc_material |89 /13 |
| Translation |12999 raw rows, not unique numeric IDs |

Many untyped columns contain text. Validate numeric IDs strictly; permissive SQLite casts can turn metadata into fake rows. Presence does not imply publish/mode/unlock eligibility. Preserve component/formula dependencies and original IDs.

Recipe fields include output quantity, need_time, six material/count pairs, complex/formulaIds/workspace; **materia4** is the real misspelled column. Fractional durations exist. Enforce complete material/stack/workspace/unlock contracts, not generic placement recipes. blocktype, buildblock and mining resource mappings remain separate.

`export_content.py` owns generated tables and content_manifest.json. Current closure counts come from that manifest, not historical audit snapshots. Preserve existing recipe order when extending, regenerate and run `--check`. Exact recipe/attribute/resource values belong in generated metadata and the feature audit, not duplicate command guides.

## Localization and export semantics

PELocalization assigns rows in sequence; later duplicate IDs win (ID2400146 appeared ten times). Export explicitly orders rowid and preserves last-row semantics. Duplicate numeric gameplay item/formula IDs fail. Name precedence: nonempty external English, embedded English, then item _engName. Per-entry fallback is a native robustness adapter versus original whole-database selection. Missing optional external DB is supported; present corruption fails clearly.

Terrain construction items come from52 buildblock rows into a256-entry map; unknown=-1. Mining uses resource.ID directly, not its level list. Primary cumulative production trials, fixed_num/self_num and stochastic rounding are separate from construction costs. Extra/special source production must not be claimed merely because raw strings were retained. See [terrain parity](TERRAIN_PARITY.md) and [harvest](SANDBOX_HARVESTING.md) for supported algorithms.

## Input, UI and dependencies

PeInputSettings is the original binding authority (version20161114), with separate General/Character/Building/Vehicle sets. Native supported controls are documented separately; unsupported source bindings do not enable gameplay. Recover serialized atlas/layout/component values and shared input ownership before visual acceptance. GameUITest's name alone does not make its code unused.

Upstream excluded third-party plugins; missing source dependencies include uLink, Steamworks, pathfinding/RVO, FinalIK, InControl, NGUI and FMOD components. Some managed DLLs remain (Behave, protobuf, Newtonsoft, C5, AssetStoreTools); omission does not mean zero binaries. They are reference evidence, not directly linkable C gameplay. CPU terrain can use maintained liblz4 instead of original lz4_dll.

Sound loaders expect absent Sound/<clipName> assets; FMOD references do not supply clips. Do not equate definition counts with recovered audio or claim original sound parity. Preserve [Pathea terms and attribution](../../README.md#original-upstream-terms); no recovered binaries/assets enter Git.
