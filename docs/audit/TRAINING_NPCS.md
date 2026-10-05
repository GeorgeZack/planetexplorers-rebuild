# Tutorial NPC roster, placement and interaction

The tutorial has **nine authored NPC spawns**, not seven. `NpcEntityCreator.CreateTutorialLineNpc` reads every `npc` row whose `training_pos` differs from `0,0,0`. `SceneEntityCreator.InitTutorialNpc` waits for the main player, then invokes that creation path without a later mission/visibility gate. The seven IDs in `TrainingTaskManager.SetNPCsAtrribType` are a separate CampID/DamageID reset list; they are not the complete spawn roster.

This audit is source-backed. The original native tutorial has visually confirmed Andy and other people, but no complete nine-person runtime survey has been performed. Static identity/placement and sampled NPC animation should not be confused with porting their AI, optional conversations or the original NPC interface.

## Authored roster

Coordinates are original Unity model-root world coordinates, not positions relative to the room import. All are created at unit scale and identity rotation. Andy's initial mission739 accepts Plot470, which changes his yaw to275 degrees about Unity +Y; this is the tutorial-visible orientation used by the C metadata. Other entries retain initial yaw0 until their behavior/conversation code changes them.

| Entity | Prototype | Name | training_pos | Tutorial yaw | Behavior record |
| --- | ---: | --- | --- | ---: | --- |
|9004|4|Adisa Mostafa|(27,1.8,17.5)|0|Npc_stand|
|9005|5|Moric Gahn|(32,1.8,6.5)|0|Npc_patrol|
|9006|6|Vera Novikova|(58,2,12.5)|0|Npc_campfear|
|9009|9|Allen Carter|(10,1.8,3.5)|0|Npc_doctor|
|9020|20|Anjum Nahas|(2.5,1.8,6)|0|Npc_patrol|
|9031|31|Viola Strand|(-27,10,20.5)|0|Npc_patrol|
|9041|41|Andy|(32,1.8,12)|275|Npc_stand|
|9047|47|Percy Malik|(27.5,9.2,2)|0|Npc_com+repair|
|9049|49|Malus Tepes|(37,2.5,5.5)|0|Npc_stand|

Behavior paths use `AiPrefab/EntityData/Npc/` as their prefix. Merely loading these metadata strings does not execute the behavior trees; fixed idle presentation is an explicitly limited first implementation. No source evidence supports hiding the outer-room NPCs until a later lesson or moving all nine near Andy.

`TrainingTaskManager.cs:79–91` attempts to set CampID and DamageID to0 for9004,9005,9006,9009,9020,9031,9041. It skips missing entities and does not include Percy or Malus. This once-at-Start call has no explicit creation wait; exact runtime ordering has not been observed. Do not expand it to all nine or infer blanket invulnerability from it.

## Dedicated appearance and equipment

`PrototypeNPC` maps each identity to its own `Npc/<name>.unity3d` bundle: `adisa_mostafa`, `moric_gahn`, `vera_novikova`, `allen_carter`, `anjum_nahas`, `viola_strand`, `andy`, `percy_malik`, `malus_tepes`. The fallback avatar-model paths are also retained in C metadata, including source object-name spelling. `PeEntityCreator.CreateNpc` sets the dedicated model path, sex, behavior, attributes, identity and battle configuration; it does not call the player customization routine used by `CreatePlayer`.

The asset audit recovers each NPC’s dedicated clothed skinned mesh, diffuse material slots and distinct Avatar. This establishes their source appearance assets, not full Unity shader/equipment parity. All nine exports now exist under `rebuild/build/reference-assets/npcs/{entity_id}/model.obj` with `rig.json`, original clothed materials,36–41 skin bones and3,876–8,108 vertices per NPC. These OBJ files are serialized rest poses, not recovered idle animation. Malus has duplicate sibling `Mouth` transforms; canonical paths use `#0/#1` only for duplicate names, preserving his actual skin binding to `Mouth#0`. Four exporter tests, eleven bake-analysis tests and native strict OBJ validation pass for all nine. The original player female bone bake cannot simply be applied to these different Avatars. Each NPC’s own humanoid `NormalIdle` has now been evaluated on its original Avatar in the isolated native-player sampler. The static OBJ remains rest-pose research output; the installed `recording.peas` scenes contain the resulting animated idle. Imported bundles and generated local outputs remain separate from the checked-in C source.

`CreateTutorialLineNpc` also applies each row's money, weapon/items and specialties. Eight rows specify weapon0/item0. Percy9047 has weapon54 and item50×500. This is a source equipment/package requirement, not authorization to show an arbitrary rifle mesh; rendering should use the recovered actual equipment or clearly state that this attachment is omitted. Armor columns on these `npc` records are0, but their dedicated mesh materials already encode individual clothing; zero is not a request to render generic undressed player bodies.

## Collision and settlement

