# Sandbox pickaxe: source evidence and next implementation slice

Status, October 5, 2026: **source identity, isolated Mine pose recovery and bounded physical C gameplay verification are complete.** The initial audit read repository and installed assets without launching the original. The root operator subsequently ran the bounded sampler and obtained the completed 91-pose capture recorded below. No original-client pickaxe gameplay action or resource outcome was observed. The source-timed item 19 host and exact renderer groups have now been implemented by the gameplay/presentation agents; source controller blending and broader action parity remain limited. Existing shovel poses must not be substituted for Mine poses.

## Exact tool identity

Item19 is Wood Pickaxe, mapped by `PrototypeItem` in [localData.bytes](../../Assets/Resources/DataBase/localData.bytes) to `Prefab/Item/Equip/Weapon/Hoe/wood_hoe`. It is not a shovel or an interchangeable axe visual.

The source [wood_hoe.prefab](../../Assets/Resources/Prefab/Item/Equip/Weapon/Hoe/wood_hoe.prefab) SHA256 is `72410f7b7daf762d076347d9e950b76c81abbacc313a3117980929d7a00583e4`. Its PEDigTool component11466528 overrides the [class defaults](../../Assets/Scripts/Items/PEEquipment/PEDigTool.cs):

| Field | Serialized value |
|---|---|
| `m_SkillID` |20110002 |
| `m_AnimName` / `m_AnimSpeed` |Mine /1 |
| `m_StaminaCost` |4 |
| `m_AnimDownThreshold` |50,20 |
| `m_DigMask` |27 |
| Held / stowed mount |`mountMain` / `mountBack` |
| Draw / stow animation |`MinePutOn` / `MinePutOff` |
| Third-person / first-person camera index |2 /1 |
| Camera offset |(0,−.10000000149,0) |
| `m_ApplyAimIK` / `m_SyncIKWhenAnim` |false / true |

The item database records durability3553, wear5 per equipment-attack message, attack65, ResDamage0, ResRange0, ResBouns1, equipment position528 and bind node5. Recipe7 produces one Wood Pickaxe from ten Wood279 in three seconds. These are content records, not evidence of a tested original crafting/mining outcome.

The installed ResourceManager entry `prefab/item/equip/weapon/hoe/wood_hoe` resolves to `resources.assets` GameObject18702. Its `Model` child114849 uses mesh6342 named `wood_hoe` and material930. Installed root transform123119 and model transform161124 are identity, matching source root400000 and model402602. The auxiliary Target transform is position(−.164000004529953,0,.6740000247955322), rotation quaternion(0,0,−.7071068286895752,.7071067094802856); it is not a transform to bake into the visible mesh. The one-mesh/one-material model makes exact offline visual recovery practical. The subsequent exporter recovered216 vertices / 188 triangles and one diffuse material/texture; source/installed transform checks and recorded hashes passed. It does not reproduce the original material shader. Graphical presentation is a separate operator-owned check.

## Verified Mine states and events

The installed `PE_Client_Data/sharedassets1.assets` contains FemaleAnimator2 controller779 and MaleAnimator override780. Direct leaves `FullBody.Dig.Mine1`, `Mine2` and `Mine3` reference the following clips; male overrides only the loop among these three. All have authored sample rate30Hz.

| State | Female clip ID | Male clip ID | Authored duration (seconds) | State speed | Loop |
|---|---:|---:|---:|---:|---|
| Mine1 |221 |221 |.6999969482421875 |1.5 |false |
| Mine2 |228 |234 |female1.3333358764648438; male1.6000022888183594 |1 |true |
| Mine3 |237 |237 |.8333358764648438 |1 |false |

| Loop event | Female authored seconds | Male authored seconds |
|---|---:|---:|
| `AnimEvent("ChangeTarget")` |.16452839970588684 |.17536334693431854 |
| `AnimEvent("DigTerrain")` |.838300883769989 |.9432708024978638 |

Mine1 and Mine3 contain no serialized clip events. Mine3's controller behavior range for full-path hash2425381971, layer1, selects StateMachineBehaviour1183. Its script reference is sharedassets0 MonoScript1916, the same `AnimatorEventSender` used by the verified shovel exit. The serialized fields have empty enter event, exit event `DigEnd`, and zero timed events. [AnimatorEventSender.cs](../../Assets/Scripts/Anim/AnimatorEventSender.cs) sends that event on state exit. Do not invent a Mine3 clip event or equate it with the clip's last frame.

