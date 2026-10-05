# Asset, data, platform and UI audit

Audit baseline: commit `de214409243efadd6b8ca2072a0b54670564900e`; working tree observed 2026-10-03. Inventory is filesystem based, excluding `.git`. Source paths below are repository relative. This audit did not execute the old Unity game; visual fidelity and behavior encoded solely in binary assets remain unverified.

## Inventory and feasibility

- `ProjectSettings/ProjectVersion.txt` identifies **Unity 5.2.4f1**; `ConfigFiles/version.txt` says **Ver 1.1.0**. These are distinct engine and content versions.
- 21,167 project files outside Git, 1,012,369,647 logical bytes. `Assets` contains 20,945 files, 847,289,367 logical bytes, including 10,983 `.meta` files, 2,888 C# files (581,836 lines), 3,235 prefabs, 57 scenes, 69 `.asset`, 1,142 materials, 197 shaders, 148 controllers, 37 animation files. Counts include example/test/vendor code, not just shipped game content.
- Raster assets: 1,069 PNG, 164 TGA, 113 JPG, 91 DDS, 61 PSD; source models: 136 FBX, two OBJ, three LXO, one MAX; nine TTF fonts. Audio inside Assets is only 12 WAV plus eight OGG. Root CustomSounds adds one OGG.
- Every inspected `.unity`, `.prefab` and `.asset` starts with binary Unity serialization, not `%YAML`. All 57/3,235/69 respectively are binary. A proper Unity serialized-file reader or a legally available legacy editor/export path is required for reliable scene/prefab references, transforms and component values; string scraping cannot reconstruct them faithfully.
- Complete small-file scan found **zero Git LFS pointer stubs**. Missing content is not solved by `git lfs pull`.
- 34 orphan `.meta` entries, predominantly folder metadata. Crucially, `Assets/Resources/Model` and `Assets/Resources/Sound` do not exist. Neither root `AssetBundles` nor `VoxelData`, `Assets/Plugins`, or `Assets/StreamingAssets` exists. 275 nonzero database `_modelPath` entries did not resolve to `Assets/Resources/<path>.prefab`; this simple path probe is a gap signal, not a full dependency traversal.

## Scenes and startup

`ProjectSettings/EditorBuildSettings.asset` contains ten serialized scene entries. Payload decoded as count + enabled uint32 + length-prefixed, padded path:

| Build entry | Enabled | Exists |
|---|---|---|
| GameStart | yes | yes |
| GameRoleCustom | yes | yes |
| GameMainMenu | yes | yes |
| GameLobby | yes | yes |
| MLoginScene | yes | yes |
| PeGame | yes | yes |
| Intro | yes | yes |
| GameTheEnd | no | no |
| GameCredits | yes | yes |
| GameED | yes | yes |

All paths use `Assets/Scenes/<name>.unity`. Many other checked-in scenes are examples or development tests. Do not treat every scene as a shipped gameplay level. `GameConfig.cs:57-67` also retains old symbolic scene constants (GameStory, GameAdventure, GameBuild, GameTraining etc.) that do not correspond to these build entries; treat active launch code/build settings as stronger evidence.

## Runtime data and content paths

`Assets/Scripts/System/GameConfig.cs:23-34,69-114,135-161` resolves game content relative to the executable/project parent. It expects `AssetBundles/`, `VoxelData/zData`, `VoxelData/networkvoxel`, `VoxelData/SubTerrains`, `i18n.db`, `CustomGames/`, and the Unity resource `DataBase/localData`; user-created content/config/cache lives under the user's documents `PlanetExplorers` directory. `Assets/Scripts/AssetsLoader/AssetsLoader.cs:267-345,407-450` distinguishes `.unity3d` bundles, editor `../` paths and `Resources.Load` paths. Assets are cached/pool registered; C content IDs should preserve this distinction in an explicit catalog rather than replicating Unity path magic.

Reusable physical content includes:

- `Assets/Resources/DataBase/localData.bytes`: ordinary SQLite 3 database, 7,776,256 bytes, **187 tables**. Open read-only directly; no Unity dependency is required. `Assets/Scripts/Database/LocalDatabase.cs:24-57` merely copies these bytes to a temporary SQLite file. `LoadAllData`, lines 59-179, enumerates subsystem bootstrap/load order.
- Root `i18n.db`: SQLite, 2,676,736 bytes, one `Translation(ID,English,Chinese)` table with 12,999 rows. Embedded database also has 12,999 translation rows. `PELocalization.cs:15-49` loads external localization first; `LocalDatabase.cs:26-30,79` supplies fallback. `ItemData.cs:744-760` resolves `_nameID` through localization, falls back to `_engName` when empty.
- `TutorialMode/Mainland`: map/water `.voxelform` at five LOD files each, `subter.dat`, `WorldEntity.xml`. `CustomGames/` supplies several named custom scenarios, mission XML, worlds, minimaps and terrain. `RandomTownArt/` supplies fourteen `.art` artifact files. These are valuable decoding fixtures, not proof the complete story world exists.
- `ConfigFiles/MapConfig.xml` enumerates Cooperation/VS/Survival, Story/Adventure/Build/Custom, eight terrain themes (Grassland, Forest, Desert, Redstone, Rainforest, Mountain, Swamp, Crater), Temperate/Dry/Wet/Random weather, and team counts 1–4.

## Database contracts and pitfalls

| Table | Raw rows | Valid numeric ID rows / meaning |
|---|---:|---|
| PrototypeItem | 1,744 | 1,743; first metadata row has `id='max=1743'` |
| synthesis_skill | 589 | 589 recipes |
| PrototypeMonster | 221 | 221 monsters |
| PrototypeNPC | 70 | 70 named NPC definitions |
| Quest_List | 1,070 | 1,069; metadata header `ID='任务ID'` |
| Quest_KillMonster | 42 | targets, not complete missions |
| sound | 5,330 | sound definitions, clips mostly absent |
| plant | 36 | farming growth data |
| mineral | 11 | scanner/mineral mapping |
| resource | 475 | gathering/production rules |
| PrototypeTree | 346 | trees/plants with attributes |
| skilltree | 236 | progression definitions |
| vc_part / vc_material | 89 / 13 | creation component/material definitions |
| Translation | 12,999 | string IDs |

Most schema columns are untyped SQLite fields storing text. Preserve integer IDs explicitly; skip metadata rows using strict numeric validation, not SQLite permissive casts alone. Do not infer gameplay-ready status solely from a row's presence (`publish`, modes, unlock and quest conditions matter).

Item columns include id, localized name ID, `_modelPath`, `_logicPath`, icon, type, stack limit, durability/repair, attack/defense and resource damage/range, movement, max/current vitals, food/debuff stats. Monster/NPC records combine model and behavior paths with essentially the same attribute schema. Recipe fields are `ID,object_ID,need_time,num_each_product,material1,num1,material2,num2,material3,num3,materia4,num4,material5,num5,material6,num6,complex,formulaIds,workspace`; note the **`materia4` typo is the actual database field**. Durations can be fractional (arrow recipe `.5`). Crafting must check and consume every material atomically, respect resulting stack limits and unlock/workspace conditions, then award `num_each_product`.

Useful initial actual IDs: wood 279, stone 280, dirt block 268, iron 271, copper 273. Wood/stone stack limits are 9,999. Source-backed early recipes:

| Recipe ID | Output item | Seconds | Materials |
|---|---|---:|---|
| 1 | 12 Wood Shovel | 3 | 279 Wood ×10 |
| 7 | 19 Wood Pickaxe | 3 | 279 Wood ×10 |
| 8 | 20 Stone Pickaxe | 3 | 280 Stone ×6, 279 Wood ×6 |
| 19 | 1 Wood Sword | 3 | 279 Wood ×10 |
| 20 | 2 Stone Sword | 3 | 280 Stone ×12 |
| 58 | 310 Wood Ladder | 5 | 279 Wood ×2 |
| 152 | 308 Wood Fence | 5 | 279 Wood ×2 |

Initial rebuild may unlock these as an explicitly simplified test scenario; do not claim original unlock progression. Raw voxel/block placement costs require the separate building-system mapping, not a fabricated generic recipe.

## Input and UI replication

`Assets/Scripts/Input/PeInputSettings.cs` is the authoritative action/default-binding catalog; version is `20161114`, with separate General, Character, Building and Vehicle settings. InControl is referenced at line 7. Defaults around lines 180-235 and 295-321:

- WASD move; Space jump; R reload; Z walk/run toggle; Shift sprint; Ctrl block.
- Left mouse primary attack/shoot/dig/build/cut/equip contextual action; right mouse interaction/gather/take/put-away; E combined interact/talk/cut/gather/take. Middle mouse autorun; wheel zoom.
- M map, Q missions, I inventory, C character, T rotate, Y replication/crafting, Alt+T skill tree, Alt+F followers, H handheld PC, J creation editor, Alt+C colony, Alt+Z friends, Alt+X shop, Enter chat, backquote game menu, Escape close menus, F5 control mode, F9 save menu, F10 load menu, F12 capture.
- Number keys 1–0 quickbar, comma/period previous/next bar; B build mode and F free building mode; arrows/PageUp/PageDown selection adjustments plus undo/redo, selection boolean ops and extrusion in building bindings. Preserve action remapping rather than baking keys into simulation.

