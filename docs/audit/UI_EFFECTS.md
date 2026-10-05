# Recovered tutorial UI effects

The original blue highlights and scanlines are separate `UITexture` children of the ordinary atlas widgets. Applying a color filter to the atlas or fonts does not reproduce them. `GameMain.prefab` references `UISpecularHandler` for frame, dialogue and health highlights and `UIHolographicHandler` for the scanline layers. The handlers allocate materials from their source shaders and update their parameters each frame.

`NewUIAtlas.mat` and `NewIcon.mat` each store white `_Color`, their own `_MainTex`, unit texture scale and zero offset. Both reference the same shader GUID `e75727d9555d9d14ca51d91908c681bc`; that shader's metadata was not found in this repository. The new helper therefore leaves the existing atlas and font rendering unchanged and ports the independently identified effect shaders.

| Native role | Source component ID | Source logical footprint | Main parameters |
|---|---:|---:|---|
| DIALOGUE | 11436526 | 850 × 150 | Intensity .9, threshold .45, borders L193/T18/R16/B18 |
| FRAME | 11400316 | 844 × 110 | Intensity .9, threshold .7 |
| HEALTH | 11492306 | 164 × 30 | Intensity .5, threshold .7, four original red/purple/gold colors |
| MAP_LEFT | 11400330 | 20 × 208 | Intensity 1.1, T/B borders58 |
| MAP_RIGHT | 11400328 | 34 × 208 | Intensity 2.2, T/B borders58 |
| HUD_SCAN | 11400336 | 830 × 50 | Local y−5; intensity .2, speed .3, tile48×7 |
| MAP_SCAN | 11400322 | 148 × 208 | Local x−15; intensity .2, speed .3, tile48×7 |

The following roles are appended after the original seven, preserving their enum values:

| Native role | Source prefab/component | Logical footprint | Intensity / mask borders |
|---|---|---:|---|
| INVENTORY | ItemPackgeWnd / 11400144 | 385 × 578, local y−10 | .8; L50/T30/R50/B30 |
| STORAGE | WarehouseWnd / 11441708 | 385 × 460 | .8; L50/T30/R50/B30 |
| CHARACTER | PlayerInfoWnd / 11408934 | 620 × 602 | .9; L50/T30/R50/B30 |
| REPLICATOR | CompoundWnd / 11400532 | 903 × 492 | 1.8; L432/T80/R86/B310 |
| MISSION | MissionWnd / 11400174 | 1062 × 552 | 1.8; L432/T80/R86/B310 |
| STORAGE_SCAN | WarehouseWnd / 11430420 | 378 × 326, local y−35 | .2; scanline shader |

The first three use `midbg_mask.png` with wave threshold .7; replicator/mission use `bigbg_mask.png` with threshold .45. All five use the same four source blue colors and noise/wave parameters as the initial frame. Storage scanlines use the original misspelled `cneterBg_Mask.png`. The analogous inventory scanline node is inactive in the source prefab and is not added. PlayerInfoWnd also contains a second 733 × 569.9 big-mask effect under an inactive WndBg subtree; the CHARACTER role deliberately follows the active 620 × 602 mid-mask subtree.

Original gender sprites `female_icon` and `male_icon` come from the separate VCE Atlas referenced by PlayerInfoWnd, rather than NewUIAtlas/NewIcon. The main UI exporter now includes that original atlas PNG and just these two named sprite records: each is 28 × 28, at (1639,954) and (1682,794), respectively. This increases the native table to 2,474 sprites across three PNG atlases.

All recovered effect transforms have identity rotation and positive scale. The dialogue highlight is a distinct overlay with a border specification different from the base `talkbg` sprite. Its original base layers remain white and blue-tinted atlas sprites underneath it.

The source `UISpecular.shader` uses an independently sliced red-channel mask, original nearest-filtered repeating `RandomVectors.png`, four interpolated colors, and a time-dependent wave. `_FadeTime` is game time; Unity `_Time.x` is time/20. `UIHolographic.shader` generates horizontal scanlines from local logical height and modulates their intensity using the same noise texture. `UIHolographicHandler.GetTwinke` selects .13 while the original UI refresh flag is set and .01 otherwise. The native caller supplies that flag and its own simulation time; phase matching against the original running client is not claimed.

Both source passes use `Blend One One`, `Cull Off`, `ZWrite Off`, and `ColorMask RGB`. The native helper uses additive ONE/ONE blending, outputs zero alpha to preserve destination alpha, flushes before restoring normal alpha blending, and restores the default shader, depth mask and culling. It should be called in ordinary 2D UI drawing between the base panel and foreground text/icons, rather than nested inside another custom shader or blend scope.

`export_training_ui_effects.py` recovers eleven original PNGs and a JSON manifest containing exact serialized component values, transforms, texture import filtering/wrapping, source paths and SHA-256 hashes. The general `export_training_ui.py` now includes this export under `effects/`. Existing installations can add only the missing effects using:

```sh
/tmp/pe-asset-tools/bin/python rebuild/tools/export_training_ui_effects.py \
  --output rebuild/build/reference-assets/ui/effects
```

The destination must be fresh. The optional Python environment needs UnityPy and Pillow. Nothing writes to the Unity project or installed original game.

`pe_training_ui_shader_load(ui_directory, ...)` loads the original masks/noise and the translated GLSL. `pe_training_ui_shader_draw(..., role, destination, seconds, alpha, refresh)` draws a single source effect. The destination is that effect's own rectangle, not necessarily its parent panel. Source logical dimensions remain fixed when the UI is scaled to a different display resolution.

The holographic components leave `RandomTex` and `AlphabetTex` null. The translation uses the declared shader defaults: bump red .5 and white alphabet, respectively; null-texture fallback in this shipped Unity player has not been independently sampled. The specular null `MainTex` uses declared white. Original PNG bytes are preserved, but Unity's build-time texture compression and the original framebuffer/postprocessing can still affect appearance. These effects do not reproduce refraction, screen-space postprocessing or other window-specific animated components.

Validation: all thirteen masks/parameters are checked against the recovered prefab, all eleven PNGs match original bytes and recorded hashes, and both GLSL fragments pass `glslangValidator` without creating a graphics context. Strict C compilation passes. Root graphical validation of the initial seven roles passed: the recovered glow and scanlines were visually inspected in `/tmp/pe-ui-effects.png`. The six additional window roles have passed headless validation; their graphical validation remains pending.

Source files:
- `Assets/Scripts/Chenzhi/NewUI/Resources/Prefab/GameUI/GameMain.prefab`
- `Assets/Scripts/ZhouXun/UIEffect/Resources/Shaders/UISpecular.shader`
- `Assets/Scripts/ZhouXun/UIEffect/Resources/Shaders/UISpecularHandler.cs`
- `Assets/Scripts/ZhouXun/UIEffect/Resources/Shaders/UIHolographic.shader`
- `Assets/Scripts/ZhouXun/UIEffect/Resources/Shaders/UIHolographicHandler.cs`
- `Assets/Scripts/Chenzhi/NewUI/Resources/UIMask/`
