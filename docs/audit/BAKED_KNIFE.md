# Baked knife attachments

`rebuild/tools/export_baked_knife.py` exports the recovered knife33 mesh and original diffuse textures as a separate multipart PEAS scene. Each material has one rigid joint and the same ordered clips as an existing baked player scene. The tool reads a closed, fully validated isolated-Animator bake; it does not launch Unity or infer attack events.

The source attachment is `m_HandChangeAttr.m_PutOffBone = mountWaist` in the knife prefab. `BoneCollector.SetModelParent` (`Assets/Scripts/ZhouXun/Voxel Creation/Scripts/WhiteCat/PE-VC/BoneCollector.cs:41`) uses `SetParent(parent,false)`. The knife root has identity local TRS, required by this exporter. Its OBJ already contains prefab-local mesh transforms, so each palette is the baked model-local mount matrix with identity inverse bind. Multiplying another prefab root or bind correction would apply placement twice.

The female source path is `Bip01/Bip01 Pelvis/Bip01 Spine1/Bip01 Spine2/mountWaist`; the male source path ends under `Bip01 Spine1`. The exporter resolves the complete source chain for the selected sex and rejects a different or ambiguous baked path. It also compares the recorded initial mount-local matrix with source TRS. The baked mount is retained even though it is not a skin joint in the player mesh.

Run from the repository root:

```sh
python3 rebuild/tools/export_baked_knife.py \
  ~/.local/share/planet-explorers-bake-reference/pose-captures/female-clips-02.jsonl \
  --rig rebuild/build/reference-player-starter/player-female-starter-rig.json \
  --knife-collision rebuild/build/reference-assets/knife-collision.json \
  --visual-assets rebuild/build/reference-assets \
  --body-scene rebuild/build/animator-bake/player-female/all/recording.peas \
  --output rebuild/build/animator-bake/player-female/stowed-all
```

Output must be a fresh directory. Input mesh/textures must match the recovered visual manifest's byte counts and SHA256 values. Export occurs in a temporary staging directory; a failed validation does not publish a partial scene. Every body part must have exactly the same ordered PEAC2 clip headers as the knife, including names, frame counts, rates, durations and loop flags. The exporter reuses the body writer's terminal-time resampling, rather than guessing lengths from nominal animation names. The output manifest records source hashes, source mount metadata, and all reference body actor hashes.

The actual `female-clips-02` artifact produced two knife material actors with ten clips and 345 samples per material. Six headless regression tests pass, covering exact ordered clocks, native C terminal/intermediate rigid poses, missing or wrong mount transforms, ambiguous source chains, incomplete captures, changed source texture bytes, body clock mismatches and protection of an existing destination. The actual generated scene also passed native loader/pose checks under ASan/UBSan with leak detection: 690 posed sample checks across both parts and ten clips. No graphical validation was performed by this exporter task.

The default scene is **stowed geometry only**. It stays on the waist even in Attack6–9 clips; the host must select a separately verified drawn attachment before showing a weapon attack. There is no PEHB export or permission to apply damage. Put-on/put-off animation, procedural IK, controller transitions, first-person weapon behavior and original material shaders are not reproduced. The source bake completed its recorded poses, but its Unity process subsequently faulted during experimental playable cleanup; capture validity and runtime sampler lifetime safety are separate results. See [pose capture analysis](POSE_CAPTURE_ANALYSIS.md) and [player animation](PLAYER_ANIMATION.md).

## Drawn attachment and source capsule

Use the same command with `--attachment drawn` and a fresh output such as `rebuild/build/animator-bake/player-female/drawn-all`. The source `m_PutOnBone=mountMain` selects the full recovered hand attachment chain. Its initial local TRS is checked against the corresponding source mount exactly as for the waist. No root, hand offset or nominal swing pose is substituted.

The actual output contains `recording.peas`, two `knife-drawn-part-*-material-0.peac` actors, original diffuse textures, and **`knife-drawn.pehb` bound to `knife-drawn-part-0-material-0.peac`**. All ten ordered clip headers match every player body part. `recording-manifest.json` records `attachment_mode="drawn"`, `attachment="mountMain"` and the hitbox actor/file mapping. PEAC and PEHB by themselves do not encode drawn-versus-stowed provenance; the host must enforce that distinction when choosing its assets.

The PEHB contains one source `SingleHandSword` attack record, group RIGHT=1, rigid joint0, attack form0. It preserves the source Z-axis capsule radius approximately0.3m, height1.5m and offsetZ−0.1m: center endpoints approximatelyZ0.2 and1.1. Four bind points retain the source local-X radius measurement. The exporter verifies the capsule transform reference, direct source-root child relationship, source visual transform match, cached local points, layer mask34154496 and knife33's geometric invariants. The exact first-material skin/geometry fingerprint anchors the PEHB; an actor with modified geometry is rejected by the native loader.

Additional headless regressions cover native capsule poses at terminal clip time and under translated/rotated/scaled roots, rejection of the wrong actor geometry, and invalid capsule dimensions, axes, local points, transforms, parent metadata and attack form. The actual ten-clip drawn actor and hitbox passed all345 capsule samples under ASan/UBSan with leak checks; radius remained approximately0.3m throughout. Graphics and actual original-game hit outcomes were not tested by this export task.

This capsule is geometry, not a live damage event. The host must also verify a drawn/equipped weapon, grounded stationary third-person action eligibility, source animation-event contact windows, skill-guide permissions, actual posed target overlap and per-target hit deduplication. Stowed exports continue to omit PEHB entirely. Drawing/put-away transitions, moving or airborne attack skills, procedural IK and controller blending remain outside this exporter.
