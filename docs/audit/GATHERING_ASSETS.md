# Recovered gathering props and frontend contract

`rebuild/tools/export_training_gathering_assets.py` exports the five actual tutorial herb meshes and the tree from the installed `trainingmanager_new` resource. No Unity runtime, graphics context or desktop input is used. The native training frontend now loads these optional props and connects real-position herb interactions to the headless gathering module. The implemented endpoint is mission748 with one safety axe; the6000-frame native smoke passed through all five real-position herb timers and the one-time axe grant. This exporter itself never changes missions or bypasses the preceding combat.

```sh
/tmp/pe-asset-tools/bin/python rebuild/tools/export_training_gathering_assets.py
python3 rebuild/tests/test_training_gathering_assets.py
make -C rebuild test
# Graphics test: run separately after the headless checks.
make -C rebuild training-gathering-smoke
```

The default output is ignored `rebuild/build/reference-assets/gathering`. An output directory must be new; select another `--output` to regenerate without overwriting existing assets. `--game-dir` selects the read-only original installation. All files are generated in a sibling temporary directory and published together after success. The optional asset environment uses UnityPy1.25.4.

## Export contract

Load `herb-0.obj` through `herb-4.obj` and `tree.obj`, resolving each prop’s matching `.mtl` and the four shared PNG textures. Each MTL contains exactly the material used by its OBJ, matching the native bounded-loader contract. Vertices and normals already include the full authored world transform: **draw at zero translation, identity rotation and unit scale**. Do not apply the training root `(12,1.5,12)` or its −90° yaw again. Each herb index matches `pe_training_gathering_herb_position(index)` and the source HoloherbTask gather array.

| Prop | Installed mesh path ID | Vertices | Triangles |
| --- | ---: | ---: | ---: |
| herb-0 / orgherb1 | 6866 | 1285 | 1373 |
| herb-1 / orgherb2 | 6580 | 1136 | 1275 |
| herb-2 / orgherb3 | 6635 | 628 | 592 |
| herb-3 / orgherb4 | 6449 | 1581 | 2215 |
| herb-4 / orgherb5 | 6483 | 493 | 510 |
| tree / orgtree | 6039 | 3150 | 4216 |

All six meshes reside in installed `PE_Client_Data/resources.assets`. Lookup resolves ResourceManager `trainingmanager_new` to GameObject120048 and follows exact child names. Each ancestor's position, rotation and scale is checked against `Assets/Scripts/Jaluca/TrainingField/Resources/TrainingManager_New.prefab`; differences fail export. Full GameObject trees are decoded because UnityPy's optimized name cache can confuse source-editor and installed-player object layouts in one process.

Source `orgherb1..5` and `orgtree` serialize with zero scale. `HoloherbAppearance.FadeHoloherb` and `HolotreeAppearance.FadeHoloTree` explicitly grow those leaf transforms to unit scale at completion. Export changes only that leaf scale to1, representing fully produced props. Every other source transform remains unchanged. The source herb visual origins are about.3m below their gather-script positions; interaction must use the manifest's separate `gather_world_position`, not render bounds or a mesh centroid.

`manifest.json` records source prefab hash, installed mesh identity and raw mesh hash, source/installed transform equality, composed matrices, bounds, material properties, source appearance fields, original capsule fields and their world transform matrices. Collider matrices retain original nonuniform scaling. They are evidence for a future Unity-capsule equivalent, not a claim that raylib's render-mesh hit test duplicates Unity physics. Do not convert render triangles into solid leaf walls.

Meshes are bounded to one million vertices, two million triangles and64 material slots; channel values must be finite and triangle indices valid. Normals use inverse-transpose transforms, mirrored hierarchy winding is corrected, original XYZ and UV conventions are retained. Four diffuse textures/materials are recovered, including their alpha channels and source tints. Source foliage uses alpha cutoffs .5 for Desert/Forest materials, approximately.65 for Grassland A and.25 for the tree’s Grassland B. The default native model shader does not reproduce those cutoffs; transparent leaf texels can still affect depth until an appropriate discard shader is installed. Bump maps, cutoff/lighting and custom vegetation shader behavior remain outside this diffuse preview.