All nine source model-root capsules are enabled, nontrigger, vertical and on layer10. Radius is.370000005m. Height1.75m and centerY.875m apply to eight; Anjum uses height1.975000024m and centerY.949999988m, leaving his capsule bottom.037500024m below the model root. The physical player capsule also uses layer10, and the source project collision matrix disables layer10 versus layer10. Layer8 is AIPlayer and its pair with layer10 is enabled; the earlier layer8 explanation was incorrect. `BiologyViewCmpt.ResetOutCollider` separately ignores the generated outer capsules against NPC/player physical capsules. Adding solid NPC blockers to player movement would change the inspected source behavior.

[pe_training_npc.c](../../rebuild/src/pe_training_npc.c) exposes immutable metadata, lookup by entity ID and `pe_training_npc_ground`. This helper is a **new bounded C approximation of initial gravity settlement**, not a change to authored coordinates or a claim of original physics parity. It converts model root to capsule feet using centerY−height/2, tries the authored position followed by.25m upward clearances up to3m, then sweeps downward up to5m for walkable support. It preserves x/z and yaw, adds.003m skin, and reconstructs the model root. A failure leaves output unchanged. The limited upward search is needed because Andy and Moric's authored capsules initially overlap the recovered room mesh slightly.

The actual recovered room test settles model-root Y to:

| NPC | Settled root Y |
| --- | ---: |
|Adisa|1.474127|
|Moric|1.855870|
|Vera|1.891949|
|Allen|1.500408|
|Anjum|1.535432|
|Viola|9.939374|
|Andy|1.823208|
|Percy|8.346012|
|Malus|1.870197|

These are reproducible C fixture outputs, not measured original NPC positions. The tests verify every settled capsule is clear, original x/z remain unchanged, Anjum's offset is handled, an initially embedded source root can settle, invalid coordinates and unsupported falls fail without mutation, and IDs/neutralization/yaw metadata match the source. Strict C11, ASan, UBSan and leak checks pass against all nine recovered-room placements.

## Conversation and lesson distinction

`MousePickableNPC.OnStart` uses `GameConfig.NPCControlDistance`, which is **4.5m**, for general interaction. It is neither the kit's10m threshold nor the mission return-area radius. Mission742 target4072 remains an independent inclusive3m region around `(32,4,12)`; Andy's actual model-root `(32,1.8,12)` must not replace that objective point. Source mission742 still requires reaching the area and explicitly selecting the NPC reply, not merely approaching a rendered model.

The source optional conversation missions are:

| NPC | Mission IDs | TalkOP scenario IDs |
| --- | --- | --- |
|Percy|724,725|3459;3460|
|Malus|726,727|3463;3464|
|Andy|728|3109|
|Adisa|729|3111|
|Allen|730,731|3114;3115|
|Moric|732,733|3118;3119|
|Anjum|734|3121,3122,3123|
|Vera|735|3125|
|Viola|736|3127,3128,3129|

These records carry `AfterLimit=2:737`; Andy728 additionally has `PreLimit=1:739`. Their raw mission gates are recorded without inventing a new availability interpretation. `NPCMission` associates each with the real entity ID and also contains general campaign missions; those campaign records should not be indiscriminately exposed in the tutorial. `UINpcWnd.UpdateMission` explicitly disables shops in tutorial mode. The new visible roster does not itself implement these side conversations, shops, specialties, patrol/repair/doctor behavior or per-NPC save state.

## Main source references

- `Assets/Scripts/Scene/SceneEntityCreator.cs:140–149`: wait for main player and create tutorial NPCs.
- `Assets/Scripts/Scene/NpcEntityCreator.cs:185–224`: exact roster filter, root transform and item/mission initialization.
- `Assets/PeWorld/PeEntityCreator.cs:1702–1750`: dedicated NPC model and identity initialization; `NpcProtoDb` maps `Bonepath`, `Bonepath_Editor` and `path` independently.
- `Assets/Scripts/Jaluca/TrainingField/TrainingTaskManager.cs:79–101`: seven-NPC attribute override; `MissionStart` opens739.
- `Assets/Scripts/Mission/StroyManager.cs:1725–1750`: NPCface angle applies world +Y rotation.
- `Assets/Resources/DataBase/localData.bytes`: `npc`, `PrototypeNPC`, `NPCMission`, `Quest_List739` and `Plot470`.
- `Assets/Scripts/MousePicker/MousePickableNPC.cs:12–17` and `Assets/Scripts/System/GameConfig.cs:199`: general4.5m interaction range.


## Asset generation and current frontend

The optional exporter requires UnityPy and a fresh destination. On a clean checkout:

```sh
/tmp/pe-asset-tools/bin/python rebuild/tools/export_training_npcs.py
```

Existing outputs must be preserved by choosing a new directory. This repeatable sequence rebuilds source assets and converts the already completed local capture into another fresh directory:

```sh
/tmp/pe-asset-tools/bin/python rebuild/tools/export_training_npcs.py \
  --output rebuild/build/npc-assets-regenerated-01
python3 rebuild/tools/analyze_training_npcs.py \
  "$HOME/.local/share/planet-explorers-npc-bake-reference/pose-captures/npcs-idle-01.jsonl" \
  --assets rebuild/build/npc-assets-regenerated-01 \
  --output rebuild/build/npc-scenes-regenerated-01
for id in 9004 9005 9006 9009 9020 9031 9041 9047 9049; do
  mkdir -p "rebuild/build/reference-assets/npcs/$id"
  cp -a "rebuild/build/npc-assets-regenerated-01/$id/." "rebuild/build/reference-assets/npcs/$id/"
  cp -a "rebuild/build/npc-scenes-regenerated-01/$id/." "rebuild/build/reference-assets/npcs/$id/"
done
```

