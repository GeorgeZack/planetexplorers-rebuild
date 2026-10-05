# Planet Explorers C rebuild

A new C11 runtime built from the supplied Unity project's source and data. Its sandbox runs exploration, tool-based mining, crafting, construction, and persistence, and can import a bounded crop of an original terrain file. A separate training mode uses the recovered original room for the tutorial sequence through combat, gathering, tree cutting, crafting and mining. Construction and the receiver puzzle also pass the 15,000-frame graphical sequence, ending at the unsupported creation lesson 756. The Unity project remains unchanged as a reference.

This is a working reconstruction of selected systems, not a complete reproduction of the game. The [replication specification](../docs/REPLICATION.md), [Block45 audit](../docs/audit/BLOCKS.md), and [reference-runtime investigation](../docs/REFERENCE_RUNTIME.md) distinguish recovered behavior from missing systems and temporary substitutes. The [sandbox alignment audit](../docs/audit/SANDBOX_ALIGNMENT.md) records live Adventure/Build observations and the next implementation milestones; the controls and current implementation are described below.

## Build and run

Requirements: a C11 compiler, Make, pkg-config, raylib and LZ4 development files, and a graphical display/OpenGL driver. Python 3 with its standard-library SQLite module is needed for content regeneration and exporter tests. Headless core tests require LZ4 but not raylib or a display. Persistence currently uses POSIX APIs; Linux is the development platform.

From the repository root:

```sh
make -C rebuild
./rebuild/build/planet_explorers
```

The main menu offers **New Sandbox**, **Continue Sandbox**, and **Tutorial**. Both gameplay modes return through **Esc → Main Menu**. Continue resumes the in-memory sandbox after a tutorial visit, or loads your save after restarting the program. New Sandbox confirms before discarding an active session. The menu uses recovered logo/fonts; its background is a new approximation.

New Sandbox starts the bounded **Adventure** reconstruction: source starter items, 500 currency, empty learned formulas and Westina Nangle's welcome. Her introduction opens automatically nearby; Enter or a dialogue click advances its nine source pages. Esc interrupts it; speak to Westina again to resume. The final reply starts Arrival. Approach **Eitai Takayama**, the recovered supplier near original world X−599/Z341 (downhill from the starting pad). Arrival completes automatically within the source5m XZ target radius; aim at her and press E or right mouse to open the NPC choice window. Select the supplies question, then use Enter, a dialogue click or the arrow through source pages4090–4094. Only acknowledgment of the final page grants the one-time package; closing early grants nothing, and a full inventory leaves the final page available for retry. Q shows current progression.

This supplier is the exact archived role2 identity, entity20016/RandomNpc22, with recovered saved appearance and original idle evaluation. New Sandbox now uses the archived starting settlement: captured ground, six intact buildings, and source Westina/Eitai positions. Older saves retain their existing worlds and provisional NPC placements. General NPC generation and the later trip to Annot remain unimplemented. Headless, asset and integrated graphical/input checks pass, including cancellation, final grant and save/reload without duplicates. [Interaction and placement](../docs/audit/ADVENTURE_NPC.md) and [appearance provenance](../docs/audit/ADVENTURE_SUPPLIER_APPEARANCE.md) define the limits.

I opens inventory; right-click an item to use a consumable, learn a recipe book or equip a supported tool. Learn **Wood Shovel Script** from the supplies, then open Y, select Wood Shovel and replicate one for ten wood. C opens the character and equipment panel. Drag inventory items to the nine quickslots; 1–9 activate those shortcuts. Scanner upgrades do not unlock recipes. HUD, inventory, character, mission and Replicator panels use the recovered training UI assets with real sandbox state; the minimap shows this world.

WASD moves, mouse looks, Shift sprints, Space jumps, Z toggles walking and middle-click toggles autorun. F5 switches first/third person; the wheel adjusts third-person distance (1.8–10 m normally, up to20m in construction). In Adventure, equip Wood Shovel12, Wood Pickaxe19, Wood Axe26 or Safety Axe1527, stand still on the ground in third person, then hold left mouse to draw and use it. Digging/cutting transactions run at the recovered source clip events, and the visible body/tool share that exact action clock. Release finishes the action; R or right mouse stows and cancels when not interacting with the supplier. Aim at nearby terrain for the shovel/pickaxe, or face the same tree selected by the camera for an axe. Supplier menu/dialogue input is consumed before world/tool input. Legacy mode retains the earlier action path.

B toggles cursor-based construction. Left mouse places at the pointer; Alt + left mouse removes, and holding right mouse turns the camera. The recovered toolbar has a bounded palette: 1 cube, 2 dirt, 3 stone, 4 wood, 5 ramp, 6 fence and 7 ladder. These slots are separate from normal quickslots. V selects cube/ramp, T rotates, G switches one-cell/four-cell brushes, and X also removes the selected brush. Four half-meter cells built together cost one resource and removed together refund one; a single cell costs one and refunds zero. Toolbar and panel clicks consume world input; canvas clicks remain available for construction. Creation saving and selection deletion are disabled. Tab releases the cursor outside construction; panels allow gravity and crafting to continue. Esc pauses simulation.

The drawn wood shovel shows a cyan wire around the current source-style target sample. Construction shows a bounded one-cell/four-cell cube or ramp candidate for dirt, stone and wood, colored green or red by the same geometry, reach, occupancy and full inventory-cost checks used by the click transaction. Previewing is read-only; clicking still revalidates and commits the normal inventory/world transaction. Furniture and removal previews are not implemented. These indicators are native presentation aids, not recovered original indicator shaders.

In both Tutorial and Sandbox, **drag a panel by its title bar** to move Inventory, Character, Storage, Replicator or Mission. Buttons and item slots follow the panel; resizing keeps it on-screen. Positions last for the current gameplay session and reset when that host is reopened. The minimap and quest tracker remain anchored, with quest headings below the full map frame.

F9 saves and F10 loads outside dialogue. PECS v7 saves the active world depth and acknowledged welcome page; interrupt with Esc before saving. Loading an unfinished welcome resumes nearby. Older v4/v5 Adventure saves retain their existing quest state and treat the welcome as completed. There is no automatic save on exit. By default the file is `planet_explorers.pesave` in the working directory; `--load` also becomes the save destination unless `--save` is specified.

Recovered runtime assets default to `rebuild/build/reference-assets`; use `--assets DIR` to override them. `--sandbox` bypasses the main menu. Rebuild save versions 1–3 load as Legacy mode, retaining their previous movement rules and unlocked recipes; v4 Adventure retains its mode and progression through the tree-collision migration described below. New Adventure uses rendered-mesh and live-tree capsule collision and learned formulas. Tutorial progress resets when leaving that mode.

```sh
./rebuild/build/planet_explorers --sandbox --save /tmp/exploration.pesave
./rebuild/build/planet_explorers --load /tmp/exploration.pesave
```

### Starting settlement

New Sandbox uses one archived reference settlement, with a64×32×128 metre sample grid. `--seed` does not regenerate this recovered crop. The terrain includes changes present in the archived original save; it is not a fresh procedural seed. Its boundaries remain finite, and Annot lies outside them. Full population, shops, machine interactions, vegetation and wider travel are unfinished.

The local recovered assets are already installed. To recover the six buildings again:

```sh
/tmp/pe-asset-tools/bin/python rebuild/tools/export_adventure_settlement_assets.py
```

Terrain must come from the bounded observation probe running in the original game, then be validated/installed with `export_adventure_terrain.py --capture PATH`. The artifact-only diagnostic is not a substitute for surrounding terrain. See [capture instructions](../docs/audit/ADVENTURE_SETTLEMENT_TERRAIN.md) and [integration](../docs/audit/ADVENTURE_SETTLEMENT.md). Missing settlement assets produce an explicit startup error. Existing saves and `--terrain` imports still use their own worlds.

### Adventure NPC recovery and checks

Adventure requires both `adventure-welcome/recording.peas` and `adventure-supplier/recording.peas` beneath the selected assets root. It contains12 parts with the saved face/hair/bronze armor, stowed knife and forearm shield; the loader renders a portrait from that same recovered appearance. NormalIdle uses a matching original FemaleAvatar bake, not an observation of Eitai's live NPC controller. Diffuse material recovery does not reproduce the full original shader, IK or gestures.

Both locally installed groups already exist. Westina uses the archived role1 entity20008/RandomNpc17 with nine parts, iron armor and stowed copper sword/shield. See the [welcome audit](../docs/audit/ADVENTURE_WELCOME.md). To reproduce it only when the destination is absent, use the optional UnityPy environment described under asset recovery:

```sh
/tmp/pe-asset-tools/bin/python rebuild/tools/export_adventure_supplier.py \
  --output rebuild/build/reference-assets/adventure-supplier
/tmp/pe-asset-tools/bin/python rebuild/tools/export_adventure_supplier.py --npc welcome \
  --output rebuild/build/reference-assets/adventure-welcome
```

The exporter requires a **fresh** output directory, the preserved original Adventure `GameSave/save0` archive, installed native game resources, the closed `female-clips-02.jsonl` bake and existing training-knife fixtures. Defaults target this machine's isolated reference profile; `--save-dir`, `--game-dir` and `--bake` override those inputs. To inspect a second export, choose another fresh output path rather than overwriting the active asset group. The original archives and game files remain read-only. [Full recovery evidence and prerequisites](../docs/audit/ADVENTURE_SUPPLIER_APPEARANCE.md).

Generated dialogue and collision data can be checked without a graphical window:

```sh
python3 rebuild/tools/export_adventure_dialogue.py --check
/tmp/pe-asset-tools/bin/python rebuild/tools/export_adventure_npc_collider.py --check
python3 rebuild/tests/test_adventure_dialogue_export.py
python3 rebuild/tests/test_adventure_supplier.py
/tmp/pe-asset-tools/bin/python rebuild/tests/test_adventure_npc_collider.py
make -C rebuild build/test_core
./rebuild/build/test_core
```

