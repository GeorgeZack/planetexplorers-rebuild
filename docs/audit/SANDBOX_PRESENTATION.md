# Sandbox presentation, assets and entity integration audit

This is a read-only source and implementation audit on 2026-10-03. It covers the default C sandbox and the route from the recovered tutorial presentation to an original Adventure or Build experience. No runtime, asset export, desktop input or graphical validation was performed for this audit. Original Adventure observations being collected separately are not assumed to be a completed parity fixture. World geometry and persistence details are in [SANDBOX_WORLD.md](SANDBOX_WORLD.md); inventory, actions and progression are covered by [SANDBOX_GAMEPLAY.md](SANDBOX_GAMEPLAY.md).

The immediate gap is integration. The repository already has original clothed characters, sampled animations, tool attachments, NPC idles, combat geometry, atlas sprites and fonts. The default sandbox does not use them. Connecting these assets requires shared world, entity and UI contracts; simply drawing the training actors over the existing prototype would retain different controls, state, collision and action timing.

The subsequent [live alignment report](SANDBOX_ALIGNMENT.md) records the completed original Adventure/Build inspection and its limits; it does not establish C presentation parity.

## Current frontend boundary

| Area | Default sandbox today | Reusable work and remaining boundary |
|---|---|---|
| Entry and world | `main.c` creates a bounded generated world or imported crop. Training and reference previews are separate CLI branches. | There is no default-world asset-directory option or explicit Adventure/Build presentation mode. Do not treat a recovered room as an Adventure map. |
| Player | First-person eye at feet+1.58, FOV70; no drawn body or held tool. | Training has a recovered female starter appearance, six locomotion clips and separate knife/cutting/digging scenes. Camera/controller behavior and finite clip selection remain approximations. |
| Terrain and construction | Vertex colors and a fixed directional brightness term; source material IDs are reduced to generic colors. Actual Block45 cube/ramp geometry is present. | Material textures, material blending, source lighting, weather and general-world shadows are absent. Source world material IDs must remain distinct from item/cost IDs. |
| Vegetation and furniture | Twelve deterministic cylinder/sphere trees; one-meter fence/ladder proxies; a static water rectangle. | Source mesh recovery is practical, but biome placement, tree identity, depletion, collision, LOD, wind and water behavior need their own integration. |
| People and enemies | No NPC/hostile collection, actor rendering, HP/death state or general AI in `PeGame`. | Nine actual tutorial NPC appearances/idles and a source hare are available. They are fixtures, not an Adventure population algorithm. |
| HUD and interaction | Custom dark panels, raylib default text, five material choices and bespoke gathered/crafted/built checkboxes. Inventory is a text list over32 slots. | Original HUD, panels, nine quickslots, icons, bitmap fonts and effects exist in training. Their current renderer reads `PeTraining`, not `PeGame`. |
| Map | No original world minimap. | Training uses a recovered room PNG and nine fixed NPC markers. General Adventure requires a world map source and dynamic entity markers. |
| Actions | Mining input is attempted every.25s; construction and inventory use sandbox controls. | Training shares recovered body/tool clocks with bounded knife, fell and shovel actions. Tutorial grants, completion events and cleanup must not become sandbox rules. |
| Save | PECSv3 bounded world/inventory/tool/crafting/construction/tree snapshot. | No general entity identity, appearance, AI, HP/death, faction or encounter persistence. GPU models and animation caches must remain derived state. |

Evidence: [default frontend](../../rebuild/src/main.c), [game state](../../rebuild/src/pe_game.h), [game implementation](../../rebuild/src/pe_game.c), [save](../../rebuild/src/pe_save.c). In `main.c`, `update_mesh`, `draw_tree`, `draw_block`, `hud`, `inventory_panel` and the default render loop make these distinctions explicit. The elevated external camera in the smoke fixture is not the normal gameplay camera.

## Preserve source mode distinctions

[PeGameMgr.cs](../../Assets/PeLauncher/PeGameMgr.cs) separates player type and scene mode: Tutorial is not Single Adventure or Single Build. `unlimitedRes` and `monsterYes` are separate settings; mode names alone do not establish all their effects.

[SceneEntityCreator.Update](../../Assets/Scripts/Scene/SceneEntityCreator.cs) explicitly returns for Single Build before ordinary observer-driven ambient refresh. Outside the excluded modes, it fills a3×3 set of entity-position regions around the scene observer. `New()` creates the fixed tutorial roster only on its Story/Tutorial path. This proves that copying tutorial NPCs and an always-hostile hare into every Build world would be wrong; it does not prove that every possible scripted creature path is disabled in Build.

