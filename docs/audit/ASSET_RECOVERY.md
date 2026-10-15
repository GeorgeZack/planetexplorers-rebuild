# Installed visual asset recovery

Offline conversion from the user's installed native game. No recovered binaries enter Git. Current runtime scope is in [contracts](../FEATURE_CONTRACTS.md); [PLAYTEST](../PLAYTEST.md) owns observations. Initial capsule-only/Tutorial746 status is obsolete.

## Reproducible tooling

Use `make -C rebuild unity-tools` and its pinned Python environment; `export_reference_assets.py --help` lists selections. `inventory_reference_asset.py` writes bundle metadata, optionally probing meshes, with a64MiB input guard (decompressed size can be larger). It rejects source-as-output. Default generated output: ignored `rebuild/build/reference-assets`.

UnityPy1.25.4 was qualified on these bundles. Original files stay read-only. Resolve small bundles first: resources.assets is1,965,305,184 bytes plus334,706,336-byte resource stream; loading the whole install while playing risks memory pressure. AssetRipper was considered but not exercised.

## Concrete bundle inventory and provenance

Relative paths under installed `AssetBundles/Item/`:

| Bundle | Bytes | Geometry | SHA256 |
|---|---:|---|---|
|wood_ax.unity3d|34474|275 vertices/228 triangles/one submesh|`8b3a9951e71053676e2232d1f877943c79013a1121476da2e1ad60c5778d167b`|
|alpine_tree05a.unity3d|2617659|613/597/one submesh|`a0dd46d193af8175cf13599501c58facc4384f13c6af7badff35f9dcb364f145`|
|scene_TrainingRoom.unity3d|12371360|two visual parts, three collider instances|`e25ba3ca905f4ba750a576d8cc3019e3e4e57fcbb0d327f4f5e12a21e20aaf28`|

Installed visual bundles are not the same evidence as source gameplay. Wood axe reports Unity4.3.0f4 despite shipping with the later player; legacy component fields never replace active source rules by name alone.

## Wood axe

PrototypeItem26 `_modelPath=Prefab/Item/Equip/Weapon/Ax/wood_ax`; bundle containers wood_ax/meleeweapon. Mesh4, material2, texture3;256×256 format10, tintRGB~.902985/alpha1. GameObject6/Transform8 identity is the gameplay root; GameObject7/Transform9 rotates~-90°X and duplicates the mesh. Export one root.

UnityPy OBJ proof:37900 bytes, SHA256 `0da4cdbdf37c50b429c6e07c15a8c7d893916bffa5a37ded50282ad4ca9d8edd`; decoded PNG62052 bytes. Manifests retain original object IDs, conversion version, source/output hashes and transforms. Current attachment/action behavior has separate [tool presentation](SANDBOX_TOOL_PRESENTATION.md) contracts.

## Tutorial room reconstruction

TrainingRoomLoader instantiates scene_TrainingRoom at(12,1.5,12), yaw-90°, plus lights, exit sign, lifts, indicators, manager/music/pathfinder. Room geometry alone is not the full lesson.

| Mesh path ID | Vertices | Triangles | Use |
|---|---:|---:|---|
|−994153193595870446|65532|53359|Visual part0,21 submeshes|
|−4312639860208082577|2907|2259|Visual part1,15 submeshes|
|3516175124011679944|1179|828|TrainingRoom_Col, renderer disabled|

All three mesh objects have enabled MeshColliders:56446 collider triangles, not just the invisible828. Preserve renderer-material order across22 materials/35 textures and loader/root/child transforms. Source prefab has six objects and an additional Cube/collider; installed related bundle has five objects. Exact wrapper equivalence is not assumed. Exporter recovers both visuals/all three collider instances; `pe_training_view` shares BVH/controller geometry.

## Vegetation and effects

Alpine_tree05A mesh−3234636106914528023 uses identity transform, diffuse2048², normal1024², cutoff~.55. Preserve leaf alpha/discard; opaque cards are wrong. Wind/normal/shader parity is separate. This sample does not identify Tutorial's exact tree. HoloCameraControl renders a camera-following RenderTexture and toggles renderers; HolotreeAppearance scales/faces the source tree and writes _Scale. Recover serialized lesson references rather than substitute by resemblance.

## Coordinate and material conversion checks

UnityPy's OBJ exporter mirrorsX/normalX and reverses winding. A hierarchy using that route needs one consistent S=diag(-1,1,1), M'=S*M*S; never mirror twice.

**Implemented exporter instead reads raw MeshHandler arrays:** keeps UnityXYZ/meters/original winding, bakes parent×childTRS and source loader transform, applies inverse-transpose normalized normals and reverses winding only for negative hierarchy scale. Draw at identity. All228 axe triangle cross-products agree with stored normals; room origin/unit-axis tests check placement. Preserve submesh/material assignments, UVs, alpha, tint, queue and texture scale/offset. Main audited room/axe UVs are identity/zero; other assets require qualification. Unity shaders are not GLSL.

Exporter stages requested outputs in a sibling directory, rejects installation-as-output, replaces files and installs manifest last. Unselected cached assets remain; manifest describes current selection. It records hashes/sizes, selected root/hierarchy/baked matrices, ordered renderer slots, material properties, collider flags and bounds. Main diffuse/tint conversion does not qualify every normal/specular/emission/light/effect. Current source-bound material additions are in [actor](ACTOR_MATERIALS.md) and [building materials](BUILDING_MATERIALS.md).

## Native preview and parser guard

`pe_reference` is frontend-only: load after window creation, draw at identity and unload textures/model before close. Reject fallback triangle counts; failed load preserves prior model. Unload diffuse textures once, excluding shared default. No gameplay/save mutation.

Raylib6 tinyobj quadratic probing could hang at the17th room material at capacity20. Export deterministic short symbols with unique DJB2 buckets across growth sizes10/20/40, retaining source IDs via obj_symbol. Both32/64-bit hash tests cover this. This is a generated-format compatibility guard, not a general OBJ importer.

Before raylib loading, validate exporter headers/syntax, finite bounded exponents, positive in-range vertex/UV/normal indices, local safe filenames, material coverage and insertion collision freedom. Reject stale unsafe names/unsupported OBJ. `test_reference_validation.c` exercises this without graphics; installed-content export tests check repeatability, all collider/material associations and exact positive room placement. Source/object/hash and numeric checks are distinct from visual parity.

## Integrated assets and evidence owners

- Training emergency kit: source kit2 shares emergencykit3 bundle geometry; placement(7,1.5,12), yaw60°, scale1;1436 vertices/1572 triangles/one material. Its independent manifest retains prefab/GUID/path/collider data; kit-only export preserves room output. Separate collider and full shading remain distinct.
- [Actor recovery](ACTOR_RECOVERY.md), [hitboxes](HITBOX_RECOVERY.md), [player assets](PLAYER_ASSET_RECOVERY.md) and [animation](PLAYER_ANIMATION.md) own skeletal recovery. They supersede the old static-preview boundary.
- [Camera](SANDBOX_CAMERA.md) owns active graph values and coordinate conventions. Source Start yaw30°/pitch-20°/distance6m maps to the tutorial formula yaw150°; it is not a second handedness conversion.
- [Training UI](TRAINING_UI.md) and [Tutorial](TUTORIAL.md) own integrated lesson/presentation status. Current native fixture progression reaches756; initial746/no-combat statements are retired.