## Native integration and next boundary

The verified content sequence is **719 → 748 → 757**, followed by721;720 is not the successor in this database. The current frontend implements the herb portion through arrival at748, shows the recovered tree, and reports that tree cutting is not available yet. It does not grant wood or continue to757. See [gathering audit](TRAINING_GATHERING.md) for mission grants, dialogue and tree timing.

1. Accept719 only through the existing real kill/death/dialogue handshake. Explicitly initialize/enter the gathering module once. Display the source719 dialogue before enabling gather input.
2. Show the five exported meshes at their baked positions while `herb_dead[index]` is false. On the real GatherHerb key edge, call `pe_training_gathering_begin_herb` using the character's actual world position. The frontend selects the nearest eligible herb with strict squared3D distance below7 and calls the training bridge. Source scripts independently evaluate every herb, so all valid nearby herbs may receive the same input; this nearest-only choice is an explicit frontend restriction. Do not claim source aim-ray or occlusion gating: the gather script only requires strict squared3D distance below7, not dead, not already gathering and the gather input.
3. Step the module with actual elapsed fixed game time, suppress movement/rotation according to its movement-lock query, and deplete at1.1s. Local activity ends at1.966s; no inventory herb or Herbal Juice is granted. The final fifth depletion completes719. The original visual fades over.3s and the parent closes after.5s; the current frontend hides depleted props immediately, a visual simplification.
4. Acknowledge the proper transition and accept748 once. Grant exactly one safety axe1527 through one inventory authority, not both the standalone ledger and a second inventory copy. Show the real tree only for its lesson. A tool model and a character Gather/Fell animation are separate asset/controller needs; this exporter supplies props only.
5. The next, unimplemented frontend extension needs tree Cut input that must use the actual character-forward2m source collider query, real axe/equipment-held state, and verified animation readiness; passing unconditional true booleans would fake those conditions. Existing module preserves seven successful hits, source entry/cut/exit timing, release cancellation and one wood279. Transition757 only after this actual completion and dialogue handshake.

The original HoloCameraControl temporarily activates the original meshes, renders them to a screen-sized render texture and hides them; separate billboard planes use that texture, distortion/glow and appearance fade. Directly rendering these recovered plant meshes is a useful faithful-geometry preview, **not a reproduction of the hologram compositor**. The exporter preserves source positions and shapes rather than inventing new props or placements.

## Validation

Five standard-library tests pass, including the actual optional exported fixtures. They check source-root yaw/translation composition, unit leaf scale without changing the source tree, singular or changed transform rejection, invalid/nonfinite mesh channels and bad indices, six exact real vertex/triangle counts, actual per-herb interaction heights, four valid PNGs, failed-export cleanup and existing-output preservation. Both source geometry recovery and installed/source transform comparison completed successfully. The native bounded OBJ/MTL validator also passed all six actual exports headlessly, including exact triangle counts. Export tests now preserve its required headers and per-file material set. The6000-frame native graphical smoke exited0 at mission748 with all five herbs depleted, axe count1 and gathering ledger748. The models loaded without native errors. The endpoint screenshot has the tree outside the camera view, so this establishes the full route/timer integration, not a close visual comparison of each plant or its transparency. See [native gathering endpoint](../reference/rebuilt-training-herbs-complete.png). The6000-frame `training-gathering-smoke` requires the preceding actual blade contacts and hare death, then walks the character to all five herbs, waits for real gathering timers, acknowledges dialogue and requires mission748, five depleted herbs and exactly one safety axe; it cannot pass by setting the objective counter directly. The3600-frame combat smoke still stops at719, and the1500-frame sequence stops before any kill.