[MonsterEntityCreator.cs](../../Assets/Scripts/Scene/MonsterEntityCreator.cs) selects Adventure prototype/scale through `AiUtil.GetMapID`, `GetAreaID`, point type and `AISpawnDataAdvSingle.GetPathIDScale`. [NpcEntityCreator.cs](../../Assets/Scripts/Scene/NpcEntityCreator.cs) also has a generated-NPC path through `PeCreature.CreateRandomNpc`. The nine named training NPCs are not substitutes for these systems.

[SingleGameInitData.cs](../../Assets/PeWorld/SingleGameInitData.cs) gives Adventure and Build different starting packages: Adventure item1289, conditional1743, and500 money; normal non-demo Build item1291 and500 money. Both initialize source clothing; Tutorial has its own initialization. The package expansion and gameplay rules need their own source-backed implementation. The current empty-inventory wood-gathering bootstrap is not an observed original Adventure start.

## Reuse assets through identity and clock contracts

The following infrastructure already works independently of Unity at C runtime:

| Recovered asset or module | Suitable reuse | Required constraint |
|---|---|---|
| `pe_actor_scene_view` / PEAS and PEAC | Multipart clothed actors, body and attached-tool scenes, arbitrary ordered clips. CPU preparation validates files before GPU upload. | Keep one model-root transform and exact shared clip clocks across material parts; own/unload GPU resources before closing their context. |
| Female starter body and sampled actions | A narrowly identified original female appearance with idle/walk/run/jump/fall/land, Attack6–9, Felling1–3 and Spade1–3. | These are evaluated clips on a specific Avatar, not a general humanoid retargeter or all player customization. Do not apply them to a different Avatar merely because bone names resemble it. |
| `pe_player_locomotion` | Selects loaded clip names/durations from actual velocity, accepted jump edge, grounded/walk/sprint state. | Discrete forward-only selection, sprint run fallback, missing blends and root motion remain provisional. Camera-relative strafing is not recovered by rotating the forward clip. |
| Knife, safety axe and wood shovel scenes | Source attachment transforms, synchronized body/tool poses, held/stowed arrangements where exported. | A recipe icon, equipment ownership and drawn model are different states. Draw/stow/action permissions need one authoritative owner; source attachment and gender provenance are not encoded by a PEAC filename alone. |
| Nine tutorial NPC scenes | Validates dedicated clothing, Avatar, skinning, multi-material parts, names and idle playback. | Each NPC has its own sampled idle and original rig. The tutorial roster/positions/dialogue cannot be reused as Adventure population. Other actions require more source clips on the correct Avatar. |
| Hare actor, PEHB, `pe_hare`, `pe_hare_attack` | One source-backed creature's appearance, locomotion constants, melee windows and animated contact geometry. | Fixed tutorial spawn, scene-collision interface and host-supplied hostility/eligibility are not a general encounter system. |
| `pe_ui_assets` and UI effects | Original sprite rectangles/borders, item/portrait icons, six loaded original bitmap fonts, source additive effects. | The higher-level training renderer and minimap still depend on tutorial state and room coordinates. |

The asset pipeline and evidence are documented in [player recovery](PLAYER_ASSET_RECOVERY.md), [Animator baking](ANIMATOR_BAKE.md), [actor recovery](ACTOR_RECOVERY.md), [NPC appearance](TRAINING_NPCS.md), [NPC idle sampling](NPC_IDLE_SAMPLING.md), [tree/axe assets](TRAINING_TREE_ASSETS.md), [shovel assets](SHOVEL_ASSETS.md) and [UI effects](UI_EFFECTS.md). Exported binaries remain local ignored outputs with source hashes; this audit does not propose copying installed bundles into the repository.

Introduce an asset catalog keyed by source item/prototype/appearance identity and explicit capabilities: static versus skinned, available clips, rig/Avatar identity, attachment mount, material policy and hitbox set. Resolve that catalog once when spawning or changing equipment. Retain the hashes and source identity in export metadata. Do not put arbitrary file loading in the per-frame draw loop or infer compatibility from directory naming alone.

The existing room mesh is useful as a controlled reference fixture and proof of hierarchy/material/collision conversion. Its baked world placement, mission props and complete-room collision are not reusable as general world geometry without explicit placement transforms. A global-to-local origin adapter must be shared by terrain, blocks, actors, attached tools, contacts, camera and map markers; imported crop metadata currently does not make the sandbox renderer globally positioned.

