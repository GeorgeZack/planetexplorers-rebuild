# Training tree ray and safety axe assets

The next tree lesson uses original HoloTree collider geometry and safety axe1527. The headless ray helper and recovered axe assets below are implemented and tested. The frontend now has optional matched axe/body cutting-scene loading and stage-clock rendering; its extended tree/crafting graphical validation is pending. They do not independently start cutting, apply damage or complete a mission. The native herb route through719→748 passed6000frames; the subsequent tree/script/craft frontend is still being integrated.

## Exact tree query

`pe_training_tree_hit(origin,forward,&distance)` in `rebuild/src/pe_training_tree_hit.c` tests the capsule on source HoloTree root GameObject167750 / CapsuleCollider13687620. Radius is1m, total height7m, axisY, world center approximately`(11.120831,4.582331,11.879339)`. Capsule sphere centers are2.5m above and below its center. The exported `gathering/manifest.json` retains the original local collider fields and full world matrix; the helper rounds the resulting center once to native float. The tiny serialized quaternion length error is below a micrometer at this scale; no runtime scale is invented.

`HolotreeCutter.Update` explicitly uses `Physics.RaycastAll(new Ray(mplayer.position,mplayer.forward),2f)` and accepts a hit whose Transform is the HoloTree root. Consequently the caller supplies **actual character feet/root position and body forward**, not eye position or camera direction. The helper normalizes direction and applies fixed2m reach. It queries the one authored root capsule without an extra nearest-obstacle restriction: the original checks all hits. The billboard child treeCollider is a different object and must not substitute for the root capsule.

