# Sandbox tool presentation

The sandbox actor now supports source wood shovel12, wood pickaxe19, safety axe1527 and wood axe26. `pe_sandbox_actions` owns admission, stage and authored time; `pe_sandbox_actor` copies its `PeSandboxActionPose` and uses the same time for the recovered body and attached tool. Rendering performs no resource edits or damage. This is separate from the tutorial's hologram-specific tree timers and grants.

## Asset and clock contract

The existing `player` scene supplies six locomotion clips. Optional fixed attachments use `player-knife-stowed`, `player-shovel-stowed`, `player-shovel`, `player-pickaxe-stowed`, `player-pickaxe`, `player-axe` and `player-wood-axe`. The four action body/tool pairs are:

| Exact item | Body directory | Tool directory | Source clips |
| --- | --- | --- | --- |
| Wood shovel12 | `player-dig` | `player-shovel-dig` | Spade1, Spade2, Spade3 |
| Wood pickaxe19 | `player-mine` | `player-pickaxe-mine` | Mine1, Mine2, Mine3 |
| Safety axe1527 | `player-cut` | `player-axe-cut` | Felling1, Felling2, Felling3 |
| Wood axe26 | `player-cut` | `player-wood-axe-cut` | Felling1, Felling2, Felling3 |

Every directory contains `recording.peas`. Supplied action groups must be complete; an entirely absent optional group leaves its capability unavailable. All groups are decoded and checked before GPU upload. Body/tool names, fps, duration, frame count, loop and PEAC2 terminal semantics must match. The expected source female clips are additionally checked against closed native-player captures08/09: Spade durations approximately .700/1.333/.833 seconds and Felling durations .600/1.100/.600 seconds, at30Hz. These are authored pose clocks, not wall-time duration guarantees. The gameplay host applies the exact equipped tool speed.

`pe_sandbox_actor_action_ready(actor,item_id)` exposes capability. `pe_sandbox_actor_set_action(actor,pose)` copies the snapshot; NULL clears it. Nonfinite or invalid clip times and mismatched source clip families are rejected without replacing the previous snapshot. Before drawing, the actor rechecks the actual equipped item and instance ID, so changing equipment cannot retain the prior tool's action. Locomotion update, repaint and pause never advance action time. The inventory preview uses idle and the current fixed held/stowed attachment without mutating gameplay time.

## Exact wood axe recovery

Prototype26 maps to `Prefab/Item/Equip/Weapon/Ax/wood_ax`. Source prefab SHA256 is `58c24d9dc708f8b9a15363dc8b9156457cb979f8cdc82520366d0ea7e301b072`. PEAxe component11484812 specifies skill20110004, `fellAnim=Fell`, speed1 and stamina4; its mounts are `mountMain`/`mountWaist`, with `AxPutOn`/`AxPutOff`. Source root400000 and visual450868 have identity local transforms. The installed ResourceManager resolves the same resource; the exporter compares those transforms against the installed prefab before exporting its actual275 vertices and228 triangles. No safety-axe mesh substitutes for item26.

The existing axe tools now use a strict two-item allowlist. Their default remains safetyaxe1527. Both static and sampled outputs preserve source hashes and IDs. Fresh output destinations are required; choose a new `--output` when regenerating an existing directory.

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

The wood axe outputs contain one rigid material part each, with10 locomotion/knife-bake clips and3 Felling clips respectively. The exporter matches every ordered header against every supplied body part. Its geometry is source prefab-local; the one-joint palette is the actual sampled `mountMain` model-space matrix. Body and tool receive exactly the same world root, yaw and scale at drawing time.

## Validation and remaining differences

Headless actor tests and strict ASan/UBSan/leak checks pass group validation before upload, failure cleanup, locomotion from actual displacement, exact item/instance binding, all three action stages, shared time/root/yaw, cancellation and pause. Seven axe exporter tests pass, including preserved item26 identity, rejection of relabelled safety assets, actual275-vertex wood mesh and exact10/3 body clocks. A separate local headless integration check loads all11 real scene groups and CPU-skins nine action body/tool poses under sanitizers. These checks are not graphics observations; root performs the graphical integration tests separately.

The fixed female starter body, discrete clip changes, diffuse materials and omitted procedural IK remain limitations. Knife/shovel have recovered stowed attachments. Both axes currently render only while held; their `mountWaist` attachments and AxPutOn/AxPutOff transition clips are not installed, so undrawn axes are invisible. The inventory preview shows idle rather than replaying an active world swing.

Pickaxe19 uses exact recovered `wood_hoe` geometry and the closed female-mine-10 capture:91poses across Mine1–3,22/42/27frames at30Hz, durations approximately.700/1.333/.833s. All42body bones map and move. Held/stowed mountMain/mountBack attachments share the existing locomotion clocks. The full four-directory group is required; all14partial subsets are rejected before upload. The general host uses skill20110002's terrain rules, speed1, entry state speed1.5, stamina4 and50/20 thresholds, with exact Mine event times. [Pickaxe provenance and recovery](SANDBOX_PICKAXE.md) records the source/installed identities and every-frame native skinning checks. MinePutOn/MinePutOff transitions, source controller blending, arbitrary avatar/gender retargeting and full general tool coverage remain separate work.

## Recovered construction toolbar evidence

The bounded sandbox toolbar uses original `UIBuild.prefab` sprites; it is not a claim of complete original building tools. Source file `Assets/Scripts/Chenzhi/NewUI/Resources/Prefab/GameUI/UIBuild.prefab` has SHA256 `b69cdab867e411a027a86a77dece22c925b781391cabe2bbb03bffc9f5cace46`. Serialized UISprite components11400564 and11400508 both reference `shortbar_1` through external atlas58/path11400000:

| Component / object | Local position | Local scale | Pivot |
| --- | --- | --- | --- |
| 11400564 / SlicedSprite (midbg) 1 | (−307,−380.0000305,10) | (614,48,1) |3 |
| 11400508 / Bg 1 | (0,−443.0001221,0) | (614,59,1) |4 |

Both serialized tints are approximately RGBA(0,.12941177,1,.3019608). These are prefab-local geometry and tint, not a direct screen-coordinate mapping. The native toolbar's bottom anchoring and supported palette are defined by its frontend. Keys1–7 select cube/dirt/stone/wood/ramp/fence/ladder independently of normal quickslots. Point/four-cell brush, rotation and exit are functional; displayed creation-saving and selection-deletion controls remain disabled. Shared hit rectangles consume toolbar clicks while leaving the build canvas available; modal/closing-frame input and inventory ownership have headless regression coverage.

See [source camera investigation](SANDBOX_CAMERA.md) for the distinct camera obstacle mask, original wheel-zoom parameters and the current visibility limitation around recovered trees. No foliage fading or extra camera collision was introduced by this audit.
