# Installed visual asset recovery

Audited 2026-10-03 against the installed Linux original at `~/.local/share/Steam/steamapps/common/Planet Explorers`. Small bundles can already be read and converted without Unity, a graphics context, or modifying the installation. The initial practical target was the wood axe for source item **26**. The optional exporter now supplies the training room, tools, emergency kit and knife; a vegetation mesh is also recoverable. The separate visual preview does not run gameplay. The playable training path now uses all three room colliders and the bounded lesson sequence through knife equipment, stopping at mission 746. Animated hare and player research exports are documented below.

## Reproducible tooling

[UnityPy](https://github.com/K0lb3/UnityPy) exposes Unity serialized objects, OBJ mesh export, and decoded texture images. The tested version is **1.25.4**, installed in an isolated Python virtual environment. No system Python packages or game files were changed. The new [inventory utility](../../rebuild/tools/inventory_reference_asset.py) reads one bundle, reports SHA256, Unity version, object counts, mesh bounds/submeshes, scene transforms, renderer/material/texture references and optional in-memory OBJ conversion checks. It writes metadata only. The default 64 MiB input guard discourages accidentally loading the full installation; compressed decompression size can exceed the input size.

```sh
python3 -m venv /tmp/pe-asset-tools
/tmp/pe-asset-tools/bin/python -m pip install UnityPy==1.25.4
/tmp/pe-asset-tools/bin/python rebuild/tools/inventory_reference_asset.py \
  "$HOME/.local/share/Steam/steamapps/common/Planet Explorers/AssetBundles/Item/wood_ax.unity3d" \
  --probe-meshes --output /tmp/pe-wood-asset.json
```

The same command works for `scene_TrainingRoom.unity3d` and `alpine_tree05a.unity3d`. Names are case-sensitive on Linux. The inventory checks that output is not the source file. Repeating the wood-axe inventory produced byte-identical JSON. OBJ conversion succeeded for all five meshes in these three bundles; axe texture decoding to PNG also succeeded. Initial exported proof files are temporary under `/tmp/pe-recovered-proof`. The practical exporter now writes ignored generated assets to `rebuild/build/reference-assets`. No asset bundle was copied or rewritten; converted binaries are not committed.

AssetRipper is an alternative for recovering a larger Unity project and references, but was not installed or exercised during this audit. Its documented engine support includes Unity 5.x; successful conversion of this game through that tool remains unverified. [AssetRipper project](https://github.com/AssetRipper/AssetRipper)

## Concrete bundle inventory and provenance

Paths below are relative to the installed game root, all under `AssetBundles/Item/`.

| Bundle | Input bytes | Recovered visual geometry | Other contents |
|---|---:|---|---|
| `wood_ax.unity3d` | 34,474 | `wood_ax`, 275 vertices, 228 triangles, 1 submesh | 1 material, 1 texture, 2 GameObjects referencing the same mesh |
| `alpine_tree05a.unity3d` | 2,617,659 | `Alpine_tree05A`, 613 vertices, 597 triangles, 1 submesh | 1 material, 2 textures, 2 shaders |
| `scene_TrainingRoom.unity3d` | 12,371,360 | `TrainingRoom_MeshPart0`: 65,532 vertices / 53,359 triangles / 21 submeshes; `TrainingRoom_MeshPart1`: 2,907 / 2,259 / 15 | 22 materials, 35 textures; separate collision mesh; 5 GameObjects |

SHA256 identities:

```text
wood_ax.unity3d
8b3a9951e71053676e2232d1f877943c79013a1121476da2e1ad60c5778d167b
alpine_tree05a.unity3d
a0dd46d193af8175cf13599501c58facc4384f13c6af7badff35f9dcb364f145
scene_TrainingRoom.unity3d
e25ba3ca905f4ba750a576d8cc3019e3e4e57fcbb0d327f4f5e12a21e20aaf28
```

These identities belong to this installed reference, not to an assumed source-control revision. The source tree and installed build can differ. The wood-axe bundle itself reports Unity **4.3.0f4**, despite being shipped with the later player. The compiled bundle's legacy MonoBehaviour fields should not replace audited source gameplay data merely because its visual asset name matches.

## First small asset: item 26 wood axe

`Assets/Resources/DataBase/localData.bytes`, table `PrototypeItem`, row `id=26`, has `_engName=wood axe` and `_modelPath=Prefab/Item/Equip/Weapon/Ax/wood_ax`. The selected C content already includes item 26 and its recipe. The installed matching bundle exposes containers `wood_ax` and `meleeweapon`.

Within this bundle:

- Mesh path ID **4**, name `wood_ax`, exports to a 37,900-byte OBJ. Its exported SHA256 is `0da4cdbdf37c50b429c6e07c15a8c7d893916bffa5a37ded50282ad4ca9d8edd` using UnityPy 1.25.4.
- Material ID **2** binds `_MainTex` to Texture2D ID **3**. Texture is 256×256, Unity format 10, decoded PNG 62,052 bytes. `_Color` RGB is approximately 0.902985, alpha 1.
- GameObject ID **6** uses identity transform ID **8**. GameObject ID **7** uses transform ID **9**, approximately −90° about X. Both refer to mesh 4/material 2. Select the desired container, rather than drawing both copies.

A local extraction prototype needs only `UnityPy.load(bundle)`, the chosen `Mesh.read().export()` and `Texture2D.read().image.save(...)`. Write a matching MTL or assign the recovered texture to the single raylib material explicitly. Preserve the source hash, object IDs, conversion version and transform in a small manifest beside the generated files. Raylib's model implementation contains OBJ and glTF loaders; the existing C frontend can consume the converted asset without adding Unity runtime dependencies. [raylib model loader source](https://github.com/raysan5/raylib/blob/master/src/rmodels.c)

The implemented exporter selects the gameplay root (GameObject6) for the axe and excludes its duplicate import root. The preview module accepts its generated OBJ. A held-tool attachment, animation and visual comparison against the original remain separate frontend work.

## Tutorial room reconstruction

[TrainingRoomLoader.cs](../../Assets/Scripts/Jaluca/TrainingField/TrainingRoomLoader.cs) instantiates resource `scene_TrainingRoom`, applies rotation `(0, -90, 0)` and position `(12, 1.5, 12)`, and separately creates `TrainingRoomLight`, `TrainingRoomExitSign`, `TrainingRoomLifts`, `EpsilonIndi`, `TrainingManager_New`, tutorial music, mission manager and pathfinder. The room alone is therefore not the complete playable tutorial.

The source resource prefab [scene_TrainingRoom.prefab](../../Assets/Scripts/Jaluca/TrainingField/Resources/scene_TrainingRoom.prefab) is readable by UnityPy: 6 GameObjects, 6 transforms, 3 MeshRenderers, 4 MeshFilters, 3 MeshColliders, 1 BoxCollider, 2 MonoBehaviours and a PrefabInstance. Its external references retain GUIDs; mesh/model source files were missing from the initial source audit. The installed bundle contains a related five-object room prefab with matching named mesh parts. Its manifest lists `Assets/Prefabs/ScenePrefab/scene_TrainingRoom.prefab`, with no bundle dependencies. Exact equivalence to the compiled `Resources` copy is not yet established; the source wrapper has an additional Cube and collider.

Useful bundle object identities:

| Purpose | Mesh path ID | Vertices | Triangles |
|---|---:|---:|---:|
| Main visual part | −994153193595870446 | 65,532 | 53,359 |
| Second visual part | −4312639860208082577 | 2,907 | 2,259 |
| `TrainingRoom_Col` collision | 3516175124011679944 | 1,179 | 828 |

The collision object's MeshRenderer is disabled. Do not display it as a third visible room part. All **three** mesh objects have enabled MeshColliders: both visible mesh parts also collide. Preserve all three components, totaling **56,446** collision triangles, including the separate 828-triangle mesh. Recover MeshCollider associations independently of renderer visibility. Child mesh transforms are identity beneath the room hierarchy; preserve the root and source loader transform. Renderer material-array order maps each visual submesh to one of the 22 materials; mesh-only OBJ export does not automatically recover these assignments or write MTL. The metadata utility preserves the references needed to do so.

The exporter now produces the two textured visual meshes and all three collider instances with source placement. The training frontend now uses those three mesh collider instances through a native BVH/capsule controller. Full material/shader conversion, separate lifts and signs, original lighting, and later mission triggers remain incomplete. A glTF scene could retain hierarchy, multiple materials and transforms, but a glTF writer was not exercised; OBJ plus explicit material/transform metadata is the implemented path here.

## Vegetation and tutorial effects

The recovered `Alpine_tree05A` sample has mesh path ID **−3234636106914528023** and an identity object transform. Its `Alpine_tree_A_special` material references diffuse `Alpine_tree_A` (2048×2048) and normal `Alpine_tree_A_N` (1024×1024). `_Cutoff` is approximately **0.55**. A solid opaque fallback would fill the leaf cards and visibly misrepresent the tree; preserve texture alpha and implement alpha discard. Specialized tree shader lighting, wind, ambient occlusion and normal handling need separate porting. This sample is a proven recoverable vegetation asset, not a verified identification of the tutorial's specific holographic tree.

The tutorial's tree appearance is more than a static model. [HoloCameraControl.cs](../../Assets/Scripts/Jaluca/TrainingField/HoloCameraControl.cs) renders selected objects through a camera following the main camera into a screen-sized RenderTexture, temporarily toggling source renderers. [HolotreeAppearance.cs](../../Assets/Scripts/Jaluca/TrainingField/HolotreeAppearance.cs) makes its display face the camera, changes the source tree scale during appearance, and updates `_Scale`. Recover the serialized `TrainingManager_New` references to identify its exact tree/herb sources before substituting the available alpine sample. A screenshot or recovered mesh alone does not replicate that effect.

## Coordinate and material conversion checks

UnityPy 1.25.4's `export/MeshExporter.py` negates mesh X and normal X and reverses triangle order during OBJ export. It leaves UV coordinates unchanged. This is already a handedness conversion: applying another blind mirror in C would reverse it. For hierarchy conversion, use a consistent reflection matrix `S=diag(-1,1,1)` and transform `M'=S*M*S`, including translations, rotations, collision vertices and the source room placement. Alternatively, decode mesh arrays directly and apply one deliberate game-wide coordinate convention. Compare asymmetric geometry before accepting the orientation.

Decoded Unity normal maps may use different channel conventions from the new shader. Preserve submesh/material correspondence, diffuse alpha, texture tiling/offset, material tint and render queue before claiming visual fidelity. Native Unity shader objects are not portable GLSL. Skinned characters and animations need an additional skeleton/bind-pose/clip conversion audit; none were exported here.

The full `PE_Client_Data/resources.assets` is 1,965,305,184 bytes, with a further 334,706,336-byte `resources.resource`. Loading the entire directory while the original game runs is unnecessary for these first assets and can exhaust available RAM. Resolve individual small bundles first; later resource-only extraction should use controlled file-backed reading and explicit dependency resolution. Original files stay in the installation; only reproducible tooling and metadata belong in the replication repository.

## Implemented optional import and C preview

[export_reference_assets.py](../../rebuild/tools/export_reference_assets.py) converts the original installation locally, using UnityPy1.25.4 only as an offline Python dependency. No Python or Unity library is linked into the C application.

```sh
/tmp/pe-asset-tools/bin/python rebuild/tools/export_reference_assets.py
# Optional smaller/focused selection:
/tmp/pe-asset-tools/bin/python rebuild/tools/export_reference_assets.py \
  --asset wood-pickaxe --asset stone-pickaxe --asset wood-axe
# Metadata and diffuse colors without PNG decoding:
/tmp/pe-asset-tools/bin/python rebuild/tools/export_reference_assets.py --no-textures
```

Default selection is room plus wood axe; `--asset` is repeatable and also supports wood/stone shovel and stone axe. `--game-dir` overrides the installation root, `--output` overrides the ignored default `rebuild/build/reference-assets`. Output inside the original installation is rejected. Conversion stages all requested outputs in a temporary sibling directory, then replaces individual generated files and installs the manifest last. Files from earlier unselected assets remain in the output cache; the current manifest describes the current selection. Original bundles are read only.

Generated outputs include `room.obj`, `room.mtl`, diffuse PNGs, `room-collision.obj`, equivalent selected tool files, and `manifest.json`. The manifest records source hashes/Unity versions, generated file hashes/sizes, selected root, original transform hierarchy, baked matrices, renderer material slot arrays, full original material property metadata, collider trigger/convex flags, and visual bounds. Collider data is preserved for future simulation work, not loaded into current game collision.

The practical exporter **bypasses UnityPy's OBJ exporter** and reads MeshHandler arrays directly. It keeps original Unity XYZ in meters and original triangle order, bakes parent-times-child TRS, applies the tutorial loader's `(12,1.5,12)` and −90° Y rotation, transforms normals by the inverse transpose and normalizes them. Negative hierarchy scale reverses face order to preserve orientation. No permanent X reflection is introduced into the terrain/simulation coordinate system. All 228 original axe triangle cross products agree with their stored normals; coordinate tests verify the room origin and transformed unit axes.

Only the main diffuse texture and tint are mapped into MTL. Other shader properties and texture scale/offset remain in manifest metadata; normal/specular maps, lighting, emission, effects and the original Unity shaders are not yet ported. The audited room/axe main textures all have identity UV scale and zero offset; arbitrary additional assets still need this check. Tree alpha-cutoff needs its own shader and the current exporter deliberately limits selections to room/tools.

The frontend module [pe_reference.h](../../rebuild/src/pe_reference.h) / [pe_reference.c](../../rebuild/src/pe_reference.c) exposes:

```c
PeReferenceModel reference = {0};
// After InitWindow; obj_path points to generated room.obj.
pe_reference_load(&reference, obj_path, error, sizeof(error));
Camera3D camera = pe_reference_camera(&reference); // whole-model bounds view
// Between BeginMode3D(camera) and EndMode3D():
pe_reference_draw(&reference);
// Before CloseWindow:
pe_reference_unload(&reference);
```

The model draws at identity because source transforms are baked. `reference.bounds` and `reference.triangles` support inspection and UI. It validates readable OBJ geometry and checks loaded triangle counts to detect raylib fallback geometry; loading failure preserves the previous model. Unloading releases its diffuse textures once, skips raylib's shared default texture, then releases model allocations. The module is frontend-only and needs `src/pe_reference.c` added alongside `main.c`, not to the headless simulation library. It does not mutate game state, player position, terrain or saves.

Verification: strict C11 `-Wall -Wextra -Wpedantic -Werror` compilation passed for the module. `python3 rebuild/tests/test_reference_assets.py` runs four dependency-free coordinate/material-hash/error tests and skips the optional installed-content test. Running it with the UnityPy virtual environment also passes the installed-content test: two complete exports produce identical files, OBJ and manifest triangle counts agree, all three room colliders and material slots survive, source placement remains positive `(12,1.5,12)`, and only the intended axe root is exported. This sub-agent performed no graphics launch or desktop input; interactive frontend integration and screenshot validation are coordinated by the root agent.

### Raylib 6 OBJ parser compatibility fix

The first frontend preview hung while importing materials. A headless reproduction using raylib 6's bundled `tinyobj_loader_c.h` identified an infinite retry during the seventeenth material insertion: its quadratic probe could not reach remaining free buckets, and growth checked only total occupancy. The original long material names derived from source path IDs triggered this at capacity 20. This was not geometry size or OBJ preflight cost.

The exporter now emits deterministic short material symbols whose DJB2 buckets are unique at each parser table size (10,20,40 for the room). The manifest's `obj_symbol` preserves their association with original material path IDs. Both 32-bit and 64-bit DJB2 calculations agree for these short names. A headless run of the exact upstream hash implementation completes all 22 material insertions with these symbols; coordinate tests exercise the growth sequence.

Before calling raylib, `pe_reference_validate` now checks the exporter-specific OBJ/MTL headers and accepted syntax, finite bounded-exponent numeric fields, positive in-range vertex/UV/normal indices, safe local filenames, referenced material coverage and collision-free material insertion at each growth size. Unsupported arbitrary OBJ files and stale unsafe material names are rejected before reaching tinyobj. This is a deliberate raylib 6 compatibility workaround for this generated format, not a general OBJ importer. Re-export assets after updating the tool.

The validator is usable without a graphics context. The 55,618-triangle room validates in approximately 0.11 seconds on this machine. [test_reference_validation.c](../../rebuild/tests/test_reference_validation.c) verifies the generated subset and rejects out-of-range faces, huge exponents, parent-directory material paths and colliding material names. It can also validate the installed room output:

```sh
cc -O2 -std=c11 -Wall -Wextra -Wpedantic -Werror -Irebuild/src \
  rebuild/tests/test_reference_validation.c rebuild/src/pe_reference.c \
  -o /tmp/pe-reference-validation-test -lraylib -lm
/tmp/pe-reference-validation-test rebuild/build/reference-assets/room.obj
```

These tests open no window. The parser issue and workaround are specific to the tested [raylib 6 tinyobj implementation](https://github.com/raysan5/raylib/blob/6.0/src/external/tinyobj_loader_c.h); upstream fixes may eventually allow simplifying this compatibility path.

## Training view integration

`src/pe_training_view.h/c` now supplies `pe_training_view(asset_dir, smoke_frames, screenshot)` as a standalone frontend entry point. It uses the recovered `room.obj` visuals and all three components in `room-collision.obj`, delegating collision to `PeSceneCollision` and movement/progression to `PeTraining`. F5 cycles third person, control mode, then first person. Third person uses right-button look; control/first-person modes support Tab to release the cursor. Esc pauses and releases the cursor; Q leaves while paused. Camera FOV is 60 degrees, with obstruction raycasting for the provisional third-person orbit camera. Simulation positions retain source Unity XYZ, including the controller's source spawn `(27,2,11)` projected onto the recovered room floor.

The UI connects the recovered room to source camera/movement, dialogue, storage, return, inventory, quickbar, consumption and knife-equipment lessons, ending at mission 746 before live combat. Source English dialogue is preserved; the inventory subset contains Herbal Juice, one owned knife and one quickslot. HP starts at 500/500 and source juice healing/cooldown rules are implemented. Colored capsules stand in for the player and Andy interaction marker. The optional recovered hare uses skeletal idle/walk animation and a bounded leisure patrol; source-informed headless attack/contact modules are not yet connected to training combat. Original player/NPC animation and exact camera-controller behavior remain unported. The room uses diffuse-only conversion. See the [playtest record](../PLAYTEST.md) for the complete keyboard/mouse verification and current fidelity boundaries.

The 90-frame smoke uses the same controller APIs for camera changes, ordinary movement, sprint, jump and autorun and expects mission 718. The 1500-frame route continues through storage, Andy, inventory, juice use and knife equipment and expects 746 with nine juices and the knife equipped. Both graphics checks passed; the newest long run also exercised patrol (mode 1, position `(3.293,1.632,17.898)`, one decision). A separate 38,308-frame keyboard/mouse run reached 746 and verified equip/unequip/re-equip conservation. Screenshot failures return a nonzero status. The view does not fake tutorial-completion bits, load synthetic room collision, save game state, or manipulate desktop input. Core/sanitizer checks, fifteen CLI tests and the new headless modules pass.

### Camera and walking audit follow-up

The training frontend now exposes Z to toggle walking/running and forwards `PeTrainingInput.walk` to the controller (source speeds 2 and 5 m/s). Sprint remains a separate higher-priority input. Its HUD lists Z and displays Walking/Running when autorun is inactive.

The binary CameraForge data in `Assets/PeCamera/Resources/Controllers/PECamera_Start.asset` and `PECamera_Update.asset` was decoded read-only through UnityPy, then according to `ControllerAsset.Read`, `PoseNode.Read`, `ModifierAsset.Read`, `Node.Read` and `Var.Read`. The active Start graph's embedded `Init` modifier stores yaw 30°, pitch −20°, roll 0°, distance 6m, copying those values into wanted values and `DistLevel`. The Update graph uses `ThirdPersonFollow` with minimum distance 1.8m, maximum 10m normally / 20m in build mode, pitch −70°..55°, damping 0.4 and follow-speed default 7.5. Actual distance also depends on activity-space and geometry clamping. These are active graph values; the standalone old `Modifiers/3rd F1.asset` is not the appropriate default reference.

Its first-person branch is `ThirdPersonShoot` named `1st Person Follow`, with pitch −75°..75° and damping 0.4. [ThirdPersonShoot.cs](../../Assets/PeCamera/Scripts/ScriptModifier/ThirdPersonShoot.cs) derives yaw changes from mouseX and the controller right stick, not A/D keys. [MainPlayerCmpt.cs](../../Assets/Scripts/PeEntity/Cmpt/MainPlayerCmpt.cs) computes camera-relative movement from horizontal-right plus vertical-forward, and `UpdateCamDirection` turns the actor to the camera in first person. [PeInputExt.cs](../../Assets/Scripts/Input/PeInputExt.cs) sets horizontal input −1 for the configured left key. Therefore A should strafe relative to camera, rather than deliberately rotate the first-person camera. An observed runtime turn on A needs separate input/runtime investigation; these source paths do not establish its cause.

The new view now starts at source distance 6m, yaw 30° and pitch −20°, converted into its camera formula as yaw150° and pitch−20°. `ThirdPersonFollow` builds Euler(−pitch,yaw,0), so its Unity+Z forward is `(sin(yaw)*cos(pitch), sin(pitch), cos(yaw)*cos(pitch))`; the frontend uses a negative-cosine Z and therefore maps yaw to180°−sourceYaw. It also uses source pitch bounds (−70°..55° third/control, −75°..75° first person). Mouse sensitivity, immediate rotation, fixed target height, simple obstruction ray and the missing source damping/activity-space adjustment remain provisional. This changes verified initial values without claiming a full camera-graph port.

## Emergency-kit visual

`--asset training-kit` exports the exact mesh/material shared by source emergencyKit2 and emergencyKit3, recovered from installed `emergencykit3.unity3d`. The source kit2 placement `(7,1.5,12)`, yaw +60°, scale one is baked into the mesh. It contains 1,436 vertices, 1,572 triangles and one material; the output is `training-kit.obj` plus its diffuse texture and independent `training-kit-manifest.json`. The kit-only export leaves the room model and original manifest untouched. Source prefab hashes, shared GUID/path IDs and BoxCollider dimensions remain in the manifest. The visual loads in training when available; its separate box collider, specular/gloss/normal-map rendering remain unimplemented. Installed-asset repeatability and guarded C validation passed.

## Skeletal assets and current validation boundary

[Actor recovery](ACTOR_RECOVERY.md) documents the original hare mesh and 48-bone sampled palettes. Its eight clips are idle, run, five authored melee animations and walk. The 90-frame idle/run preview passed, followed by a 630-frame graphics check cycling all eight clips before hitbox-overlay integration. [Hare movement](HARE_MOVEMENT.md) distinguishes source patrol values from the native scheduler/controller approximations; this is not hostile AI or completed combat. [Hitbox recovery](HITBOX_RECOVERY.md) records the original defense/attack capsules and skeletal mapping. Its headless tests pass, while overlay graphics remain pending.

[Player asset recovery](PLAYER_ASSET_RECOVERY.md) provides a separate exporter for the original default male/female modular body meshes, diffuse textures, skin weights, inverse binds and Avatar/default-pose data. Repeat exports and decoded Attack6 scalar coverage pass headless tests. These static research OBJs do not replace the training capsule or solve humanoid animation. Player OBJ graphics and matching-runtime bone-pose capture remain pending. [Player animation](PLAYER_ANIMATION.md) records the controller/event evidence needed for that work.

Original-client observations confirm knife grant, equipment and drawing, with a hare encounter visible. No successful hit or kill has been confirmed. Native training stops at 746 with no live attacks, combat damage, actor death or post-kill mission transition. See [PLAYTEST.md](../PLAYTEST.md) for retained evidence, including earlier checks at their then-supported boundaries.