## Shared presentation state and input

[pe_training_ui_draw](../../rebuild/src/pe_training_ui.h) takes `PeTraining`; it cannot truthfully draw a sandbox inventory by changing a few labels. A shared read-only presentation snapshot should expose actual vitals, currency, inventory categories/pages, equipment, quickslots, crafting jobs, notices, map/quest markers and interaction targets. A corresponding action interface should request inventory moves, use/equip, recipe submission and cancellation from the authoritative gameplay owner. Rendering must not duplicate or mutate those collections.

The current source layout has42 visible inventory cells,30 storage cells and nine quickslots; `PeGame` has32 inventory slots and no matching category/page/quickslot system. Reusing panel chrome is possible before every feature is implemented, but empty source-looking controls must not claim functionality or invent extra capacity. Existing crafting escrow, cancellation, tool-instance durability and stack limits need to survive any UI migration. The UI should not award tutorial items or update tutorial mission counters in a general world.

Modal input must be resolved before world input: pointer presses consumed by inventory, dialogue, recipe controls or drag/drop cannot also mine, attack or place. Camera mode, free cursor, pause and equipment visibility should come from shared state rather than multiple independent booleans. Tool action admission must use an actual equipped instance and physical movement/ground state. Preserve source keyboard/mouse behavior as each mode is audited; the current sandbox right-click placement/X removal differs from the recovered point brush's left add/Alt+left subtract.

[pe_training_ui_minimap.c](../../rebuild/src/pe_training_ui_minimap.c) samples the training-room PNG and draws the fixed training roster. Reuse its sprite/frame/shader work separately from map content. An Adventure map needs a top-down world render/cache or an independently verified source map representation, with dynamic entities and world coordinates. Displaying the room map over procedural hills would be a functional error even if the frame matches perfectly.

## Hostile AI integration

[pe_hare.h](../../rebuild/src/pe_hare.h) deliberately accepts a host-selected target and documents that it does not invent perception, reputation or hostility. Its initialization uses source tutorial spawn `(15,2,12)` and `PeSceneCollision`. [pe_hare_attack.h](../../rebuild/src/pe_hare_attack.h) similarly separates normalized animation windows, wall-time skill cooldowns, guide gates, geometry and successful contact deduplication. These separations should remain intact.

Before one general-world encounter, provide:

1. Stable entity/prototype IDs and an explicit mode/spawn definition; appearance/scale and source combat attributes belong to that entity, not a draw call.
2. A world collision/query adapter covering edited terrain, Block45 and appropriate scene obstacles. The target position and LOS query must use the same coordinate space as the rendered creature. Grounding cannot keep querying the training-room BVH after the world changes.
3. Host target selection/permission derived from the relevant source subset. [TargetCmpt.cs](../../Assets/Scripts/PeEntity/Cmpt/TargetCmpt.cs) owns enemies, threat/hatred, scanning and attack selection; the existing C helper expects their result rather than reproducing them. An explicit debug encounter may inject a target for testing, but should be labeled as that fixture.
4. One authoritative action clock and root transform shared by body, tool and animated defence/attack capsules. Source contact windows and successful skill execution must authorize damage; screen-space clicking or proximity alone cannot.
5. Current HP, damage/death transition, one-shot death notification, resource/loot policy and save semantics. `PeHare.max_hp` is not current HP, and `PeGame` currently has no player HP/death owner.
6. Cancellation and target invalidation for death, despawn, equipment changes, lost eligibility and world replacement. Entity IDs must not be recycled while a contact-dedup record can still refer to the previous entity.

[pe_training_melee.h](../../rebuild/src/pe_training_melee.h) supplies a tested bounded stationary third-person knife contact path, including matching source body/weapon clocks. It explicitly omits moving attacks, swept contact, hit-stop and general AI/skill effects. Reusing it should preserve these admission limits rather than silently allowing unsupported situations. General source AI includes behavior-tree arbitration, navigation, other attacks and non-hare prototypes; those remain separate work. See [enemy audit](TRAINING_ENEMY.md), [combat audit](TRAINING_COMBAT.md) and [hitbox recovery](HITBOX_RECOVERY.md).

## Art portability and rendering cost