`export_adventure_dialogue.py` generates source9031/9032 text, role tokens and arrival constants from the original database/localization; `export_adventure_npc_collider.py` generates exact physical/Defence capsules from FemaleModel. The supplier exporter includes matching PESC/provenance. `pe_adventure_npc` retains the first supported pose as the immutable arrival anchor, while later terrain/tree changes may settle the displayed NPC. NPC placement and open dialogue pages reset on load; existing PeGame progression and one-time supply ownership persist. The source physical player/NPC layer10 self-pair is disabled, so Eitai is not a player movement blocker. None of these checks establishes general town generation or graphical parity.

## Recovered terrain textures

Sandbox terrain now uses the original six diffuse atlases,84 source material slots, source triplanar projection and per-triangle material blending. Grass, soil and exposed stone remain textured after digging and reload. Imported terrain retains exact original IDs; the synthetic world's grass/soil/stone use representative source materials3/1/17. Source OpenGL mip selection and tile repetition/insets are retained; lighting, water and world generation are still approximations. [Cutaway capture](../docs/reference/sandbox/rebuild-terrain-layers-reloaded.png), [source audit](../docs/audit/SANDBOX_TERRAIN_MATERIALS.md).

The assets are installed on this machine under `rebuild/build/reference-assets/terrain`. To recover or verify them using the optional offline UnityPy/Pillow environment:

```sh
/tmp/pe-asset-tools/bin/python rebuild/tools/export_terrain_assets.py
make -C rebuild terrain-material-test
make -C rebuild terrain-assets-check terrain-asset-test ASSET_PYTHON=/tmp/pe-asset-tools/bin/python
```

The C game loads ordinary PNGs and has no Unity dependency. Unsupported imported material IDs render magenta instead of silently choosing another material. The recovered normal atlases are retained for future shader work; the inspected source OpenGL shader itself uses geometric normals for lighting.

## Original terrain import

`--terrain` reads an original `.voxelform` file through the native LZ4 reader and extracts a 64 × 32 × 64 density-sample crop at LOD 0. By default it searches for a suitable surface and crop; `--origin X,Y,Z` specifies nonnegative file-local sample coordinates. Raw material IDs and crop origin are preserved in saves. Import fails without replacing state if the file/crop is invalid or no safe player position can be found.

```sh
./rebuild/build/planet_explorers \
  --terrain 'CustomGames/Dragon Desert/Worlds/Dragon Desert/map_x0_y0.voxelform' \
  --save /tmp/dragon-desert-crop.pesave
```

Imported terrain currently has no imported trees, resource props or NPC population. Adventure can place the same bounded reference supplier if supported free space is available; this does not import a town or NPC records. New Adventure still receives its starter inventory. To explore it with tools and materials earned in the generated world, load that save first:

```sh
./rebuild/build/planet_explorers \
  --load /tmp/exploration.pesave \
  --terrain 'CustomGames/Dragon Desert/Worlds/Dragon Desert/map_x0_y0.voxelform' \
  --save /tmp/equipped-terrain-crop.pesave
```

This preserves inventory, equipment, clock, and crafting progress, and replaces world-specific terrain, structures, trees, and player position. It imports terrain samples, not a complete original scene or Unity save.

## Original game reference

The installed original Linux **V1.1.4**, running **Unity 5.2.4f1**, has reached its main menu and the tutorial with Andy on the simulation deck; screenshots verify both. The Unity editor has not been run, and the supplied project has not been rebuilt in Unity. These observations establish an executable reference, not complete game or rebuild parity. The [reference-runtime investigation](../docs/REFERENCE_RUNTIME.md) records the evidence.

For an existing original Linux installation, run from the repository root:

```sh
python3 rebuild/tools/launch_reference.py
```

The helper uses `~/.local/share/Steam/steamapps/common/Planet Explorers` by default and creates a separate profile at `~/.local/share/planet-explorers-reference`. Override either location as needed:

```sh
python3 rebuild/tools/launch_reference.py \
  --game-dir '/path/to/Planet Explorers' \
  --user-data /tmp/pe-reference-profile
```

The profile path must contain no whitespace because of the original command-line parser. `--prepare-only` validates the installation and prepares settings without starting the game. The helper changes `QualityLevel` to 6 and `AntiAliasing` to 0 in the profile's `PlanetExplorers/Config/SystemSaveData.xml`. It also sets the launched process's `XDG_CONFIG_HOME` to the profile's `engine-config` directory and resets only `UnityGraphicsQuality` to integer 0 in that directory's `unity3d/Pathea Games/Planet Explorers/prefs` before every launch. Existing files receive uniquely named backups before changes; other settings and the user's shared Unity preferences are preserved.

Both settings matter: disabling MSAA resolved the observed black window and `GLXBadContextTag` errors, while resetting engine startup quality prevents a stored higher quality from enabling MSAA before game settings load. The helper launches the native 64-bit player windowed at 1280 × 720, in English with CPU terrain extraction, and leaves console output visible. It does not download the game or change Steam settings. Ordinary Steam Play is not established by the isolated launch evidence. The C rebuild itself requires neither this installation nor Steam.

### Automated reference input

On the current Hyprland/Xwayland desktop, the bounded driver can operate the original client without a person holding the controls:

```sh
python3 rebuild/tools/reference_input.py layout
python3 rebuild/tools/reference_input.py key F5
python3 rebuild/tools/reference_input.py key Shift_L+w --duration 1
python3 rebuild/tools/reference_input.py look 200 0
python3 rebuild/tools/reference_input.py click 1045 584
python3 rebuild/tools/reference_input.py shot /tmp/reference.png
```

It targets only the original client title/executable, releases held keys, and validates client-local click coordinates. Keyboard holds are capped at five seconds. Clicks build a bundled Wayland helper on first use with `wayland-scanner`, a C compiler and `pkg-config wayland-client`; screenshots require `grim`, and keys/look require libX11/libXtst. No download or permanent desktop configuration change is involved. It needs the graphical session available and will focus the game, so avoid simultaneous desktop input while it runs. This driver supports controlled observations; it is not a general autonomous-game bot or a complete tutorial script.

## Recovered room and tools

The installed training room and six resource-tool bundles can be converted without Unity. The optional exporter requires UnityPy 1.25.4 in a separate environment:

```sh
python3 -m venv /tmp/pe-asset-tools
/tmp/pe-asset-tools/bin/python -m pip install UnityPy==1.25.4
/tmp/pe-asset-tools/bin/python rebuild/tools/export_reference_assets.py
/tmp/pe-asset-tools/bin/python rebuild/tools/export_reference_assets.py --asset training-kit
/tmp/pe-asset-tools/bin/python rebuild/tools/export_reference_assets.py --asset training-knife
/tmp/pe-asset-tools/bin/python rebuild/tools/export_reference_actor.py
./rebuild/build/planet_explorers --reference-room rebuild/build/reference-assets/room.obj
```

Default output is the ignored `rebuild/build/reference-assets/` directory. Original bundles stay in the installation. The room preview uses source coordinates and recovered diffuse textures; WASD/mouse moves the free camera, 1 returns to the deck view, Home shows the exterior, and Escape exits. It does not load or save a world. Collision geometry is exported separately but not active in this preview. NPCs, original lighting/shaders and holographic effects remain absent.

`--reference-room` also accepts exported tools and supports `--smoke 90 --screenshot /tmp/room.png`. It rejects world/save/panel options. [Asset recovery](../docs/audit/ASSET_RECOVERY.md) documents transforms, material provenance and the guarded workaround for a raylib 6 OBJ parser hang. [Playtest evidence](../docs/PLAYTEST.md) includes the original tutorial and the rendered C room.

## Playable training room

After exporting the assets above, open the recovered room with collision and the supported lesson sequence:

```sh
./rebuild/build/planet_explorers --training-room rebuild/build/reference-assets
```

This mode loads `room.obj`, its materials/diffuse textures, and `room-collision.obj` from the directory. Unlike the free-camera `--reference-room` preview, it moves a player capsule against all three exported source mesh colliders, including the hidden collision mesh. Original Unity world coordinates are preserved. The authored spawn is `(27, 2, 11)`, settled onto the actual deck; missing assets or an unsafe spawn produce an error instead of a substitute room.

The optional `--asset training-kit` export adds `training-kit.obj` and its materials for the storage prop. Room collision and lesson state remain available without that visual prop. The tutorial loads all nine original NPC models from `npcs/{entity_id}/recording.peas`, with their own recovered idle animations, source placements and Andy's scripted orientation. Names appear above nearby visible characters; Andy retains the lesson interaction. NPC bodies do not block the player, matching the original collision layers. Minimal asset sets without NPC scenes retain the earlier Andy marker; partially installed NPC groups fail with an explicit error. Side conversations, patrol/doctor/repair AI and Percy's weapon attachment remain unimplemented. See the [NPC audit](../docs/audit/TRAINING_NPCS.md) for the roster and capture/export process.

| Input | Training action |
| --- | --- |
| WASD | Move relative to the camera |
| Z | Toggle walk 2 m/s / run 5 m/s |
| Shift / Space | Dash at 8 m/s / jump, subject to stamina |
| Middle mouse | Toggle autorun; backward input cancels it |
| F5 | Cycle Third Person → Control Mode → First Person |
| Right mouse in Third Person | Hold to look around |
| Mouse in Control Mode / First Person | Look around; Tab releases or recaptures the cursor |
| Space / Enter / click the dialogue panel | Advance the current source dialogue; the acknowledgement does not also jump or use a tool |
| E / right-click the kit or Andy | Interact during the applicable lesson and within reach |
| I | Open or close the inventory; click category icons to select items, equipment or resources |
| C / click the health display | Open or close character information independently of the bag; drag its model preview to rotate |
| Q / minimap mission button | Open or close the source-style mission window |
| Y | Open or close the tutorial Replicator |
| Drag Herbal Juice to quick-bar slot 1 | Assign the stack during the quick-bar lesson |
| 1 / right-click quick-bar slot 1 | Consume assigned Herbal Juice when its ten-second cooldown permits |
| Inventory Equipment tab, right-click an unequipped weapon | Equip the owned knife, Safety Axe or selected shovel instance |
| Character weapon slot, right-click | Unequip the same weapon back into the bag |
| Bottom menu button | Open the supported Inventory, Character, Replicator, Mission and Pause commands |
| Left-click in combat | Draw, then attack while stopped; click during combo window to chain (combat assets required) |
| Short right-click in combat | Stow the drawn knife |
| Escape | Close an active panel, otherwise pause/resume; the cursor is released while paused |
| Q while paused | Exit the training session |

