# Recovered wooden shovel visual

Item **12**, the source wooden shovel, now has a diffuse static export and two rigid animated attachment exports. Geometry comes from the installed original; mount transforms come from the source player prefabs and are checked against the closed source bake. These are visual assets, not evidence that mining occurred in the original.

## Source identity

Repository resource `Prefab/Item/Equip/Weapon/Spade/wood_spade` maps to item12, equipment mask **528** and bind node **5**. The source `wood_spade.prefab` SHA-256 is `04216f14143156cfea9d4a5088198f5d6ad4f12c16955111aecadbd246cef71e`. The exporter checks those database fields and compares root Transform400000 and visual Transform434150 with the installed ResourceManager object. Both transforms are identity. The installed root GameObject is **18710**, and the visual mesh is **6662** in `resources.assets`: **211 vertices / 140 triangles**, one diffuse material. Mesh raw-data SHA-256: `00497d29c93846ec4c9aebbecaa356c7688dc1328ed5e5db064dbf2a2b055213`.

The original PEDigTool component11483140 specifies `mountMain` while held and `mountBack` while stowed, with MinePutOn/MinePutOff animation names. The female animated mountMain Transform400208 lies beneath the right hand; mountBack Transform400268 lies beneath Bip01 Spine3. The exporter traverses the actual model hierarchy and excludes ragdoll mount chains. It checks the exact relative path and local matrix against the selected closed bake; a matching basename alone is insufficient.

## Reproduce

Static recovery requires the optional UnityPy1.25.4 environment and a legally installed original:

```sh
/tmp/pe-asset-tools/bin/python rebuild/tools/export_reference_shovel.py
```

The fresh output directory is `rebuild/build/reference-assets/wood-shovel`, containing `wood-shovel.obj`, MTL, source diffuse PNG and `manifest.json`. Source geometry, triangle order, normals and UVs follow the established Unity XYZ convention; the visual child transform is baked once. No original asset or game file is modified. Existing output is refused.

Baked attachment recovery uses only Python's standard library:

```sh
python3 rebuild/tools/export_baked_shovel.py \
  "$HOME/.local/share/planet-explorers-bake-reference/pose-captures/female-clips-02.jsonl" \
  --rig rebuild/build/reference-player-starter/player-female-starter-rig.json \
  --visual-assets rebuild/build/reference-assets/wood-shovel \
  --body-scene rebuild/build/animator-bake/player-female/all/recording.peas \
  --output rebuild/build/animator-bake/player-female/shovel-all
```

Add `--mode stowed` and choose a fresh output such as `shovel-stowed-all` to recover the back attachment. Both ten-clip outputs are locally available. Each is a one-part PEAS scene with one rigid identity bind and source mount matrices. Every ordered clip's name, duration, rate, frame count, loop flag and terminal-frame semantics must match every body actor before output is committed. The exporter does not synthesize a missing mount or alter a mismatched clock. Input hashes, exact mount provenance and limitations remain in its manifest. The same tool accepts a separately completed Spade1–3 bake with its matching body scene.

## Checks and boundaries

Six `test_baked_shovel.py` tests pass: native synthetic posed geometry under sanitizers, exact shared clocks, mountBack selection, invalid attachment/item/root/hash rejection, incomplete/mismatched capture rejection, preservation of existing output and checks on the optional real ten-clip and three-clip digging artifacts, including distinct main/back transforms. Source root/visual transforms match the installed assets. No graphical shovel alignment or original-runtime mining result is claimed by these checks.

Fixed held/stowed scenes do not reproduce MinePutOn/MinePutOff transition timing. No attack activation, digging contact, terrain edit, durability loss or mission progression is inferred from the visual. Material conversion uses source diffuse texture/color only; shader effects are not reconstructed. Clip-only bake limitations—including controller transitions, procedural IK, root motion and affine sample interpolation—remain as documented in [ANIMATOR_BAKE.md](ANIMATOR_BAKE.md). Action and permission rules belong to [TRAINING_MINING.md](TRAINING_MINING.md).

## Completed digging bake

Closed capture 09 contains 91 source poses, with Spade1/2/3 frame counts 22/42/27. Its matching body scene is `rebuild/build/animator-bake/player-female-dig/all/recording.peas`. Held and stowed outputs are `rebuild/build/animator-bake/player-female/shovel-dig-all/recording.peas` and `shovel-stowed-dig-all/recording.peas`. Their clocks match the body exactly; all mount palettes vary over the corresponding source motion. The actual native loader/skinning implementation evaluated all frames for the body and both shovel attachments under sanitizers. These outputs do not apply the source Spade1 controller speed 2, transition blending or event dispatch; those remain host behavior. Original-runtime mining and graphical shovel alignment remain unobserved here.