Large NGUI-dependent UI source lives under `Assets/Scripts/Chenzhi/NewUI/Scripts/PE_GameUI`, including `GameWnd/UIItemPackageCtrl.cs`, `GameWnd/UIMissionWndCtrl.cs`, `GameMain/UIMissionTrackCtrl.cs`, `UIBuild`, workshop, options and HUD. `Assets/Scripts/GameUITest` additionally contains colony trade/training and companion UI code; despite its folder name it may be used by production prefabs, so do not delete on name alone. C UI should reconstruct inventory/equipment/hotbar, interaction prompts, crafting, mission journal/tracker, map, follower/colony/creation views and settings in phases. Existing atlases and raster art can be converted, but binary prefab positions and missing NGUI library block an exact immediate UI import.

## Missing or legacy dependencies

`README.md:11` explicitly says third-party plugins were excluded. C# references confirm missing uLink networking (`GameNetwork/Player/BaseNetwork.cs`), Steamworks (`PlayerNetwork.cs`), A* Pathfinding and RVO (`Assist/PEPathfinder.cs`), FinalIK (`Assist/IKCombat.cs`, `PeEntity/Cmpt/IKCmpt.cs`), InControl (input), NGUI widgets, and FMOD namespaces/components (`PeAudio/Ambient/BGMInst.cs`). Native `lz4_dll` imports occur in `Terrain/Voxelform2/.../VFDataReader.cs` and `ZhouXun/Layered SubTerrain/LSubTerrIO.cs`. Rebuild should replace behavior behind explicit interfaces, not attempt to link incompatible Unity assemblies.

Seventeen DLLs do remain, including Behave runtime/editor/assets, protobuf-net, Newtonsoft.Json, generated Behave libraries, C5 and AssetStoreTools; README's general omission statement is not a literal claim that zero binaries exist. Behavior tree source/config may be inspectable, but generated assemblies cannot be linked into C gameplay directly.

Audio is severely incomplete: `Scripts/Sound/AudioManager.cs:80` loads `Sound/<clipName>` and `SoundSE.cs:91` uses the same root, which is absent. `PeAudio/Ambient/BGMInst.cs` implements pre/post warmup and volume transitions using missing FMODAudioSource; BGA/SPOT use it too. Reconstruct ambient/music crossfade and spatial-emitter contracts and use provisional/generated tones until licensed clips are supplied. Do not claim the 5,330 sound definitions imply 5,330 available audio assets.

## C foundation options and migration plan

Local toolchain verified: `/usr/bin/cc`, GCC, Clang and make; pkg-config reports raylib 6.0.0, SDL2 compatibility 2.32.72, SDL3, OpenGL and SQLite 3.53.4. `/usr/include/raylib.h` and `sqlite3.h` exist. A C11 core with raylib window/input/OpenGL/audio is the shortest native 3D starting path on this machine; keep simulation headless and platform APIs at the edge. This is a local feasibility finding, not a cross-platform compatibility guarantee. CMake/ninja were not found in PATH in the initial probe; Make is available.

Build a stdlib Python offline exporter opening SQLite read-only and generating checked-in immutable C tables, preserving source DB SHA-256 and original numeric IDs. Begin with explicit item/recipe subsets, validating localized names, stack values and all material references. Later expand to all typed gameplay tables. Keep source licensed data separate from newly written engine code. Convert source images/audio to portable PNG/OGG, model content to glTF or another runtime format through an auditable pipeline; independently extract Unity scene/prefab IDs and references. Never feed Unity shader code directly to OpenGL and expect parity.

`README.md:7` retains Pathea asset ownership, permits hobby/noncommercial use, prohibits direct resale of recompilation and requires written consent for commercial asset use. Record these supplied terms and preserve provenance. This is relevant to asset redistribution; it does not prevent the authorized local rebuild. Exact final content parity is blocked by absent art/audio/story terrain/plugin content and requires a recovered asset set or replacement production.


## Export validation findings