The supported sequence is **739 → 747 → 718 → 740 → 741 → 742 → 743 → 744 → 749 → 745 → 746 → 719 → 748 → 757 → 721 → 751 → 752** with the corresponding optional assets. Construction additionally implements **752 → 753 → 756**, with 756 an unsupported creation-lesson boundary and the 15,000-frame graphical check passed. The 6000-frame graphical check passed through 748 with five herbs depleted and one axe. Andy’s original welcome300215 and Tips300258 precede camera input. Space, Enter or a dialogue click advances them; camera and movement events then complete the first two lessons, followed by the five original interface explanations. Walk to the materials at `(7,1.5,12)`, open the kit, transfer five Herbal Juices, and return to Andy at `(32,4,12)`. Complete the reply and medicine dialogue, then open inventory: accepting lesson 743 grants five additional juices, giving ten. Assign the stack to quick-bar slot 1 and press 1 to consume one, leaving nine. Accepting 745 grants one knife and opens the original equipment dialogue. Open I, choose Equipment, and right-click the knife to equip it. This advances to **746, the combat lesson**. With the matching baked body, drawn knife and hare hitboxes installed, left-click draws the knife, another click starts a stationary third-person attack, and a click during its combo window queues the next stage. A short right-click stows it. A real hare death opens completion dialogue 300230, then 719’s gathering introduction. With the recovered herb assets installed, approach each plant and press E. Five timed depletions enter **748**, grant one Safety Axe and show the original lumbering introduction. With the recovered axe/cutting and shovel assets, the next lessons continue through tree cutting, shovel crafting and real mound excavation to 752, as documented below. Without the optional combat assets, the equipment lesson remains the available boundary. Events and inventory changes are gated by the applicable lesson; repeated actions do not duplicate the kit transfer or mission grant.

The English dialogue preserves source text and IDs. The compact inventory, quick bar, dialogue panel, log, and status display represent the supported subset of the original HUD. The Items/Equipment tabs cover only juice and knife; full mission/NPC interfaces, original UI layout and input timing are incomplete. Right-click uses a target ray; the E interaction path is also subject to the controller's reach checks. See [interaction provenance](../docs/audit/TRAINING_INTERACTION.md).

The source camera starts at a 6 m follow distance, yaw 30°, pitch −20°, and 60° field of view. The C direction convention represents that same direction with yaw 150°; this does not reflect the room coordinates. Mouse pitch is limited to −70°…55° in Third Person/Control Mode and −75°…75° in First Person. Camera clipping uses a simple obstruction ray. Mouse sensitivity, immediate rotation, fixed eye/target heights, and the absence of original camera damping, activity-space adjustment, animated targeting, and zoom remain provisional.

The fixed 60 Hz controller uses a 0.30 m radius, 1.80 m tall capsule and static triangle collision with sliding and steps. Recovered movement speeds and stamina rules are present, but this is a new controller rather than the Unity Rigidbody/action system: jump impulse 7 m/s, gravity 20 m/s², instant acceleration, air steering, collision skin, and step response remain substitutes. The jump action ends on landing or after the source three-second timeout, allowing stamina recovery during a longer fall. Optional recovered starter-character assets supply six locomotion clips; absent assets retain a blue capsule. Original lighting and shader effects remain absent. See the [asset recovery](../docs/audit/ASSET_RECOVERY.md) and [tutorial audit](../docs/audit/TUTORIAL.md) for source evidence and limits.

Training is a separate session with Herbal Juice, one owned knife, a lesson resource ledger and one quickslot. HP starts at 500/500. Juice immediately restores 30 HP, applies the source timed healing buff, and starts a ten-second cooldown; it neither restores stamina nor revives a dead player. The fixed-step implementation preserves the source strict tick/expiry order, including the final tick at step 601. Reusing replaces the buff. Stationary knife damage and prototype 13 death progression are implemented when combat assets are installed. Training includes the source wood-shovel recipe in lesson 721; mission save/load remains absent and exiting resets progress. See [equipment and medicine evidence](../docs/audit/TRAINING_COMBAT.md). Training cannot be combined with `--reference-room`, terrain, save/load, or smoke-panel options. The tutorial module exposes an independent snapshot, but neither the training frontend nor PECS saves currently persist the training interaction/inventory state. The sandbox retains its separate inventory, saves, and gather/craft/build checklist.

A short graphics check exercises the camera and movement lessons; a longer check walks the supported sequence and exercises inventory/quick-bar use:

```sh
./rebuild/build/planet_explorers --training-room rebuild/build/reference-assets \
  --smoke 90 --screenshot /tmp/pe-training.png
./rebuild/build/planet_explorers --training-room rebuild/build/reference-assets \
  --smoke 1500 --screenshot /tmp/pe-training-sequence.png
make -C rebuild training-sequence-smoke
```

The 90-frame check expects mission 718 after the first six camera/movement objectives. The 1500-frame check expects mission 746 with nine Herbal Juices and the owned knife equipped after storage, Andy, inventory, assignment, use, and equipment; unmet expectations, asset loading failures, and screenshot failures return errors. Core, sanitizer, and CLI checks pass. Earlier graphics checks reached 718, and the previous 1500-frame storage/return run reached its then-supported boundary at 743 with five juices. The extended 1500-frame graphical run now passes at 746 with nine juices, an equipped knife, full HP and all entry dialogue acknowledged. A separate keyboard/mouse playthrough also passed: dropping outside the quick bar kept mission 744, an empty slot, and ten juices; a valid drag advanced to 749 with ten; pressing 1 reached 745 with nine. That earlier session completed after 12,753 frames with both camera-mode objectives and all four movement objectives recorded.

A later 38,308-frame keyboard/mouse playthrough reached 746 with nine juices, an equipped knife and 500/500 HP; unequipping and re-equipping conserved the same knife. The newest 1500-frame graphics run also exercised leisure patrol, reporting mode 1, one decision, and hare position `(3.293,1.632,17.898)`. At that stage, core/sanitizer checks, fifteen CLI tests and the new headless combat/actor/hitbox modules passed; the current suite has eighteen CLI cases. See the [playtest evidence](../docs/PLAYTEST.md) for completed observations. `make -C rebuild training-smoke` repeats the short check.

## Recovered tutorial interface

Training now uses the recovered source UI atlases, bitmap fonts, window layouts and item/portrait sprites. The bottom status/shortcut frame, Andy/Tips dialogue, inventory and storage grids, character preview, mission tracker/log, Replicator and supported menu commands replace the earlier rectangular text panels. The tutorial begins with Andy’s actual simulation-deck welcome and the source third-person camera explanation. Training and sandbox use the same recovered UI assets through separate hosts; lesson state and general PeGame state remain distinct.

The export contains 2,474 sprite records across three original PNG atlases, eight source bitmap-font exports and the original TTF files. Source effects add 13 bounded specular/scanline overlays using 11 original mask/noise PNGs and translated GLSL. The room minimap uses the original TrainingRoomMinimap bitmap, source crop/shader behavior, player heading and the nine NPC markers. Quest/localization export supplies 19 original mission titles/descriptions and 24 ordered objectives, including empty title718 and the source counter rules. See [UI audit](../docs/audit/TRAINING_UI.md), [effect provenance](../docs/audit/UI_EFFECTS.md) and [source layout](../docs/audit/TRAINING_UI_LAYOUT.json).

For a fresh UI destination, recover assets with the optional UnityPy/Pillow environment used by the other source exporters:

```sh
/tmp/pe-asset-tools/bin/python rebuild/tools/export_training_ui.py \
  --output rebuild/build/reference-assets/ui
python3 rebuild/tools/export_training_ui_text.py --check
./rebuild/build/planet_explorers --training-room rebuild/build/reference-assets
```

The asset exporter requires a fresh destination; it does not overwrite the Unity project or original installation. Existing recovered UI assets can be reused. The bitmap-font metrics, portraits and frame sprites are original source assets; full equivalence to the shipped Unity rendering, texture compression and postprocessing is not claimed.

C opens only character information; I opens the bag independently. Equipped weapons leave their bag slots and appear in the character weapon slot; right-clicking that slot returns the same instance without repairing or duplicating it. Opening storage also opens the bag. Inventory category clicks, equipment right-click, script use and juice drag/drop call the existing headless transactions. The source quickbar displays an equipped-item cell plus nine assignable cells, but only assignable slot1/Herbal Juice is currently functional. The other slots/pages, full armor equipment, general inventory sorting/splitting/discard, arbitrary item use, arbitrary recipes/quantity/cancel controls, complete quest history and general options menus remain unsupported. The mission window shows the current real lesson and no fabricated completion rewards. Other storage-category buttons and general window dragging remain unsupported; character-preview dragging only rotates the model.