Controller transitions are:

- Mine1 → Mine2: normalized exit `.6000000238418579`, fixed transition `.4000000059604645` seconds, offset0.
- Mine2 → Mine3 when Mine is false: normalized exit `.800000011920929`, fixed transition `.20000000298023224` seconds, offset0.
- Mine3 → exit: normalized exit `.8999999761581421`, non-fixed transition `.10000000149011612`, offset0.

Authored clip time, state speed, tool stamina slowdown and transition blending are separate quantities. The existing native tool host approximates transitions discretely. Extending that host with these constants does not establish Unity blend, root-motion, procedural IK or draw/stow transition parity.

## Action and transaction rules to retain

[Action_DigTerrain.cs](../../Assets/Scripts/PeEntity/Cmpt/Motion/PEAction/Action_DigTerrain.cs) is shared by this pickaxe and the shovel. Admission requires an active indicator and nonbroken equipped tool. Indicator reach is strictly below3m from player feet+up1 to the snapped sample. `PEDigTool.Update` sets indicator radius from live ResRange; an editor indicator radius is not an independent gameplay footprint.

Starting captures the target and charges stamina. `ChangeTarget` preserves the first locked target, then accepts later valid indicators. `DigTerrain` dispatches the exact tool skill, sends `Battle_EquipAttack`, handles breakage, and charges continuing stamina unless already ending. A valid first attempted swing can wear the tool even if the selected sample is empty. Stamina cost is multiplied by the player's reduction attribute; tool speed1 is multiplied by.9 at each post-charge threshold50/20 and clamped to.5–1.5. Mine1's state speed1.5 is an additional factor, unlike the shovel entry's speed2.

Release clears Mine and starts the five-second fallback. `DigEnd`/`OnEndFullAnim` can end the action sooner; immediate cancellation resets FullBody and clears masks/subacceleration. Preserve the current C conservation repair for capacity-blocked edits rather than inferring that every animation event must grant resources.

`skMain`20110002 and shovel20110001 have zero cooling/preparation/main/ending durations and share trigger233. The pickaxe uses prepare effect128 (sound8, effect46); the shovel uses prepare129 and one-time130. [SkInst.cs](../../Assets/Scripts/SkillSystem/SkInst.cs) loads these records. [SkEntityVoxelTerrain.cs](../../Assets/Scripts/SkillSystem/SkEntityVoxelTerrain.cs) derives terrain damage/radius/resource bonus from caster attributes and delegates to [DigTerrainManager.cs](../../Assets/Scripts/BuildNDig/DigTerrainManager.cs). These shared transactions are reusable; the skill row's zero cooldown is not a substitute animation clock. Sound, impact effects, rare bonuses and the full skill interpreter remain separate work.

## Initial native gaps and bounded implementation plan

At the initial audit, [pe_sandbox_actions.c](../../rebuild/src/pe_sandbox_actions.c) supported exact items12/26/1527 only. Its dig branch assumed Spade names, event times, threshold40 and entry speed2. [pe_sandbox_actor.c](../../rebuild/src/pe_sandbox_actor.c) likewise had shovel/felling groups only, and item19 stamina came from generic cost5 in [export_content.py](../../rebuild/tools/export_content.py). The current implementation adds exact item19 timing, cost4 and its own four-group presentation. The complete Mine group gates action admission; generic and source-timed paths must never dispatch together. Historical generic behavior is not evidence of Mine event timing.

A narrow next slice is:

1. Extend [inventory_reference_bake.py](../../rebuild/tools/inventory_reference_bake.py) with verified direct Mine leaves221/228/237, the male234 override, source events/transitions and runtime identity-ambiguity checks. Extend the allowlist in [ReferenceAnimatorBake.cs](../../rebuild/tools/ReferenceAnimatorBake.cs) and cross-language tests together. Preserve the existing ten-clip default and maximum ten selected clips; Mine is a separate three-clip selection. The launcher imports the catalog, so no new launch mechanism is needed.
2. Add an exact pickaxe visual exporter using [export_reference_shovel.py](../../rebuild/tools/export_reference_shovel.py)'s source/installed transform and material checks. Retain item19 identity, source hashes and Main/Back mount chains; do not relabel shovel output.
3. The designated original-game operator performs one fresh isolated female Mine1–3 bake with the existing [reference_animator_bake.py](../../rebuild/tools/reference_animator_bake.py) flow. At30Hz the offline lengths imply91 total samples including terminal guards; validate actual closed output rather than assuming that count proves success. Preserve bounded graph retention, completion provenance, the120-frame lifetime watchdog and exit evidence. Agents must not launch or operate the desktop incidentally.
4. Use unchanged [analyze_reference_bake.py](../../rebuild/tools/analyze_reference_bake.py) to export actual Mine body parts. Add a pickaxe attachment exporter following [export_baked_shovel.py](../../rebuild/tools/export_baked_shovel.py), with strict body/tool clock equality. Existing completed ten-clip locomotion samples can supply exact held/stowed pickaxe mount animation. Existing Spade/Felling recordings cannot supply Mine poses; no completed Mine capture existed at the initial audit. The subsequent allowlist extension and run10 below provide that missing evidence.
5. Extend the authoritative action host by explicit tool family/metadata, not more assumptions that every dig tool is item12. Admit item19 only with a complete matching actor group, bind the equipped instance, use Mine authored clocks/events, and reuse the conserved terrain transaction. The renderer reads the same action snapshot and never advances a second edit clock. Reuse existing target selection and release/cancellation semantics.
6. Add focused female/male event-boundary tests, entry-speed and50/20 slowdown cases, first-target locking, empty-attempt wear, blocked-capacity conservation, equip replacement, pause/cancel/release behavior and exact pose-clock matching. Validate malformed/partial asset groups and cleanup headlessly. Then perform operator-controlled graphics checks and a bounded physical-input fixture proving edits happen at Mine events with the same instance and correct wear.

The recovery now uses the existing C actor formats and body analyzer without modifying original game/source files. The implementation steps above remain the dependency order for extending another exact tool; their completion does not imply original gameplay parity. Initial scope can remain the already supported female, grounded third-person tool host; arbitrary genders, moving/air actions and full Animator blending require separate evidence.


## Completed recovery checkpoint

The catalog and managed allowlist now include Mine1–3 alongside the previous 16 clips. Default selection remains the original ten, and at most ten may be selected in one bake. Thirteen launcher/inventory tests pass, including cross-language allowlist equality, exact direct Mine leaves, female/male metadata and preservation of authored time versus entry state speed. The sampler was compiled offline before the original was launched; no sampling, graph-retention or isolation code changed.

The root operator completed `~/.local/share/planet-explorers-bake-reference/pose-captures/female-mine-10.jsonl`:91 samples in22/42/27 frames, FemaleAvatar/FemaleAnimator2, three independent isolated clones. Strict analysis resolves all 42 body skin bones with no missing mapping. All 42 change in every clip. Maximum hierarchy reconstruction error is1.049657993e−6; maximum bone displacement from each clip's first pose is .597514/.947504/.584547m. Requested authored time and source event metadata are retained; this does not measure the live controller's transitions or root-motion path.

| Provenance input | SHA256 |
|---|---|
| Closed Mine capture |`a006463469ba00bf7768f1e5748d46437f74d9fb205848a09672ec93f62bc309` |
| Installed sharedassets1.assets |`d9c65f820a2186116d9eb73d815fe07d9f46924ddfd103ee9510bf254f9a3d67` |
| Capture launch manifest |`16b665aa7a382f99be12e9972eb4d38f008fb23272650fc0b09e394b091f285b` |
| Sampler source |`31f1432120e6b7086d9fd276a972ae2b50edd1d469108347d03c599db779e5d7` |
| Mine catalog plan |`fdd8699522044346139d226663e364823666c76c0a70cee2c3327c4d7a976cec` |
| Female starter skin rig |`58f2e013f5c5dc3724ea3e22c41f4ea425f457c3530d3568502452e33d378bff` |

Root observed `Post-cleanup alive120frames`, a responsive original tutorial, then selected QuitGame and Yes. The original exited; its sidecar reports −9/signal 9 at `2026-10-05T13:18:30.436471+00:00`, consistent with the verified shipped self-kill quit handler. The pre-quit log is preserved as `female-mine-10.player.log`. This is not an exit-code-zero claim, and the analyzer still correctly leaves process-teardown validation false because external lifecycle evidence is not encoded by a successful JSONL end record. See [sampler lifecycle history](ANIMATOR_BAKE.md).