Use another unused suffix for subsequent regenerations. These copy only generated models, rigs, textures, sampled actors and evidence; they do not modify source assets or the original installation. The first command emits `npc-plan.json` with source bundle hashes and exact effective idle identities. A new original-player capture requires the separately documented [isolated NPC sampler](NPC_IDLE_SAMPLING.md), an unused capture basename and explicit launch; conversion alone does not launch Unity. The `capture_base` above names the nine `.npc-ID.jsonl` files and their individual `.launch.json` provenance, rather than a single combined JSONL file.

The compiled NPC frontend loads each `npcs/{entity_id}/recording.peas` as a complete nine-character group, requiring its looping `NormalIdle`. An absent complete group leaves optional NPC visuals disabled; a partial or invalid supplied group fails loading. It currently does **not** render the rest-pose OBJ fallback. All nine completed idle captures have been validated and installed. The1750-frame native graphical run passed with exit0; every NPC gallery screenshot was visually inspected with intact appearance and idle deformation. The frontend renders these sampled scenes, not the rest-pose OBJ fallback. The view retains source identity/yaw and grounded root positions, advances idle time at simulation steps, renders NPCs independently of the current lesson and uses actual Andy capsule bounds for return-lesson mouse picking. Names are hidden behind room walls or outside the display distance. NPC bodies do not block player movement, consistent with the source collision-layer exclusion. Side conversations and AI behavior are not added by this visual integration.


## Completed idle capture and headless validation

Session `npcs-idle-01` contains **606 authored poses**: three female NPCs each62frames over2.00001526s, and six male NPCs each70frames over2.26666689s. The female controller’s direct `NormalIdle` and male override’s distinct `NormalIdle` were resolved from each original bundle; all127 humanoid bindings are evaluated by the installed Unity Animator on that NPC’s own Avatar. No female-player matrices or fabricated idle motion are substituted.

Strict analysis checks closed successful end records, individual launch/source clip identity, exact Avatar/gender, monotonic sample timing, finite/invertible matrices, parent/world consistency and every skin-bone mapping. All36–41 skin bones vary on every NPC; no required bones are missing. Malus’s duplicate mouth sibling maps to the correct canonical `#0` skin path. Maximum hierarchy error across all nine is1.55×10⁻⁶ in matrix components. This is sampled clip-only evaluation; it does not prove general native humanoid retargeting, controller transitions, AI or procedural IK parity.

The installed scenes contain **12 material parts**: Vera, Allen and Andy each use two, the others one. A headless native PEAS/PEAC loader and CPU-skinning harness evaluated every exported frame of every part with AddressSanitizer, UndefinedBehaviorSanitizer and leak checking. All passed. Across the full idles, maximum model-local Y bounds are approximately1.75–1.96m and maximum per-vertex displacement from the initial pose ranges9.6–57.3mm. Small negative local foot positions (approximately1–26mm depending on NPC) are original sampled skin geometry and are not silently offset by the exporter. Grounding remains the separately described capsule approximation.

All nine runtime scenes are installed at `rebuild/build/reference-assets/npcs/{id}/recording.peas`. Research `rig.json`, `model.obj` and `model.mtl` were verified byte-identical before and after this installation. Analysis and per-part provenance accompany the actors. A separate check matched all nine current original bundle SHA-256 values against both export manifests and immutable launch plans, and matched captured Avatar names and signed clip IDs to those source records. Native headless evidence is retained locally in `rebuild/build/npc-bake/idle-01-native-validation.txt`; four source-asset tests, two batch-conversion tests and eleven shared bake-analysis tests pass, including incomplete-capture publication rejection and signed64-bit bundle clip IDs.

The preserved original log records nine successful completion messages and nine retained inactive graphs. The parent observed a responsive tutorial screenshot after capture. This session’s preserved logs do **not** contain the120-update watchdog marker, so no measured120-update survival claim is made for this run. Ordinary window-close was requested; `.exit.json` records−9/signal9 at2026-10-04T00:59:12UTC, consistent with the separately verified original `PEMutex` shutdown self-kill. This is not an exit-code0 assertion. The subsequent C1750-frame graphical run passed with exit0, mission746, kit transferred, Andy reached, nine juices and knife granted/equipped; idle time reached29.167s. All nine retained gallery screenshots are `docs/reference/rebuilt-training-npc-ID.png`, with the run result in `docs/reference/rebuilt-training-npcs-result.txt`. Every character was visually inspected; a foreground railing occludes Malus’s lower body in his gallery image, while native posed bounds remain intact. Core, Python,18CLI, ASan and UBSan checks also passed at this milestone. This verifies the visible sampled-idle integration, not NPC AI or optional side conversations.