Headless coverage includes six generated-text tests, source layout/asset checks, character-panel independence from lesson743, conserved equipment instances and rejected modal actions. The full tests and positive UI core harness pass after adding character state and startup dialogue. Runtime font switching uses native32-pixel headings,18-pixel dialogue/tabs and26-pixel overhead names rather than authored37/20/30 scales. Manual checks passed at1280×720 for welcome/Tips, I/C/Q/Y windows, the source blue cursor, inventory category clicks and close button, mission close and menu→Replicator. A954×1048 portrait window also passed character display and drag-to-rotate preview checks. The new UI’s15,000-frame rerun reached mission756 with the expected combat, gathering, crafting, mining and construction state. The final tracked15,000-frame run after visual polish passed with exit0. See the [final run log](../docs/reference/rebuilt-ui-result.txt), [completed frame](../docs/reference/rebuilt-ui-complete.png) and [UI evidence](../docs/PLAYTEST.md#original-source-ui-integration--manual-checks-and-complete-progression). Earlier gameplay smoke results below predate this UI replacement unless explicitly stated otherwise. Dialogue blocks movement and F5 in this C host; that blanket modal policy is not established as source-equivalent.

## Recovered animated hare

The optional actor exporter above adds `hare.peac` and `hare-diffuse.png` to the reference asset directory. The C renderer skins the original 1,766 vertices against 48 recovered bone palettes. Preview it with:

```sh
./rebuild/build/planet_explorers --reference-actor rebuild/build/reference-assets/hare.peac
make -C rebuild actor-smoke
```

The preview supports 1 for idle, 2 for run, Left/Right to select any clip, R to restart, Space to pause, right-drag to orbit and the wheel to zoom. The export also contains five original melee clips and walk. Its earlier 90-frame graphics check passed through idle/run, and a 630-frame check passed while cycling all eight clips. The 630-frame check also verified the source hitbox overlay. Training optionally displays the hare at the source combat spawn, settled onto room collision, at deterministic scale 1. It now uses a source-informed leisure patrol with the recovered walk/idle clips. Its scheduler, random sequence, capsule controller and ledge avoidance are native approximations; perception, hostile pursuit, hare attacks and ragdoll remain unimplemented. The player’s stationary knife attacks can now damage this posed hare and dispatch death progression when combat assets are installed. [Movement provenance](../docs/audit/HARE_MOVEMENT.md) distinguishes source values from these choices. Optional baked knife assets follow the recovered animated waist mount; otherwise the static waist proxy remains. [Actor recovery](../docs/audit/ACTOR_RECOVERY.md) explains the sampled animation format and interpolation limits; [hitbox recovery](../docs/audit/HITBOX_RECOVERY.md) records source capsules and their skeletal mapping. The stationary training melee host now connects the player contact/timing modules to attacks and damage; the 3600-frame graphical check passed through 719. [Player animation](../docs/audit/PLAYER_ANIMATION.md) records the recovered knife events and remaining humanoid retargeting work. Original-client observations confirm knife grant, equipment and drawing; they do not confirm a successful hit or kill.

## Player visual research

The optional player exporter recovers the default male/female modular meshes, diffuse textures, skin weights, inverse binds and Avatar data into a separate ignored directory:

```sh
/tmp/pe-asset-tools/bin/python rebuild/tools/export_reference_player.py
```

Output defaults to `rebuild/build/reference-player/`. This produces static Avatar-default-pose OBJs and decoded Attack6 scalar data, not a playable animated character. Headless recovery and repeatability tests pass. Subsequent original-runtime pose capture and isolated clip baking now supply the animated training character described below. See [player asset recovery](../docs/audit/PLAYER_ASSET_RECOVERY.md) for exact source identities, mathematical checks and the unresolved humanoid retargeting step.

## Sandbox controls

| Input | Action |
| --- | --- |
| WASD / mouse | Move / look |
| Shift / Space | Sprint / jump |
| Z / middle mouse | Toggle walk/run / autorun; backward input cancels autorun |
| Mouse wheel, third person | Zoom from 1.8–10 m, or 1.8–20 m in construction; modal UI consumes scrolling |
| E / right mouse while aiming at Eitai | Open the eligible supplier's source choice window |
| Enter / dialogue arrow | Select the supply question or advance dialogue; final4094 acknowledgment grants supplies |
| Hold left mouse, outside construction | Adventure shovel12/pickaxe19/axe26/1527: draw/use while grounded, stationary and in third person |
| R / right mouse, outside construction and supplier interaction | Stow and cancel the current timed tool action |
| B | Enter/exit cursor-based construction |
| Left mouse / Alt + left mouse / X, in construction | Place / remove / remove at the pointer |
| Hold right mouse, in construction | Turn the camera |
| 1–9, outside construction | Use assigned quickslots; resource/furniture shortcuts select placement items |
| 1–7, in construction | Cube / dirt / stone / wood / ramp / fence / ladder; independent of normal quickslots |
| V / T / G | Cube/ramp / quarter-turn rotation / one or four cells |
| Y | Replicator; learned recipes only in Adventure |
| I / C / Q | Inventory / character / missions |
| Inventory right-click | Use, learn or equip a supported item |
| Inventory drag | Assign a quickslot |
| Quickslot right-click / drag | Use / rearrange shortcuts |
| Character weapon right-click | Unequip the tool |
| Tab / F5 | Free cursor / first- or third-person camera |
| F9 / F10 | Save / load the configured file |
| Escape | Close a panel, or pause/resume |
| Pause buttons | Resume, Save, Load or Main Menu |

Panels stop movement but allow simulation and crafting to continue. Escape pause stops simulation. Zoom starts at the source 6 m distance and uses the scalar Unity 5.2 SmoothDamp algorithm with a .5 s smooth time. Wheel input is gated by world-input ownership; unpaused third-person smoothing continues while panels block new wheel input. Pause, lost focus and first person freeze this clock. The .8 m-per-notch adapter and native collision offset remain approximations. Original activity-space scaling, two-second clamp recovery and the remaining CameraForge graph are not ported; vegetation does not gain camera blocking or fading. See the [camera source audit](../docs/audit/SANDBOX_CAMERA.md). These are rebuild controls, not the original game's complete action map.

## Implemented systems

- A seeded test world or imported original terrain crop, with two-byte density/material samples and the original isosurface threshold of 128. CPU marching cubes uses tables exported from the original source. Four bounded render regions, up to 32 cells per axis, cache GPU meshes; byte comparisons include the normal-gradient halo so edits rebuild affected regions and necessary neighbors.
- Mesh-consistent terrain ray picking/heights, safe spawning, walk 2 m/s, run 5 m/s, sprint 8 m/s, jumping, slope steps, terrain collision, and shape-aware Block45 collision. Z toggles walk/run; middle mouse toggles autorun.
- Seventy-two source-derived item definitions and twenty-one recipe records, with original IDs, stack limits, ingredient/output counts, durations, and tool/player properties. Thirty-two shared inventory slots include stable non-stack item identities and persistent per-instance durability.
- Equipped pickaxes, shovels, and axes. Mining reproduces the source voxel-box indicator's threshold search, neighbor support, and snapped sample selection, including nonzero samples below the mesh isovalue. Reach is strictly under 3 m from feet plus one meter to the sample. Damage uses source attack/resource modifiers, hardness attenuation, byte-density reduction, and primary resource drop distributions. Known materials with no drops can still be removed. Unknown resource definitions are rejected without inventing a yield.
- Adventure tool host for exact items12/19/26/1527, with source entry/loop/exit gates, authored DigTerrain/Fell events, serialized tool speeds and stamina slowdown. Target locking, equipment-instance binding and cancellation prevent duplicate edits. Recovered female body and exact tool meshes read this shared clock. Wood Pickaxe19 uses its recovered Mine1–3 body/tool poses, entry speed1.5, source strike events, stamina4 and50/20 slowdown thresholds. Source controller blending/IK and arbitrary gender poses are not reproduced. See [action rules](../docs/audit/SANDBOX_ACTIONS.md) and [tool presentation](../docs/audit/SANDBOX_TOOL_PRESENTATION.md).
- Recovered tree geometry and diffuse foliage replace the generated world's cylinder/cone visuals. One original tree mesh is reused at provisional positions with unit scale; source alpha cutoff and the matching source trunk capsule support rendering and axe interaction rays. Adventure player movement collides with live source capsules; depletion removes their rendering and collision. Legacy movement is unchanged. Biome/species selection, wind, LOD, original resource-size modifiers and stump/felling simulation remain absent.
- One archived Adventure supplier with source identity, saved appearance, original idle evaluation, source capsule picking and a recovered NPC choice/dialogue window. Arrival and final acknowledgment drive the existing conserved progression/inventory transaction. Local supported placement is provisional; supplier graphical/input verification is pending.
- Source stamina capacity 100 and recovery speed 40, tool stamina/durability costs, and recovery delayed until an active action finishes plus at least 0.1 seconds. The player prefabs require 15 stamina to start sprinting and drain 5/s; a running sprint continues below 15 until exhausted. Jumping requires and consumes 10 stamina. Tool swings slow below source stamina thresholds; empty stamina does not prevent swinging, while broken tools do.
- One crafting job at a time, total input availability checked at start, inputs consumed per completed batch, and completed products held for source-style delivery: on a batch completion when more than five seconds have elapsed since delivery, or on the final batch. Three-second recipes therefore first deliver two products at six seconds in a longer job. Cancellation preserves unfinished inputs and collects completed products; insufficient output space preserves the pending job.
- Original Block45 half-meter cube/ramp geometry, packed shape/rotation/material bytes, four rotations, and grouped `ceil(cells/4)` build costs / `floor(cells/4)` refunds. Entire batches validate before inventory or world mutation. Crafted fence/ladder proxies and structures from older rebuild saves retain their separate geometry.
- A 26-hour clock at the source's normal 12× rate, starting at day 94.35.
- `PECS` save version 5 with explicit little-endian fields, integrity checks, bounded semantic validation, temporary-state loading, and atomic file replacement. Rebuild save versions 1–3 migrate on load as Legacy mode (original controller and all exposed recipes known). Version 4 Adventure loads first validate non-tree geometry, then resolve an existing tree overlap using same-height radial candidates in .05 m increments, up to 3 m away. Candidate paths sweep non-tree geometry, avoid new tree intersections and move outward from initially overlapping trees. Velocity and saved clocks are preserved; grounding is recalculated. No safe candidate means the load fails without replacing current state. Version 5 requires a valid full-collision spawn and does not apply this rescue. Unity saves are not supported. Terrain metadata, RNG, tool identity/wear, stamina, pending craft output, construction, trees, player state, Adventure grants, recipe knowledge, nine quickslots, HP and juice clocks persist.

## Validation

`make -C rebuild menu-smoke` checks the main menu. `make -C rebuild adventure-smoke` creates source-backed progression fixtures, then checks the character, Replicator and an airborne save through the graphics host. These commands open game windows. `rebuild/build/adventure_fixture --inspect SAVE` summarizes a rebuild save without graphics. The bounded `rebuild/tools/rebuild_input.py` driver targets only the three exact local application titles and the compiled executable; use one desktop operator and the current PID.

To exercise the three source-timed tool fixtures with actual mouse input, build the executable and fixture helper, then run:

```sh
make -C rebuild all build/sandbox_action_fixture
python3 rebuild/tools/playtest_sandbox_actions.py
```

This bounded Hyprland/Xwayland playtest launches only the local C rebuild, creates isolated shovel/pickaxe/felling fixtures, holds left mouse for eight seconds per action, saves through F9 and verifies resource changes, tool-instance identity and wear. It releases input and closes its owned windows on failure. Use one desktop operator and leave input undisturbed during the run. Default artifacts are fixture/result saves, logs, screenshots and `result.json` under `rebuild/build/sandbox-action-playtest`; `--output-dir DIR` selects another destination. It does not load or overwrite a normal user save. The physical-input tool playtest passed with the source 6 m camera fixture: two shovel swings consumed 10 durability and produced 2 dirt; one pickaxe Mine strike consumed 5 durability and produced 2 dirt; six wood-axe swings consumed 30 durability, depleted tree7 and produced 8 wood. All three retained the same tool instance and unrelated possessions. The pickaxe fixture loses its valid target after that sample is removed; held input does not imply further resource edits. See the [playtest record](../docs/PLAYTEST.md) for artifacts and the separate target-preview inspection status. This validates the bounded C fixtures, not original-game animation or resource parity.

```sh
make -C rebuild test
make -C rebuild sanitize
make -C rebuild check-content
make -C rebuild cli-test
make -C rebuild asset-test
make -C rebuild sandbox-world-test
make -C rebuild sandbox-camera-test
make -C rebuild smoke
```

`test` runs C world, marching-cubes, original-file reader, Block45, gameplay, persistence and initial tutorial suites, plus content, launcher, reference-input and asset-conversion Python tests. The installed-asset repeatability test is optional and runs when UnityPy and the installation are available. `asset-test` checks the guarded model parser without opening a graphics context, but links raylib. `sandbox-world-test` checks recovered tree loading, ownership/cleanup, depletion visibility and interaction-ray behavior through headless graphics stubs. `sandbox-camera-test` checks the independently verified Unity 5.2 damping values, frame-rate variation, overshoot, pause/input gates and normal/build bounds. `sandbox-ui-test` covers real inventory actions and build-toolbar/modal input ownership; `sandbox-actor-test` checks shared action pose clocks and exact tool groups. These headless checks are also included in `test`; they do not substitute for graphical inspection. Launcher tests cover isolated settings, repeat-launch quality reset, backups, malformed XML, and process arguments/environment. Fixtures cover source geometry/rotations, original terrain samples, malformed compressed files, movement, tool wear, stamina recovery, primary yields, pending crafting/cancellation, grouped construction transactions, save migration, and semantic tampering with recomputed checksums. `sanitize` runs the C suites under AddressSanitizer and UndefinedBehaviorSanitizer with leak detection. `check-content` verifies both generated database content and marching-cubes tables. `cli-test` exercises argument validation and startup failures.

`smoke` requires a display. It creates a dedicated save fixture under `rebuild/build/` through normal game APIs: gather wood, craft/equip a pickaxe, mine terrain, place four cube and four ramp cells, and queue five fences. It then renders both the inventory/equipment panel and an imported original terrain crop to screenshots there. It does not replace the user's save. Individual unattended runs are also available:

```sh
./rebuild/build/planet_explorers --smoke 90 --screenshot /tmp/pe-smoke.png
./rebuild/build/planet_explorers --load /tmp/exploration.pesave \
  --smoke 90 --smoke-panel inventory --screenshot /tmp/pe-equipment.png
```

After the render-cache and placement fixes, the optimized C suites, ten Python content tests, eleven CLI tests, generated-content checks, and ASan/UBSan tests with leak detection all passed. The generated-world and imported-terrain smoke runs each completed 90 frames and were visually inspected: 9,765 and 9,644 terrain triangles respectively, with five region rebuilds each (four initial uploads and one edited region). These checks do not establish every input path, other operating systems, or original-game parity. The reference-runtime investigation records the limits of original-runtime comparisons.

The latest live-tree movement/save-v5 and smoothed-camera integration passed physical-input verification: trunk contact, six axe swings, depletion and walking through the cleared space; visible v4 overlap migration followed by v5 save/reload; and zoom6→3.6m with character-panel wheel consumption. The three Adventure graphics smokes also passed. Reproduce the bounded desktop check with `python3 rebuild/tools/playtest_sandbox_trees.py` after building `all build/sandbox_action_fixture`. It uses isolated saves and captures under `rebuild/build/sandbox-tree-playtest`, verifies fresh F9 writes and closes only its own C windows. An unlocked desktop and one operator are required. See the [playtest record](../docs/PLAYTEST.md) for original-versus-C evidence and remaining limits.

## Content provenance

`tools/export_content.py` reads the original SQLite files without modifying them. Its generated manifest records source hashes, selected IDs, schema mappings, tool/player fields, resource data, and limitations. The bounded catalog contains 72 items and 21 recipes: its initial tools and construction outputs are expanded by the source Adventure supply scripts and their ingredient closure. Adventure learns the corresponding formulas by consuming supported scripts; Legacy exposes all exported formulas. Supported swords, knife and bow can occupy the Adventure hand-equipment slot, but their general sandbox attacks are not implemented. The separate training scene implements knife33 equipment and a bounded stationary combat path.

`tools/export_mc_tables.py` exports the original terrain extraction tables with provenance. Generated C data is checked in, so normal compilation needs neither Python nor runtime SQLite.

```sh
make -C rebuild content
make -C rebuild check-content
```

Mining `resource` records, smooth-terrain placement `buildblock`, and Block45 `blocktype` mappings are distinct. See the [content audit](../docs/audit/CONTENT.md) and [Block45 audit](../docs/audit/BLOCKS.md). The original [source and asset terms](../README.md) still apply; no new license is assigned to source-derived content. raylib and LZ4 remain external libraries with their own terms.

## Remaining limitations

The native world is bounded: there is no streamed full planet, original procedural generator, terrain LOD transition pipeline, water simulation, full vegetation-placement import, or general scene graph. The optional training mode imports one recovered static room with its mesh colliders. Source marching-cubes tables are reproduced, but the original GPU pipeline, material shaders, and streaming chunk lifecycle are not. The four-region render cache optimizes edits within the bounded volume; it does not stream terrain. Adventure uses capsule sweeps against the actual sandbox marching-cubes, Block45, furniture and live source tree capsules; Legacy retains its earlier cylinder/trilinear-density controller. Training sweeps capsules against its recovered room and active lesson geometry. These native controllers are not the original Unity physics implementation. Walk/run/sprint speeds and stamina rules now follow recovered player data, but jump velocity 7 m/s, gravity 20 m/s², instant acceleration, and airborne steering remain temporary. See the [tutorial and movement audit](../docs/audit/TUTORIAL.md) for serialized values, source-default discrepancies, and the original mission sequence.

Generated trees now share an original recovered mesh, but their placement, unit scale, eight-wood yield and unit resource/size factor remain provisional; source maximum health is 255. Their source trunk capsules support axe targeting and Adventure player movement collision, while camera queries exclude trees. Bare-hand loose-wood gathering remains an explicit bootstrap substitute. Trees do not fall when support is removed, and depletion hides them without a stump or falling animation. The water plane is decorative. Adventure Wood Shovel12, Wood Pickaxe19, Wood Axe26 and Safety Axe1527 dispatch through the general source-timed action host and matching poses. Legacy and unsupported tool variants retain generic cooldown pacing. The pickaxe uses its own Mine poses and exact recovered mesh; it does not substitute shovel presentation. Both the general action host and tutorial hosts approximate controller transitions discretely. The full action/skill/buff interpreter is absent. Mining currently covers the selected tools' single-sample radius and primary drops; rare/special bonuses, progression modifiers, and larger tool shapes are not implemented. Its persisted deterministic RNG is not Unity's RNG sequence.

Only Legacy mode unlocks all exposed recipes. Adventure starts with no learned formulas and implements bounded Arrival/supply/script progression, not the full campaign or settlement logic. Inventory presents original category tabs over 32 shared backing slots; this is not the original per-category capacity system. Full item components, repair, workspaces and equipment slots remain absent. Crafts pause safely if inventory conditions change, retaining pending output; this intentionally avoids legacy loss/failure paths. Saves preserve movement velocity and stamina but clear held sprint/jump action flags; an airborne reload does not resume the jump action's regeneration lock. General tool-action stages are transient and are cancelled on load. The provisional legacy digging wrapper rejects an empty selected sample without wear, whereas the Adventure source-timed shovel preserves attempted-swing wear for a valid locked empty sample. Later invalid indicators suppress the attempted event; capacity-blocked edits preserve inventory, density, RNG, wear and the continuing charge.

Block45 supports only cube and ramp, bounded to 1,024 cells. Other primitives, extended blocks, face-mask culling, structural physics, and full brush/undo tools remain absent. Top placement can leave a gap of less than half a meter above smooth terrain. Fence/ladder props and legacy rebuild blocks retain full one-meter collision proxies; ladders are not climbable. Removing support does not collapse structures.

The optional training mode now connects recovered player/hare animation, stationary third-person knife contacts, source damage and a death-driven transition from 746 to 719. Its 3600-frame graphical combat check passed through 719. This is not general combat parity: normal locomotion clips remain in use while armed; drawing/stowing switches attachments immediately; camera-forward aiming is provisional; permissions use a fixed tutorial-pair whitelist; and the hare retains leisure behavior without hostile attacks or ragdoll. Capsule contacts are instantaneous, without source swept history. Moving/air/first-person attacks, controller transitions, root motion, hit reactions and procedural IK remain unfinished.

Audio, complete UI layout, general NPC AI, full campaign/dialogue, reputation, colony/farming, the creation editor, vehicles and multiplayer remain unimplemented. The training frontend now drives herbs, tree cutting, script learning, crafting and real mining, graphically verified through 752. Construction and the 50-ray receiver puzzle are graphically verified through the 756 boundary. Optional local imports provide the room, tools, kit, body, knife and hare; the Unity editor/source build remains constrained by missing third-party dependencies. Test success establishes this supported subset, not original-game parity.

## Code map

| File | Responsibility |
| --- | --- |
| `src/pe_world.c` | Density, generation, ray/collision queries, edits and material interpretation |
| `src/pe_mesh.c` | CPU extraction using source marching-cubes tables and region invalidation checks |
| `src/pe_legacy.c` | Read-only original `.voxelform`/LZ4 access and bounded crops |
| `src/pe_block.c` | Source Block45 geometry, rotations, collision and rounding rules |
| `src/pe_game.c` | Movement, tool instances, stamina, inventory, production and construction |
| `src/pe_tutorial.c` | Source mission IDs, event-driven initial lessons and independent snapshots |
| `src/pe_reference.c` | Guarded recovered OBJ/material loading and visual preview |
| `src/pe_actor.c`, `src/pe_actor_view.c` | Bounded sampled-actor loading, CPU skinning and rendering |
| `src/pe_hare.c` | Source-informed bounded leisure patrol |
| `src/pe_combat.c`, `src/pe_hare_attack.c` | Source action timing, stamina and damage/window rules |
| `src/pe_training_melee.c`, `src/pe_knife_contact.c` | Stationary training action host, posed knife contacts, per-stage deduplication and HP |
| `src/pe_hitbox.c` | Recovered skeletal capsules and headless contact queries |
| `src/pe_scene_collision.c` | Bounded triangle OBJ reader, static BVH, ray and capsule queries |
| `src/pe_training.c` | Fixed-step recovered-room controller and tutorial action events |
| `src/pe_training_dig.c`, `src/pe_training_terrain.c` | Source shovel events, actual mound edits, shared render/collision data and resource receipts |
| `src/pe_training_build.c`, `src/pe_training_receiver.c` | Transactional lesson construction and exact50-ray receiver scoring |
| `src/pe_training_text.c` | Exact source English dialogue and ID provenance |
| `src/pe_training_view.c` | Playable training cameras, input, objective HUD and graphics smoke |
| `src/pe_save.c` | Validated versioned persistence and migrations |
| `src/pe_sandbox_actions.c` | General source shovel/pickaxe/axe action events and authoritative pose clocks |
| `src/pe_adventure_npc.c`, `src/pe_adventure_npc_view.c` | Deterministic bounded supplier placement, source picking geometry, archived appearance and portrait |
| `src/pe_adventure_dialogue.c` | Source arrival test, supplier choice/pages, role-token binding and final acknowledgment handoff to PeGame |
| `src/pe_sandbox_actor.c`, `src/pe_sandbox_world_view.c` | Recovered body/tool poses and shared tree rendering/interaction queries |
| `src/pe_sandbox_ui.c`, `src/pe_sandbox_ui_input.c` | Real sandbox state, recovered panels and bounded build-toolbar input ownership |
| `src/pe_sandbox_camera.c` | Headless desired orbit distance, verified scalar SmoothDamp and mode/input gates |
| `src/pe_tree_collision.c`, `src/pe_sandbox_collision.c` | Source tree capsules and transactional geometry snapshots for Adventure movement/save validation; separate tree-excluding camera queries |
| `src/pe_sandbox_view.c` | Sandbox input, camera placement, regional rendering and fixed-step scheduling |
| `src/main.c`, `src/pe_app_menu.c` | Command-line entry, main-menu rendering and host routing |
| `generated/` | Exported content, extraction tables and provenance |
| `tools/` | Reproducible offline exporters and isolated original-game launch helper |
| `tests/` | Headless, content, CLI and graphics fixtures |

Next milestones are the unsupported creation lesson 756, streamed original terrain with LOD/neighbor edits, and fuller tool/resource/action fidelity. The specification defines the longer path through combat, narrative, settlements, creations, and multiplayer.

## Recorded original player poses

The optional original-runtime probe captured 566 evaluated female player poses, including all four stationary knife stages in both views. [Capture instructions](tools/REFERENCE_POSE.md), [analysis and exports](../docs/audit/POSE_CAPTURE_ANALYSIS.md), and [clock calibration](../docs/audit/PLAYER_ANIMATION.md) distinguish measured behavior from remaining Animator work. A multipart recording can be played directly in the C renderer:

```sh
./rebuild/build/planet_explorers --reference-actor-scene \
  rebuild/build/pose-analysis/female-starter-tp-attack6/recording.peas
```

Space pauses, R restarts, Left/Right step sampled frames, Up/Down select clips, right-drag orbits and the wheel zooms. `--smoke N --screenshot /tmp/player.png` runs a bounded graphics check. The loader retains the exact validated actors and decoded textures through upload, so disk changes cannot replace validated geometry. The replay includes original transitions and IK; it is sampled animation evidence, not a generalized humanoid Animator.

Training optionally loads `player-idle/recording.peas` under its asset directory. To use the recovered starter character after exporting the idle recording:

```sh
mkdir -p rebuild/build/reference-assets/player-idle
cp -a rebuild/build/pose-analysis/female-starter-idle-v2/. rebuild/build/reference-assets/player-idle/
./rebuild/build/planet_explorers --training-room rebuild/build/reference-assets
```

This legacy fallback replaces the capsule visual with the original starter outfit at the first recorded idle pose. Prefer the baked locomotion assets below for animated movement. First-person body visibility remains suppressed. Missing optional character assets preserve the capsule mode; malformed supplied assets fail loading. [Training screenshot](../docs/reference/rebuilt-training-player.png).

A matching captured stowed knife can replace the temporary waist placement:

```sh
mkdir -p rebuild/build/reference-assets/player-knife-stowed
cp -a rebuild/build/pose-analysis/knife-idle-v2/. rebuild/build/reference-assets/player-knife-stowed/
```

It appears only while the knife is equipped. The [knife exporter](../docs/audit/KNIFE_COLLISION.md) also builds a combined attack recording and collision capsule anchored to the same sampled weapon transforms. The optional [headless melee fixture](tests/REFERENCE_MELEE.md) verifies source damage against actual recovered hare geometry at an explicit controlled placement. It does not complete the combat lesson.


## Recovered player locomotion

The isolated original-Unity sampler supplies ten female clips, including six used by training: idle, forward walk/run, jump, fall and land. Once exported, install the optional assets:

```sh
mkdir -p rebuild/build/reference-assets/player
cp -a rebuild/build/animator-bake/player-female/all/. rebuild/build/reference-assets/player/
mkdir -p rebuild/build/reference-assets/player-knife-stowed
cp -a rebuild/build/animator-bake/player-female/stowed-all/. rebuild/build/reference-assets/player-knife-stowed/
./rebuild/build/planet_explorers --training-room rebuild/build/reference-assets
```

The body and equipped knife share the selected clip, authored time, position and yaw. The selector uses measured horizontal displacement and accepted physical jump edges. All six named body clips are required when a baked player scene is supplied, and any supplied baked knife must match their timing. First-person body visibility stays suppressed. Older idle recordings and the capsule remain fallbacks when the optional assets are absent.

This is discrete forward movement animation: directional blend trees, transition crossfades, root motion, weapon locomotion and the separate sprint clip remain unfinished. Sprint temporarily accelerates the run clip. The [locomotion audit](../docs/audit/PLAYER_LOCOMOTION.md) separates source thresholds from these approximations; [bake recovery](../docs/audit/ANIMATOR_BAKE.md) records reproducible pose evidence, earlier cleanup crashes and the subsequently validated bounded-retention sampling workflow.

## Stationary training combat assets and check

Install the matching drawn scene alongside the baked body and stowed scene above. The exporter accepts a closed isolated bake and writes a fresh directory:

```sh
python3 rebuild/tools/export_baked_knife.py \
  ~/.local/share/planet-explorers-bake-reference/pose-captures/female-clips-02.jsonl \
  --rig rebuild/build/reference-player-starter/player-female-starter-rig.json \
  --knife-collision rebuild/build/reference-assets/knife-collision.json \
  --visual-assets rebuild/build/reference-assets \
  --body-scene rebuild/build/animator-bake/player-female/all/recording.peas \
  --attachment drawn --output rebuild/build/animator-bake/player-female/drawn-all
mkdir -p rebuild/build/reference-assets/player-knife-drawn
cp -a rebuild/build/animator-bake/player-female/drawn-all/. rebuild/build/reference-assets/player-knife-drawn/
./rebuild/build/planet_explorers --training-room rebuild/build/reference-assets \
  --smoke 3600 --screenshot /tmp/pe-training-combat.png
```

If `drawn-all` was already exported, use the copy commands without rerunning the fresh-directory export. The reference directory also needs `hare.peac`, `hare-diffuse.png` and `hare.pehb`. A supplied drawn scene must match the body’s named clip metadata and contain `knife-drawn.pehb` anchored to its first material actor. Missing optional drawn assets leave combat unavailable; malformed supplied assets fail loading. The current host uses the recovered female stationary Attack6–9 clips. [Attachment and capsule provenance](../docs/audit/BAKED_KNIFE.md).

The 3600-frame fixture first completes the normal tutorial interaction sequence. It then follows the actual hare, stops within its configured 1.05 m approach distance and attacks through the same action/contact APIs. Success requires actual contacts, HP reaching zero, the one-time death receipt, acknowledgement of 300230 and both 719 intro lines, while conserving nine juices and the equipped knife. It does not set target HP or mission completion directly. **The 3600-frame graphics check passed:** eight attack starts produced 24 actual capsule contacts, hare HP fell from 212 to 0, and the run reached 719 with all six locomotion states exercised, nine juices, the owned/equipped knife and 500/500 player HP. It used real pursuit and room movement, without teleportation or direct HP assignment. [Completed sequence](../docs/reference/rebuilt-training-combat-complete.png). The standalone reconstructed-contact fixture, complete core tests, 18 CLI cases and ASan/UBSan/leak checks also pass. A second 3600-frame run repeated the same 24 contacts. The [first-contact pose](../docs/reference/rebuilt-training-melee-contact.png), captured after two display updates, shows coherent blade grip and hare placement with target HP 204/212. A successful original-client hit or kill remains unobserved.


## Training herbs and next-lesson boundary

The optional exporter recovers all five original herb meshes and the tree at their authored world transforms. Using the isolated UnityPy environment described above, write a fresh output directory (skip the export if these assets already exist):

```sh
/tmp/pe-asset-tools/bin/python rebuild/tools/export_training_gathering_assets.py \
  --output rebuild/build/reference-assets/gathering
./rebuild/build/planet_explorers --training-room rebuild/build/reference-assets \
  --smoke 6000 --screenshot /tmp/pe-training-herbs.png
```

The 6000-frame fixture includes the full combat path, walks to all five actual herb positions, starts the same timed gathering actions used by E, and requires mission 748, five depleted herbs and exactly one safety axe 1527. It does not teleport or set herb completion directly. **The 6000-frame graphical check passed with exit 0:** mission 748, five herbs depleted and one axe, with nine juices, the owned/equipped knife and player HP 500/500 preserved. Combat again recorded eight attack starts, 24 contacts and hare HP 0/212; all six locomotion states ran. [Completed herb lesson](../docs/reference/rebuilt-training-herbs-complete.png). The complete core suite, 18 CLI cases and ASan/UBSan/leak checks passed (`/tmp/pe-gathering-validation.log`). The 3600-frame fixture intentionally stops at 719 without gathering input.

After acknowledging 4072/300231, E starts the nearest eligible herb within the source strict squared distance `<7`. The source attaches independent input listeners to every herb, so one press could start multiple overlapping eligible plants; the C frontend selects one, while the controller permits distinct concurrent actions. There is no knife-equipment restriction. Movement and new jumps are locked until the 1.1-second depletion, with a 1.966-second local action lifetime; gathering produces no inventory resource. The fifth depletion and the 748 axe grant commit together. Dead players cannot start gathering, and death cancels pending undepleted actions. The source 748 introduction 300233 is shown; this is **748, not 720**. Tree cutting and crafting are now connected and graphically verified below.

Rendering uses recovered diffuse geometry directly. The original hologram camera, glow/distortion, production fade and player gathering animation are not reproduced. No original-client herb interaction has been captured or verified. See [source gathering rules](../docs/audit/TRAINING_GATHERING.md).


The controller and frontend now pass the **9000-frame graphical sequence through748 → 757 → 721 → 751**. Safety Axe and Knife share source slot 16: equipping either returns the other to inventory. Drawing the axe sets the held state; the tree action requires held cutting input, a real player-forward 2m ray hit and an explicit animator-readiness signal. Seven timed hits grant one wood and the next lesson's script 406. Successful script use consumes it, learns recipe 1 and grants 20 wood, giving 21 total. Replicator Y opens the source instruction; starting the three-second recipe spends 10 wood. Completion produces one shovel, then 751 grants its second shovel, leaving 11 wood and two shovels. That 9000-frame fixture stops before mining input; the longer 12000-frame fixture continues through excavation to 752. Scripted tree damage does not invent stamina/durability costs; readiness and draw/stow remain host approximations; the three recovered Felling clips now drive matching body and axe poses. Blocked successor grants preserve earned items and paid work rather than duplicating or discarding them. Closing the replicator does not cancel crafting; cancellation and training persistence remain unimplemented.


## Install recovered cutting and shovel animations

These optional assets use closed, locally generated original-Unity captures. Follow [the supervised bake workflow](../docs/audit/ANIMATOR_BAKE.md) to obtain the ten-clip capture 02 or its validated repeat 06, cutting capture 08 and shovel capture 09. Capture files and original binaries are not bundled. Every weapon scene must be exported against the exact body scene it accompanies. The commands below use the existing ten-clip body from capture 02. Exporters require fresh output directories; if an export already exists, skip its generation and use the installation commands.

Recover the safety axe and build the separate cutting body:

```sh
/tmp/pe-asset-tools/bin/python rebuild/tools/export_reference_axe.py
python3 rebuild/tools/analyze_reference_bake.py \
  "$HOME/.local/share/planet-explorers-bake-reference/pose-captures/female-clips-08.jsonl" \
  --rig rebuild/build/reference-player-starter/player-female-starter-rig.json \
  --report rebuild/build/animator-bake/female-clips-08-report.json \
  --export-dir rebuild/build/animator-bake/player-female-cut
python3 rebuild/tools/export_baked_axe.py \
  "$HOME/.local/share/planet-explorers-bake-reference/pose-captures/female-clips-02.jsonl" \
  --rig rebuild/build/reference-player-starter/player-female-starter-rig.json \
  --visual-assets rebuild/build/reference-assets/safety-axe \
  --body-scene rebuild/build/animator-bake/player-female/all/recording.peas \
  --output rebuild/build/animator-bake/player-female/axe-all
python3 rebuild/tools/export_baked_axe.py \
  "$HOME/.local/share/planet-explorers-bake-reference/pose-captures/female-clips-08.jsonl" \
  --rig rebuild/build/reference-player-starter/player-female-starter-rig.json \
  --visual-assets rebuild/build/reference-assets/safety-axe \
  --body-scene rebuild/build/animator-bake/player-female-cut/all/recording.peas \
  --output rebuild/build/animator-bake/player-female/axe-cut-all
mkdir -p rebuild/build/reference-assets/player-axe rebuild/build/reference-assets/player-cut rebuild/build/reference-assets/player-axe-cut
cp -a rebuild/build/animator-bake/player-female/axe-all/. rebuild/build/reference-assets/player-axe/
cp -a rebuild/build/animator-bake/player-female-cut/all/. rebuild/build/reference-assets/player-cut/
cp -a rebuild/build/animator-bake/player-female/axe-cut-all/. rebuild/build/reference-assets/player-axe-cut/
```

The cutting body/axe pair is required together and requires the ten-clip axe scene. Its three named clips are Felling1–3. The tutorial host retains its independent entry/hit/exit timers; a recovered animation event does not replace scripted tree HP accounting. See [tree asset provenance](../docs/audit/TRAINING_TREE_ASSETS.md).

Recover the wooden shovel and separate digging body:

```sh
/tmp/pe-asset-tools/bin/python rebuild/tools/export_reference_shovel.py
python3 rebuild/tools/analyze_reference_bake.py \
  "$HOME/.local/share/planet-explorers-bake-reference/pose-captures/female-clips-09.jsonl" \
  --rig rebuild/build/reference-player-starter/player-female-starter-rig.json \
  --report rebuild/build/animator-bake/female-clips-09-report.json \
  --export-dir rebuild/build/animator-bake/player-female-dig
python3 rebuild/tools/export_baked_shovel.py \
  "$HOME/.local/share/planet-explorers-bake-reference/pose-captures/female-clips-02.jsonl" \
  --rig rebuild/build/reference-player-starter/player-female-starter-rig.json \
  --visual-assets rebuild/build/reference-assets/wood-shovel \
  --body-scene rebuild/build/animator-bake/player-female/all/recording.peas \
  --output rebuild/build/animator-bake/player-female/shovel-all
python3 rebuild/tools/export_baked_shovel.py \
  "$HOME/.local/share/planet-explorers-bake-reference/pose-captures/female-clips-02.jsonl" \
  --rig rebuild/build/reference-player-starter/player-female-starter-rig.json \
  --visual-assets rebuild/build/reference-assets/wood-shovel \
  --body-scene rebuild/build/animator-bake/player-female/all/recording.peas \
  --mode stowed --output rebuild/build/animator-bake/player-female/shovel-stowed-all
python3 rebuild/tools/export_baked_shovel.py \
  "$HOME/.local/share/planet-explorers-bake-reference/pose-captures/female-clips-09.jsonl" \
  --rig rebuild/build/reference-player-starter/player-female-starter-rig.json \
  --visual-assets rebuild/build/reference-assets/wood-shovel \
  --body-scene rebuild/build/animator-bake/player-female-dig/all/recording.peas \
  --output rebuild/build/animator-bake/player-female/shovel-dig-all
mkdir -p rebuild/build/reference-assets/player-shovel rebuild/build/reference-assets/player-shovel-stowed rebuild/build/reference-assets/player-dig rebuild/build/reference-assets/player-shovel-dig
cp -a rebuild/build/animator-bake/player-female/shovel-all/. rebuild/build/reference-assets/player-shovel/
cp -a rebuild/build/animator-bake/player-female/shovel-stowed-all/. rebuild/build/reference-assets/player-shovel-stowed/
cp -a rebuild/build/animator-bake/player-female-dig/all/. rebuild/build/reference-assets/player-dig/
cp -a rebuild/build/animator-bake/player-female/shovel-dig-all/. rebuild/build/reference-assets/player-shovel-dig/
```

Training validates the complete four-directory shovel set together. The first two scenes share the original ten body clocks; the digging body/held shovel share exactly Spade1–3. Held/stowed attachments use source mountMain/mountBack. The separately exported stowed digging scene is an offline reference and is not required by the frontend. Static geometry and all digging frames have passed headless native sanitizer checks. Those asset checks are separate from the passing 12000-frame C graphical mining run below; an original-client dig outcome remains unobserved. Source state speed, controller transitions, root motion and MinePutOn/MinePutOff behavior remain separate from sampled authored poses; [shovel asset provenance](../docs/audit/SHOVEL_ASSETS.md) and [mining action rules](../docs/audit/TRAINING_MINING.md) describe the boundaries.


## Sandbox tree and wood-axe assets

The sandbox reuses the installed cutting/shovel body and tool groups above. Its tool transactions use the general source events, without the tutorial's seven-hit tree, mission grants or fixed mound. The recovered female body is required; supplied tool groups must be complete and match their body clocks. Axes currently have a fixed held attachment only, so stowed axes are invisible. [Tool presentation and exact source identity](../docs/audit/SANDBOX_TOOL_PRESENTATION.md) describe these limits.

The sandbox also requires `sandbox-world/tree.obj`, its material/texture and `tree-collider.pesc`. First recover `gathering` with the herb/tree exporter described above, then run:

```sh
/tmp/pe-asset-tools/bin/python rebuild/tools/export_sandbox_world_assets.py
/tmp/pe-asset-tools/bin/python rebuild/tools/export_sandbox_world_assets.py --check
```

This creates a fresh `rebuild/build/reference-assets/sandbox-world` directory, rebasing the recovered tree out of its tutorial placement into source-local meters. It verifies the matching source capsule and preserves provenance hashes. `--check` validates an existing export without rewriting it. All provisional sandbox trees share this model, while saved positions and remaining resource state determine placement and visibility. The capsule supports axe interaction rays and Adventure movement collision; depleted trees are excluded, and camera obstruction queries retain the original tree exclusion. The headless collision constants are checked into `generated/pe_tree_collider.h` and use the same source decoder as the visual PESC. They do not depend on recovered graphics or Unity at runtime. Regenerate or verify them with the optional UnityPy environment:

```sh
/tmp/pe-asset-tools/bin/python rebuild/tools/export_tree_collision.py
/tmp/pe-asset-tools/bin/python rebuild/tools/export_tree_collision.py --check
```

The check verifies the generated header and, when present, byte-identical `sandbox-world/tree-collider.pesc` source values. It requires the repository source prefab but no installed game. Normal native compilation does not need UnityPy.

Wood Axe26 uses its own original mesh. After the body and cutting scenes above are installed, generate these fresh outputs; do not relabel the Safety Axe scene:

```sh
/tmp/pe-asset-tools/bin/python rebuild/tools/export_reference_axe.py --item-id 26
python3 rebuild/tools/export_baked_axe.py \
  "$HOME/.local/share/planet-explorers-bake-reference/pose-captures/female-clips-02.jsonl" \
  --rig rebuild/build/reference-player-starter/player-female-starter-rig.json \
  --visual-assets rebuild/build/reference-assets/wood-axe \
  --body-scene rebuild/build/reference-assets/player/recording.peas \
  --output rebuild/build/reference-assets/player-wood-axe
python3 rebuild/tools/export_baked_axe.py \
  "$HOME/.local/share/planet-explorers-bake-reference/pose-captures/female-clips-08.jsonl" \
  --rig rebuild/build/reference-player-starter/player-female-starter-rig.json \
  --visual-assets rebuild/build/reference-assets/wood-axe \
  --body-scene rebuild/build/reference-assets/player-cut/recording.peas \
  --output rebuild/build/reference-assets/player-wood-axe-cut
```

Each wood-axe scene contains the actual 275-vertex model bound to the same sampled `mountMain` used by its companion body. Existing destinations are preserved; use new output directories for regeneration, validate them, then deliberately install the matching set. Original game assets and sampled captures remain local prerequisites, not redistributed binaries.

## Training mining controls and verification

After crafting, use **I → Equipment** and right-click either Wood Shovel. Each keeps its own durability. Close the inventory and acknowledge Andy, then left-click to draw it. In third person, aim at the mound within three meters of the player and hold left mouse to dig. The highlighted sample is the actual source-style snapped target. Release ends the digging cycle; a short right click stows the shovel. First-person digging is currently unsupported.

```sh
make -C rebuild training-mining-smoke
```

The 12,000-frame graphical test passed through mission752: five shovel events damaged and removed three real samples, yielding six dirt, then the next lesson granted9,000. Final dirt9,006, wood 11, two shovel instances, nine juices and500/500HP were checked. The equipped shovel ended at3528/3553 durability; the second remained untouched at3553/3553. Terrain render/collision snapshots share the same marching-cubes mesh and update after edits. The source mound disappears on completion. [Digging capture](../docs/reference/rebuilt-training-mining.png). The new core, exporter,18CLI and sanitizer tests pass; source regeneration checks match.

The source animation poses are recovered, while state transitions are discrete and draw/stow is immediate. Original live digging outcomes, controller blending/IK and original terrain shading remain unverified. Training building controls and the real receiver evaluator now pass the 15,000-frame graphical sequence; sandbox construction remains separate. See [mining behavior](../docs/audit/TRAINING_MINING.md) and [shovel assets](../docs/audit/SHOVEL_ASSETS.md).


## Training construction and receiver puzzle

At 752, acknowledge the building introduction and press **B**. A successful build-mode opening queues the original completion dialogue, then accepts 753. After the receiver introduction, aim at the source terrain anchors or existing blocks:

| Control | Training construction action |
| --- | --- |
| B | Enter or leave Build Mode |
| Left mouse | Place one half-meter dirt cube |
| Alt + left mouse | Remove the targeted placed cube |
| T | Cycle cube rotation |

The training frontend currently exposes cubes only; the standalone Block45 module and sandbox also support ramps. Each one-cell placement costs one dirt; removing one cell refunds zero, following the source grouped ceiling/floor rules. Clicks are consumed once, including across fixed-step catch-up, and dialogue acknowledgements do not also place blocks. Actual cube geometry and receiver slabs participate in player collision.

The puzzle evaluates50 source rays against the nearest layer12 receiver, terrain or building geometry. A receiver hit is−100; any other hit or miss is+1, so all 50 must avoid their receivers. There is no placed-block-count shortcut. Success clears the authored anchors and lesson-written blocks without a cleanup refund, exits build mode and reaches **756**, whose creation controls remain unsupported. This source mission chain skips the older754/755 save/export lessons. [Source rules and limits](../docs/audit/TRAINING_BUILDING.md).

Optional original receiver meshes and indicator textures can be recovered locally:

```sh
/tmp/pe-asset-tools/bin/python rebuild/tools/export_training_build_assets.py
make -C rebuild training-building-smoke
```

The 15,000-frame fixture **passed with exit 0 at mission 756**. It used the complete prior lesson sequence, 19 actual cube placements and one removal, reached receiver score 50 and cleared all lesson cells and terrain triangles. Dirt ended at 8,987:9,006 minus 19 single-cell placement costs, with no refund for the one-cell removal or lesson cleanup. Wood 11, two shovels (equipped durability 3528/3553; second 3553/3553), nine juices and HP 500/500 were preserved. Combat again recorded eight attack starts and 24 real contacts, ending hare HP 0/212. [Construction in progress](../docs/reference/rebuilt-training-building.png) and [completed construction lesson](../docs/reference/rebuilt-training-building-complete.png). This validates the C sequence, not an original-client construction outcome or complete game parity. The core receiver/mission/collision regressions and strict sanitizer checks pass. Missing optional receiver art falls back to matching plain boxes; the original hologram shader and particle presentation are not reproduced.


### Tutorial NPC validation

All nine NPC models and per-Avatar idle clips are installed in the current local asset set. Run `make -C rebuild training-npcs-smoke` for the 1750-frame storage/Andy sequence and camera survey of all nine characters. The check requires the complete NPC group, validates progression through mission 746, and saves individual screenshots under `rebuild/build/smoke-training-npcs.png-npc-ID.png`. It passed with9NPCs, 9 juices and the owned/equipped knife. Source recovery, regeneration commands and remaining behavioral limitations are recorded in the [NPC audit](../docs/audit/TRAINING_NPCS.md).

## Sandbox pickaxe assets

Wood Pickaxe19 requires the complete `player-pickaxe`, `player-pickaxe-stowed`, `player-mine` and `player-pickaxe-mine` groups beneath the reference-assets root. They are installed locally from the original mesh and the closed91-pose Mine capture. Held/stowed locomotion uses the existing ten-clip body capture; action scenes use only Mine1–3 with matching body/tool clocks. Missing whole groups disable that tool action; partial or mismatched groups fail validation before upload. The [pickaxe recovery audit](../docs/audit/SANDBOX_PICKAXE.md) records exact provenance, fresh-directory export commands and remaining source-transition limitations. `rebuild/build/sandbox_action_fixture --mine OUTPUT.pesave` creates an isolated item19 physical-test fixture through the same gameplay APIs as the other tools.

### Supplier interaction check

With the recovered supplier scene installed and the desktop unlocked:

```sh
make -C rebuild all build/adventure_supplier_fixture
python3 rebuild/tools/playtest_adventure_supplier.py
```

This generates an isolated fresh Adventure fixture and tests proximity, cancellation, source dialogue, one-time supplies and save/reload through physical input. It does not open Steam or normal saves. See the [recorded result](../docs/reference/sandbox/rebuild-supplier-result.json) and [playtest](../docs/PLAYTEST.md#october-5-adventure-supplier-interaction).