The query handles cylinder sides, rounded caps, tangency, parallel rays and endpoint range. An optional distance is written only on success. Zero/nonfinite directions, invalid origins and origins strictly inside the capsule miss. Inside-origin exclusion follows [Unity's raycast contract](https://docs.unity3d.com/ja/current/ScriptReference/Physics.Raycast.html); boundary behavior has not been experimentally compared with the old player's PhysX implementation. The host enables queries only when the task activates its initially disabled collider, and disables them after depletion. The movement controller now also uses this solid tree while mission748 is active and tree HP is positive. `pe_training_tree_overlap` and `pe_training_tree_sweep` combine with room BVH queries through an explicit per-step active flag; there is no mutable global collider. Initialization leaves the obstacle disabled, and depletion removes it immediately.

At the recovered room player floor height around1.647m, the ray intersects the lower rounded cap rather than the vertical cylinder. A player2.5m horizontally from capsule center can hit in approximately1.6m. A center-distance or AABB test would give different reach and is not used.

`tests/test_training_tree_hit.c` exposes `pe_test_training_tree_hit`. Strict C11 warnings-as-errors and ASan/UBSan/leak tests passed side/cap hits, actual floor height, inclusive2m versus just-outside reach, normalized direction, away/parallel/tangent/miss rays, inside origins, nonfinite inputs and unchanged failed output.

## Source safety axe

Prototype1527 maps to `Prefab/Item/Equip/Weapon/Ax/safety_axe`, not the wooden-axe bundle. The source prefab is `Assets/Resources/Prefab/Item/Equip/Weapon/Ax/safety_axe.prefab`. Its root transform is identity; the visual mesh child rotates90° aboutX. The installed resource resolves the actual mesh and diffuse texture:706vertices and524triangles. Export composes that visual child transform exactly once into **axe-root-local** positions/normals.

```sh
/tmp/pe-asset-tools/bin/python rebuild/tools/export_reference_axe.py
python3 rebuild/tools/export_baked_axe.py \
  "$HOME/.local/share/planet-explorers-bake-reference/pose-captures/female-clips-02.jsonl" \
  --rig rebuild/build/reference-player-starter/player-female-starter-rig.json \
  --visual-assets rebuild/build/reference-assets/safety-axe \
  --body-scene rebuild/build/animator-bake/player-female/all/recording.peas \
  --output rebuild/build/animator-bake/player-female/axe-all
python3 rebuild/tests/test_baked_axe.py
```

Both tools require a fresh output directory. The static export uses optional UnityPy; the baked exporter needs only Python's standard library and a completed validated bake. It can also export the three Felling clips by supplying that closed bake and its exactly matching body scene; the10clip locomotion bake is not a replacement for those cutting poses.

Static output is `safety-axe.obj`, its MTL/PNG and `manifest.json`, with source prefab hash, selected installed mesh hash, source/installed transform comparisons, equipment fields and male/female mount chains. The baked output is one rigid PEAC2 actor plus PNG and `recording.peas`, using `mountMain`. The exporter validates the source root identity, matching character model path and mount localTRS. It checks every ordered clip header against all body parts, including fps, frame count, duration, loop and terminal semantics. The root passed to rendering must be the same player root/yaw/scale as the body; the actor already includes the mount transform.

Source equipment metadata selects `mountMain` when drawn and `mountWaist` when stowed, using `AxPutOn`/`AxPutOff`; this export represents the fixed drawn attachment only. `CutPoint` is axe-root-local`(-.041,0,.312)`. Source PEAxe fields include `m_FellSkillID=20110004`, `m_AnimSpeed≈1.22`, `m_StaminaCost=4` and `fellAnim="Fell"`. The special hologram cutter still uses its audited seven-hit script; these general tool fields do not prove a separate stamina charge or modify that counter.

Both knife33 and safetyaxe1527 occupy equipment mask16. `EquipmentCmpt.TryPutOn` checks overlapping masks, verifies the old item can be removed and the inventory receiver can accept it, then takes off the prior equipment. Thus the bounded training host correctly replaces equipped knife with equipped axe while retaining both owned items. This is distinct from having two simultaneously equipped weapons and choosing a current one.

Five baked-axe exporter tests pass, covering exact two-clip metadata and native rigid-pose endpoints, incorrect item/mount/root, altered source hashes, incomplete bakes, mismatched body clocks, existing-output preservation and the actual recovered10clip scene. The actual706vertex axe actor also passed the native headless scene loader and every midpoint vertex pose in all10clips under ASan/UBSan/leak checks. The static OBJ passed the bounded native reference validator. No axe graphics or cutting pose success is claimed by these headless results; controller transitions, procedural IK, original material shaders and tool activation are separate work.

## Native cutting presentation boundary

The frontend validates every ordered axe/body clip clock before loading the optional `player-axe`, `player-cut` and `player-axe-cut` groups. The three cutting names resolve explicitly to Felling1/2/3, and body and axe share the same Entry/Cut/Exit `tree_time`, position and yaw. Felling3 lasts approximately.6s in the baked asset while the original special cutter exits after.533s, so this presentation is intentionally cut short by its gameplay timer. The two vertical capsules use an exact Minkowski difference: player-feet space has center `treeCenter−(0,.9,0)`, half-axis3.1 and radius1.3. Sweeps return earliest contact fraction and an outward normal; strict overlap excludes exact touching. Tangent and separating contacts do not block sliding. The controller chooses the earliest room/tree collision for movement, step attempts and floor probes. Regression tests pass blocked walking, sliding around the trunk, a short jump that cannot tunnel through it, passage above its top, and passage after HP0; the original full core movement suite also passes.

Native grounded/stationary/no-melee readiness currently substitutes for Animator layer1 DoNothing; that is an explicit controller approximation, not a recovered transition graph. The fixed mountMain axe representation also omits the source draw/stow transitions.

## Closed cutting bake08 and matching axe output

The closed capture `female-clips-08.jsonl` (SHA-256 `c552e6542d06042de3161c117476c4ef70d93769294058b04a5c47173ff30c3e`) contains75 validated authored samples. Its nine-part body scene is `rebuild/build/animator-bake/player-female-cut/all/recording.peas`. The matching one-part axe scene has now been exported to `rebuild/build/animator-bake/player-female/axe-cut-all/recording.peas` with identical ordered PEAC2 clocks:

| Clip | Frames | Authored duration | Loop |
| --- | ---: | ---: | --- |
| Felling1 | 20 | .600000381s | No |
| Felling2 | 35 | 1.100006104s | Yes |
| Felling3 | 20 | .600000381s | No |

Use the same export command above with the08 capture, the matching body-cut scene, and the fresh `axe-cut-all` output. The native headless loader and all706 vertex midpoint poses in every cutting clip passed ASan/UBSan/leak checks. An independent binary fixture check verifies exact body/axe clip headers and nontrivial first-vertex midpoint motion of approximately.148m/.531m/.464m; the looping Felling2 terminal point equals its initial point. These values check actual recovered movement, not attack activation or tree contact. Root owns installation of these scenes as `player-cut` and `player-axe-cut` and the extended graphical playthrough.
