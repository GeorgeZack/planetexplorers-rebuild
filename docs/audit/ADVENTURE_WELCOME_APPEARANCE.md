# Archived Adventure welcome appearance

The recovered native scene `rebuild/build/reference-assets/adventure-welcome/recording.peas` represents **Westina Nangle, entity20008 / RandomNpc17 / Adventure role1** from the preserved Adventure manual-slot1 archive. This is one saved appearance. It does not reconstruct the procedural NPC population or original settlement layout. Runtime loading uses PEAS/PEAC/PNG assets without Unity or archive dependencies.

## Source-derived identity and selection

The read-only input is `~/.local/share/planet-explorers-sandbox-reference/PlanetExplorers/GameSave/save0`. The shared bounded archive reader validates version5 headers, GUIDs, record ranges and component versions, then resolves equipment instances through `ArchiveKeyItemAsset` and the read-only source database. `AdRandNPCMission` role1 must resolve to RandomNpc17. The archived `EntityInfoCmpt` names Westina Nangle; `AvatarCmpt` and the nude avatar base both select `Prefab/PlayerPrefab/FemaleModel`. The saved position is `(-606,27.50515365600586,349)`, with a 180-degree Y rotation. These coordinates are provenance, not a placement contract for the bounded C world.

| Instance | Item | Appearance |
| --- | --- | --- |
| 23 | 76 | Female iron helmet, `female05-Helmet` |
| 24 | 99 | Female iron torso, `female05-torso` |
| 25 | 135 | Female iron legs, `female05-legs` |
| 26 | 196 | Female iron boots, `female05-feet` |
| 27 | 3 | Copper sword, source stowed `mountBack` |
| 28 | 36 | Copper shield, source `Bone L Forearm` |

The saved nude selection has hair01, head13, and Female torso/hands/legs/feet. Helmet76's replacement mask3073 selects HairF, HairT and HairB. `AvatarData.GetParts` deduplicates those three references into one helmet mesh; exporting three helmets would be incorrect. Head13 and the bare saved hands remain. The preserved profile has `HideHeadgear=False`; the exporter checks and hashes this setting.

All27 saved morph values and eye/lip/skin/hair colors remain in `welcome-manifest.json` and `source/welcome-rig.json`. Head13 uses14 signed morph parameters, iron torso6, iron legs4 and boots1. The shared reconstruction follows source single-frame morph channels and resource-local BakeMesh skinning, as described in [supplier appearance](ADVENTURE_SUPPLIER_APPEARANCE.md). Saved skin/eye colors are white, lip red and hair `(0.8549000,0.4078000,0.4941000,1)`; the helmet replaces all saved hair parts. The inspected color function does not apply lip color. PNGs bake recovered diffuse tint/masks, preserving alpha; original lighting, specular/normal shading, gamma and shader filtering remain outside scope.

The copper sword uses the installed exact `Prefab/Item/Equip/Weapon/Sword/copper_sword` resource. The source prefab's `m_HandChangeAttr.m_PutOffBone` is `mountBack`; the exporter verifies that source model mount path and local matrix against the isolated bake. It excludes the separate Ragdoll hierarchy, validates identity equipment roots and records mesh IDs/hashes and source attachment metadata. Shield36 retains the source left-forearm attachment. These are idle attachment defaults, not recovery of unsaved held/hidden/combat state.

## Idle, capsule and verification

The existing closed `female-clips-02.jsonl` bake supplies source FemaleAvatar `NormalIdle`. Gender, Avatar, all39 required bone paths and the sword mount match. The output has9 material parts,7,991 vertices and62 samples at30Hz with the exact PEACv2 terminal timestamp. The duplicated head material submesh has separate vertex records. NPC controller transitions, IK and dialogue gestures are not included; no new original executable launch or live Westina BakeMesh comparison was performed.

`welcome-collider.pesc` comes from the same `export_adventure_npc_collider.source_capsule()` used for Eitai: FemaleModel/Model physical CapsuleCollider13600000, enabled/nontrigger, layer10, center `(0,.875,0)`, radius `.3700000047683716`, height1.75, Y-axis. The manifest also records Defence13690738 and source outer-collider growth. Shared female geometry does not establish player blocking; source player/NPC layer10 self-collision remains disabled.

Eleven headless Python tests passed: seven existing supplier checks and four welcome checks covering role/prototype identity, multi-slot helmet replacement/deduplication, unchanged fixture bytes, invalid equipment sex and unknown mask rejection. Two fresh welcome exports produced identical20 runtime files; a fresh supplier regression export matched all25 existing supplier runtime files. All20 welcome manifest source hashes were rechecked after export, including original archives, database, settings, installed resources, source scripts/prefabs and closed bake. The original save and installed assets remain unchanged. Native runtime and graphical evidence are recorded separately by the host integration.

## Reproduce

```sh
/tmp/pe-asset-tools/bin/python rebuild/tools/export_adventure_supplier.py --npc welcome \
  --output rebuild/build/adventure-welcome-new
python3 -m unittest discover -s rebuild/tests -p test_adventure_supplier.py
python3 -m unittest discover -s rebuild/tests -p test_adventure_welcome_assets.py
```

The destination must be fresh. Omitting `--npc` preserves the supplier selection, filenames and default output. `--npc welcome` defaults to `rebuild/build/reference-assets/adventure-welcome`; current published assets must be preserved when repeating the export. Full source hashes, exact appearance, mesh/morph/material data and closed bake validation are retained in the manifest and `source` directory.