Static OBJ conversion preserves source placement and material slots; actor PEAS restores one diffuse image per material part. Current [actor rendering](../../rebuild/src/pe_actor_view.c) uses diffuse textures and supplied tint with raylib's default material path. Original normal/specular/emissive maps, skin/hair shaders, environmental lighting and render queues are not automatically reproduced. PNG alpha must be interpreted according to the source shader, not universally as alpha blending. The recovered Alpine tree sample, for example, has source cutoff approximately.55; the tutorial foliage's separate chosen threshold is not a universal vegetation rule.

The static exporter retains extra material properties in metadata even where it renders only diffuse. Preserve tiling/offset, alpha cutoff, color, culling, blend/depth policy and source shader identity in future catalogs. Unity Cg shader objects are not portable GLSL. Start with one controlled material and fixed lighting/camera evidence before applying a guessed shader to every recovered asset. Keep source material colors separate from the sandbox's construction-cost item mapping.

Original terrain shaders under [VoxelBlend](../../Assets/Terrain/Voxelform2/Voxelform/Source/Shaders/VoxelBlend/VoxelBlend.cginc) and its shader variants use multiple diffuse/normal maps and packed blend attributes. The current C mesh/frontend carries a normal/material and vertex color, with no equivalent texture/blend input. A texture atlas assignment alone cannot reproduce those shaders. Vegetation placement and terrain painting are also coupled to the generator and `Layered SubTerrain`/grass systems; see [world alignment](SANDBOX_WORLD.md).

Population scale introduces a performance gap before visual parity. `pe_actor_view_draw` skins every draw, expands indexed vertices into mesh arrays, and uploads positions and normals every draw. It has no actor-level frustum culling, LOD, shared GPU skinning or distance-based animation update policy. The current nine-NPC room fixture proves correctness at that fixture size; it does not establish a general-world crowd budget. Asset sharing, pose caching and bounds/culling must preserve independent actor clocks and unload ownership. Measure frame time, bytes uploaded, resident texture/palette memory and load latency at explicit populations before choosing optimization thresholds.

## Concrete order and acceptance gates

| Priority | Next bounded result | Required evidence before expanding |
|---|---|---|
| 1 | Explicit Adventure/Build configuration and source initialization fixture; shared world/entity/presentation contracts. | Source initial package/mode settings recorded; state owners and origin conversion documented; no tutorial grants, NPC roster or ambient spawn injected into Build by default. |
| 2 | One recovered female player and appropriate held/stowed tool in the sandbox, using shared collision and source action clocks. | Body/tool/contact poses agree at the same clip/time/root; walk/run/jump admission, pause/resume, equipment transitions and terrain edits behave consistently; unsupported clips remain explicitly bounded. |
| 3 | Original HUD/inventory/crafting presentation backed by actual sandbox state. | Inventory/escrow/tool-instance conservation tests pass through UI actions; panel clicks cannot trigger world actions; source fonts, atlas borders, scaling and focus behavior compared at recorded resolutions. Real world map content replaces the room raster. |
| 4 | One explicitly configured Adventure enemy encounter. | Spawn/grounding, deterministic target and cooldown fixture, source event-window contacts, HP/death-once, cancellation and save/reload checked. Build negative fixture confirms no ordinary ambient population path. |
| 5 | A source-selected population and vegetation slice. | Prototype/biome/scale selection and despawn/persistence verified; distinct actors retain distinct clocks/IDs; no use of the tutorial roster as generic NPC population. Profile increasing actor counts and edited-terrain grounding. |
| 6 | Controlled terrain/vegetation/material fidelity. | Source material IDs and blend inputs preserved; normal/alpha/culling/UV behavior matched on small fixtures; rendering and collision placement agree; frame/memory budgets measured before streaming/LOD expansion. |

Keep headless state/geometry/loader tests separate from visual comparisons. Useful existing owners include `test_game`, `test_actor`, `test_actor_scene`, `test_player_locomotion`, the training melee/hare/hitbox suites, `test_ui_assets` and the source export tests in [Makefile](../../rebuild/Makefile). New integration tests should cover failures at these seams rather than repeat a helper's implementation. Required regression examples include a loaded save with equipment/action state, a destroyed support surface beneath an actor, a world-origin change, an invalid optional asset group, and a UI drag released over a world target.

For original comparisons, record installed version, Adventure/Build settings, seed/configuration, avatar/equipment, position, camera/FOV, time/weather and graphics settings. Capture the same bounded actions and views in C. The previous room/player/NPC/UI graphical successes remain useful component evidence; neither those fixtures nor this source inspection establish default-sandbox Adventure parity.