[export_reference_pickaxe.py](../../rebuild/tools/export_reference_pickaxe.py) validates item19's database resource and serialized action/mount identity, compares installed/source visual transforms, and writes the exact original mesh with diffuse texture and source hashes. [export_baked_pickaxe.py](../../rebuild/tools/export_baked_pickaxe.py) attaches that mesh to the captured Main or Back bone matrix with identity rigid inverse bind. Both exporters require fresh output destinations; original inputs are read-only. Static geometry is `rebuild/build/reference-assets/wood-pickaxe`. The four runtime groups are:

| Runtime directory under reference-assets | Evidence/contents |
|---|---|
| `player-pickaxe` |One 216-vertex part, held mountMain, existing run02ten-clip body clocks/345 poses |
| `player-pickaxe-stowed` |Same exact tool, mountBack, same ten clocks/345 poses |
| `player-mine` |Nine body parts / 5,152 vertices, run10Mine1–3/91 poses |
| `player-pickaxe-mine` |One 216-vertex held part, identical Mine1–3 clocks/91 poses |

All use PEAC v2's exact authored terminal timestamp. Actual native loading and every-frame CPU skinning of all four groups passed AddressSanitizer/UndefinedBehaviorSanitizer. Combined Mine body bounds are approximately(−.317,−.006,−.194) to(.285,1.753,.967)m; the held Mine tool bounds are(−.417,.364,−.348) to(.289,2.095,1.509)m. Seven exporter tests pass with actual optional assets present: identity/mount rejection, corrupted geometry, incomplete capture, clock mismatch, existing-destination preservation, synthetic native pose checks, real source hashes and byte-identical repeat export of the locomotion tool. These prove data/pose consistency, not graphical appearance or timed gameplay success.

The completed run used the following sequence. On repetition, select a fresh capture filename and fresh export destinations; the shown capture already exists. Only the designated operator performs `--launch`, arm-file creation and original-game interaction.

```sh
/tmp/pe-asset-tools/bin/python rebuild/tools/inventory_reference_bake.py \
  --output rebuild/build/animator-bake/mine-clip-plan.json
TMPDIR="$PWD/rebuild/build/compiler-tmp" python3 rebuild/tools/reference_animator_bake.py \
  --plan rebuild/build/animator-bake/mine-clip-plan.json \
  --clip Mine1 --clip Mine2 --clip Mine3
# Operator-only launch: same command with --launch and a fresh --output path.
/tmp/pe-asset-tools/bin/python rebuild/tools/export_reference_pickaxe.py
python3 rebuild/tools/analyze_reference_bake.py \
  "$HOME/.local/share/planet-explorers-bake-reference/pose-captures/female-mine-10.jsonl" \
  --rig rebuild/build/reference-player-starter/player-female-starter-rig.json \
  --report rebuild/build/animator-bake/female-mine-10-report.json \
  --export-dir rebuild/build/animator-bake/player-female-mine
python3 rebuild/tools/export_baked_pickaxe.py \
  "$HOME/.local/share/planet-explorers-bake-reference/pose-captures/female-mine-10.jsonl" \
  --rig rebuild/build/reference-player-starter/player-female-starter-rig.json \
  --visual-assets rebuild/build/reference-assets/wood-pickaxe \
  --body-scene rebuild/build/animator-bake/player-female-mine/all/recording.peas \
  --output rebuild/build/pickaxe-staging/player-pickaxe-mine
```

For held/stowed locomotion, use the same attachment command with `female-clips-02.jsonl`, `--body-scene rebuild/build/reference-assets/player/recording.peas`, `--mode drawn` or `--mode stowed`, and distinct fresh staging outputs. Preserve `player-female-mine/all` as the body research export. Publish all four complete groups together into their runtime directories while no frontend is reading them; a partial action group fails loading before GPU upload. The existing run02body/tool assets and other equipment groups are unchanged. The subsequent root-owned physical check passed one Mine strike/five wear/two dirt, with exact instance retention, close presentation inspection and pre-impact panel cancellation. See the [playtest record](../PLAYTEST.md) for those C observations, which remain distinct from isolated original pose sampling.