The initial C exporter is implemented in `rebuild/tools/export_content.py`; generated tables and detailed provenance are in `rebuild/generated/pe_content.c` and `content_manifest.json`. It selects 12 item definitions and seven recipes (IDs 1, 7, 8, 19, 20, 58, 152), preserving the source quantities, fractional-capable durations, stack limits and English localization. `--check` verifies both generated files without rewriting them.

Direct export uncovered a localization detail beyond raw table counts: `Translation` contains ID 0 and repeated numeric IDs, including ten rows with ID 2400146. The old `PELocalization.LoadData` assigns dictionary entries in sequence, so later rows replace earlier ones. The exporter explicitly orders by SQLite `rowid` and uses the last translation row. Duplicate numeric gameplay item/recipe IDs are rejected. Consequently raw translation row counts must not be presented as unique string counts.

Export name resolution prefers nonempty external English, then embedded English, then the item `_engName`; the per-entry fallback is an intentional robustness improvement over the old loader's whole-database selection. Missing optional external localization is supported; an existing but corrupt localization database fails clearly instead of silently changing names.

Source hashes recorded by the initial export:

- `Assets/Resources/DataBase/localData.bytes`: SHA-256 `c3d63ee865a7e10e309e11e9ee561c93feef39c379a584dcf68722a523cd4376`.
- `i18n.db`: SHA-256 `965648c34c5767aeeba8187c121ba128a24e5915eb532d18f666371e24f40299`.

The exporter reads both databases with SQLite `mode=ro`. Its tests cover reference closure, original recipe values, metadata filtering, deterministic output, freshness without writes, fractional time, the `materia4` column, localization precedence/fallback, read-only access and invalid/missing source failure.

### Equipment and terrain resource export extension

The next native milestone expands the catalog to **27 items and eight recipes**, appending recipe 13 (ten Wood → one Wood Axe, three seconds) while preserving the original seven recipe positions. `PeItemDef` now retains source item type, attack, raw resource damage/range, skill ID, durability maximum, per-attack durability expenditure (`durDec`), and cutting damage/bonus. Dig and axe stamina defaults are read from `PEDigTool.cs` / `PEAxe.cs` rather than inferred from zero item attributes; both class defaults are five. Their source hashes accompany the database hashes. Binary prefab overrides remain unverified.

`initProp` player row 1 supplies stamina maximum 100, recovery speed 40, recovery delay 0, consumption multiplier 1, resource radius 0.49, resource damage bonus 0, attack 10, and cutting damage/bonus 0. These are base attributes; they do not include the entire legacy buff/skill system. Selected shovel/pickaxe item `ResDamage` and `ResRange` values are actually zero and remain zero in the catalog. In `SkEntityVoxelTerrain.cs:61-75`, terrain damage is caster attack × (1 + resource damage bonus), with an additional ×5 in return-resource mode; radius is caster resource range. Equipment buff 30200091 adds the shovel/pickaxe attack to player attack. `ItemObj.cs:1639-1641` expends `durDec` on an attack; selected tools use five raw durability units. Original `Action_DigTerrain.cs` rejects broken tools but slows animation at stamina thresholds 40/20, rather than imposing a hard minimum stamina gate.

**Construction materials and mining drops are separate catalogs.** The 256-entry `pe_terrain_material_items` table comes from 52 `buildblock` rows and maps legacy material IDs to placement-cost items; unknown entries return -1. It is not used as proof of a mining drop. `pe_terrain_material_durability` comes from `resource.ID -> durability`; 128 resource rows have IDs within the byte material range. `NaturalRes.GetTerrainResData` matches this ID directly, not its `level` list.

Actual mining descriptors are exported separately as `pe_terrain_resources`, including primary cumulative item thresholds, `fixed_num`, and `self_num`. All referenced primary items are included in the 27-item catalog. Example: material 57 selects Zinc (989) for integer rolls below 70 and Iron (271) below 100, with `self_num=4`; material 55 has no primary drop. Dirt resource 2 and stone resource 17 each have `self_num=2`, with durations 0.20159 and 0.15875 respectively. Generated terrain's dirt/grass and stone may use those representative resource IDs, but its own small palette must never be mistaken for original material numbering.

`DigTerrainManager.GetResouce:402-447` counts removed samples by material, computes trials as `(fixed_num > 0 ? fixed_num : self_num + bonus) × removed_count`, stochastically rounds fractional trials, and samples the cumulative primary thresholds. Production segments two and three describe additional and special drops. Those segments are retained verbatim in the provenance manifest but **are not implemented in the exported runtime descriptors**; rare bonus drops and original RNG parity remain outstanding. Construction-item lookup does not replace these rules.
