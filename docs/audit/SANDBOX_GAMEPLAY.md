# Sandbox gameplay alignment audit

Audit: 2026-10-03. Scope: ordinary C sandbox `rebuild/src/pe_game.c/h`, generated content and saves, compared with Unity source. No implementation or runtime testing was performed for this audit. The more developed `--training-room` is a separate host. Root owns live Adventure observation; the separately attributed starter-state observations below were subsequently supplied by that session. [GAMEPLAY.md](GAMEPLAY.md) describes the broader source systems.

The sandbox conserves resources through a playable gather/craft/dig/build loop and imports original item/recipe IDs, selected terrain arithmetic and worn tool instances. Its largest substitutions are procedural wood gathering, immediate tool actions, unrestricted recipes, shared inventory and stamina-only player attributes. Resource mutation, stamina charge, durability wear, animation event, cancellation and recovery have separate source boundaries.

## Current implementation boundary

| Area | C implementation | Alignment / gap |
| --- | --- | --- |
| Movement | Walk2/run5/sprint8; sprint starts15, drains5/s; jump costs10 | Verified normal movement constants; collision/acceleration/ballistics provisional |
| Stamina | Max100/recovery40/minimum0.1s rest | Source defaults; tools use temporary cooldown; jump never times out airborne |
| Gathering | Twelve generated trees,8wood/255HP each; bare hands1wood/0.35s | Explicit bootstrap, not source plants/tree production |
| Cutting | Wood Axe CutDamage46; temporary0.5s swing | Item damage exists; prototype/size/hardness/yield/event timing missing |
| Mining | Tool required, source indicator, atomic damage/wear/output | Selected-sample arithmetic aligns; prefab overrides/action scheduler missing |
| Equipment | One tool ID referencing a durable bag instance | Identity/wear works; slots, held states, weapons/armor/components absent |
| Crafting | Eight recipes, one job, batch input, strict completion-triggered delivery after5s | Source data/delivery; all unlocked, safe capacity/input policies differ |
| Vitals/progression | Stamina and diagnostic counters only | No sandbox HP/death/food/oxygen, missions, learned recipes or mode lifecycle |
| Save | PECSv3 world/possessions/wear/RNG/pending craft | Rebuild format, no Unity archive compatibility |

C anchors at audit time: init`:182`, step`:201`, equipment/stat accessors`:63–88`, crafting`:90–149`, mining`:453–503`, gathering`:505–535`. Function names are the durable references.

## Mining

Source: `Assets/Scripts/PeEntity/Cmpt/Motion/PEAction/Action_DigTerrain.cs`; serialized tool and animation evidence: [TRAINING_MINING.md](TRAINING_MINING.md). Source and installed Wood Shovel12 prefab checks prove stamina **4**, action skill20110001, Spade bool/speed1, mountMain/mountBack, durability3553 and wear5. Generic `export_content.py:89–95,194–204,233` still reads class initializer5 and exports stamina5 for all selected dig/cut tools. Its provenance explicitly says prefab overrides are undecoded. Its database item skill_id is not the component's action skill. Verify each pickaxe/axe independently; do not change them all to4 from the shovel evidence.

Wood Shovel damage: baseAtk10+item23=33; `SkEntityVoxelTerrain.cs` uses attack×(1+ResDamage), then resource-returning multiplier5, yielding165. ResRange0.49/ResDamage0 selects one sample. `BuildNDig/DigTerrainManager.cs:188–217` applies duration, hardness `37.14689*d*d-14.12429*d+1.39`, damage normalization1275, attenuation threshold0.33 and minimum damage2.5; density≤127 disappears. C preserves this selected-sample arithmetic. Generated stone maps to legacy17, other generated terrain to2; imported source1 preserves raw material IDs.

`pe_world_dig_target` reproduces centered voxel boxes, thresholds1,11,…131, solid axial support and shifted-origin fallback. Reach is strictly **feet+up1 to snapped sample <3m**, not camera3m. A source empty fallback can still receive an attempted swing's wear/stamina; C rejects it without costs. C also removes placed cells/furniture before mining with the same LMB, a convenience rather than proof of source action equivalence.

Source charges4 at action start and locks the first target. ChangeTarget/DigTerrain events control subsequent targeting and mutation. Continuing hits charge the next stamina; ending hits do not. Recovery remains locked through the ending tail. Low stamina is allowed, with post-charge0.9 speed at≤40 and another0.9 at≤20. C charges after mutation and applies `0.5/speed` cooldown; this does not reproduce female Spade2 hit0.6035147905 or male0.6481015086. Existing PeTrainingDig can inform a reusable host, but its blend/absolute event dispatch timing is still explicitly approximate.

`NaturalRes.cs` and `DigTerrainManager.GetResouce:402–498` group removed samples by material, choose positive fixed_num or self_num+bonus, multiply by removed count, stochastically round, sample cumulative0..100 primary drops, then ordinary/special extras. C exports primary byte-material drops only and uses its own deterministic RNG; no Unity seed equivalence. Known no-drop materials correctly remove without output. Unknown definitions reject C mining; source logs missing hardness and proceeds with available damage, so this is a bounded-content restriction.

Acceptance before replacing mining:

- Separate start/locked target/hit/repeat/release/DigEnd/cancel. Held input must not charge per rendered frame.
- Material1 duration0.21167 and damage165 remove densities255/210/190/130/110 in4/3/2/1/1 hits, with two dirt per removal only. Test partial edits, empty fallback costs and separate shovel wear.
- Reject reach equality3m; allow distant camera/near body; exercise supported low-density samples.
- Test charge4, thresholds40/20, release-tail recovery,60/120Hz presentation and invalid-input atomicity.
- Keep capacity conservation explicit: refusing output must not destroy resources. Specify wear/stamina policy for a refused opportunity instead of claiming repaired failure paths are identical to Unity.

## Gathering and ordinary trees

`Action_Gather.cs` uses skill20110003 and3m subterrain picking. It selects NaturalRes type10 and, with unlocked cursor, intersects mouse rays with prototype bounds. Gather/GatherEnd animation events apply resources/end the action. It is not repeated loose wood from any tree; its redundant self-distance comparison does not negate the3m picking query.

`DigTerrainManager.Fell:877–882` looks up resource `tree.prototype+1000` and subtracts `cut_damage*resource.duration/(height_scale*width_scale)`. `GetTreeResouce:902–980` uses positive fixed_num, otherwise `self_num*width_scale²*height_scale*(1+bonus)`, minimum1 before integer production loops, plus extras. Size affects cutting time and yield; removal updates LSubTerrain/RSubTerrain. C has no prototype/scale and uses direct46 damage/remaining8wood. The training hologram's seven hits/one wood are scripted and must not replace ordinary trees.

Next fixture: one source tree prototype/resource/scale/identity and one gatherable plant. Compare small/large tree damage/yield, instance depletion, targeting, event collection and saved removal without respawn. Keep a labeled bootstrap option until Adventure starting stock/known recipes/acquisition are observed; removing loose wood alone could strand the empty-handed sandbox spawn.

## Inventory and equipment

C27 items use original IDs/stack limits; selected resource stacks9999, tools stable instances. C32 shared slots differ from source `ItemPackage.cs:383` Item/Equipment/Resource/Armor tabs and CodeIndex`:746` tab+slot encoding; `PlayerPackageCmpt.cs:83` creates27 shortcuts.

`EquipmentCmpt.cs:263,325` resolves intersecting equipment masks, removal/capacity permission and attribute/components. Verified knife33/axe1527 share bit16; shovel12 position528 intersects16. C retains one tool reference in the bag, without general source transfers, held/drawn distinction, animations, gender/skill gates, armor, repair, energy or strengthening. Craftable Wood/Stone Swords1/2 have PE_TOOL_NONE and are rejected by generic equip: unfinished outputs, not sandbox combat.

Acceptance: conserved ownership, full-bag refusal preserving old equipment, mask conflicts, exact stat add/remove, broken-tool gating, wear on swapping, stable save IDs and unique equipped references. Category migration must not orphan existing32-slot saves; transient held actions need a separate restore policy.

## Crafting and progression

All eight selected formulas output one:

| Formula | Output | Inputs | Seconds |
| ---: | --- | --- | ---: |
|1|Wood Shovel12|Wood279×10|3|
|7|Wood Pickaxe19|Wood279×10|3|
|8|Stone Pickaxe20|Stone280×6, Wood279×6|3|
|19|Wood Sword1|Wood279×10|3|
|20|Stone Sword2|Stone280×12|3|
|58|Wood Ladder310|Wood279×2|5|
|152|Wood Fence308|Wood279×2|5|
|13|Wood Axe26|Wood279×10|3|

`ReplicatorCmpt.cs:71–114` loads synthesis_skill; MaxProductCount`:274` limits batches by materials/9999. StartReplicate`:450`, UpdateReplicate`:484–531`, serialization`:533` define one job. Batch completion consumes input and accumulates output; delivery occurs at completion when **since last delivery>5s**, or final batch. C aligns: multi-batch3s first delivers two at6s, not5s. Product quantity differs from batches for future output_count>1.

Declared C repairs: start checks capacity after hypothetical consumption; capacity block retains completion time whereas source resets elapsed0; missing inputs pause rather than source delivery/end; cancellation first requires pending delivery and preserves unfinished inputs. Do not restore legacy loss paths merely for resemblance.

C exposes every formula. Source KnownFormula/AddFormula`:244`, flags/list/serialization and item Formula books are real progression. Skill crafting-time rules depend on `RandomMapConfig.useSkillTree`; record that setting. The selected eight recipes are not established as a fresh Adventure known list.

Acceptance: start/finish/cancel/save at2.9/3/5/6s; three3s batches deliver2 at6s/1 at9s;5s batches retain strict>5 intermediate rule; filled output/stolen inputs obey declared safe policy. Test unknown/duplicate recipe learning and book consumption only on successful new learning. Learned IDs require versioned persistence beyond running-job state.

## Vitals, movement and mode lifecycle

`PESkEntity.cs:8–128` defines HP/stamina/comfort/oxygen/hunger/energy, modifiers/injury/progression; current/max clamps at`:120–160`. Sandbox only stores stamina, no HP. Training HP500/Herbal Juice/melee do not make these available here. Survival rates require active update/buff tracing; no hunger/oxygen drains are invented here.

initProp1 provides stamina100/recovery40/delay0/reduction1, Atk10/ResRange0.49/ResDamage0/CutDamage0. `SkExpFuncSet.cs:116` recovers only when scaled Time.time is strictly later than last consumption+max(delay,0.1). Serialized normal walk2/run5/sprint8/start15/drain5 supersede class-initializer assumptions. Jump cost10 aligns; C velocity7/gravity20 and instant acceleration/step-up are provisional.

Concrete mismatch: `Action_HumanMove.cs:784–943` ends Jump after elapsed **>3s**, even airborne, or grounded/water after initial delay. C jumping clears only on landing, blocking regeneration through a tall fall. Training already has an action-age model/regression. Test airborne beyond3.1s: recovery resumes after the last active timestamp+delay without forcing landing or cancelling vertical velocity.

`GameTime.cs:11–13` sets normal speed12, Story day94.35 and Build94.50. C hardcodes94.35/12x with26-hour day: source Story choice, not verified Adventure initialization. Spawn/package/known recipes/skill settings/clock need mode-specific evidence. Counters are diagnostics, not progression. Crop import preserving possessions is inspection, not general world travel. Adventure and Build must not be equated; ambient spawning/lifecycle differ.

## Work order and live-session worksheet

1. Export effective tool prefab overrides, keeping item skill versus action skill distinct; verify shovel4/20110001 and other tools independently.
2. Generalize audited dig action state without mission751/material1-only/grant9000 logic. Keep blend uncertainty. Add tall-fall action timeout independently.
3. Import one ordinary tree/plant resource loop; retain explicit bootstrap until Adventure starting state is observed.
4. Add mode package/known formulas/category capacity/equipment masks with conserved versioned saves.
5. Resolve life/death/healing and survival update chains, then general combat eligibility/action/contact rather than tutorial whitelist.
6. Expand tools/content/world persistence; multi-sample removals group by material before yield and never duplicate on reload.

For root's Adventure observation: record mode/difficulty/skill-tree setting and save identity; starting bag/equipment/known formulas; equipped versus drawn; shovel start/release at high/low stamina; counts/durability after partial versus removed terrain; ordinary plant/tree acquisition; batch delivery/cancellation; sprint exhaustion/recovery. Screenshots do not establish raw density, hidden IDs or exact callbacks. Log uncertainty. Tutorial grants/treeHP7/mound are not Adventure rules.

Existing PASS evidence remains in earlier audits/PLAYTEST. These acceptance cases are a work order, not newly executed C tests or broad Adventure parity claims.


## Observed Adventure start and source package

Root subsequently launched the installed original in **Adventure / Normal**, seed Planet Maria, Grassland Dry, height128m, world2km, three AIs, SkillTreeOFF and AllScriptsOFF. Before any supply-quest reward, the bag showed ten red juice, ten revival boxes, four apparent scripts, five equipment entries and400 meat/200wood. The Replicator was empty before right-clicking any of those items. This is an original-runtime observation supplied by the desktop operator, not an action performed by this audit agent. [Starting resource screenshot](../reference/sandbox/original-starting-resources.png).

The visual identities resolve exactly through `Assets/PeWorld/SingleGameInitData.cs:12–28,59–67,256–260`: a new Adventure game adds item1289, automatically calls Use on that outer box, then sets money500. `Assets/PeLauncher/PeGameLoader.cs:977–988` invokes this only in the new-game path; Restore is empty and must not regrant the package. PrototypeItem1289 has fixed bundle:

```
0;33,1,1527,1,73,1,70,1,973,1,974,1,975,1,1579,1,916,10,228,1,937,10,229,400,279,200
```

| ID | Source English item | Count | Meaning |
| ---: | --- | ---: | --- |
|33|Knife|1|Equipment|
|1527|Safety Axe|1|Equipment; not craftable Wood Axe26|
|73|Headlamp|1|The light icon is not a separately inferred flashlight|
|70|Water Pitcher|1|Blue equipment icon|
|228|Battery Pack Type01|1|Root observed full charge; source supplies the instance|
|916|Herbal Juice|10|Consumable|
|937|Revival Shot|10|White/blue consumable box|
|973|Scanner App Update|1|Metal IDs2,3|
|974|Scanner App Update|1|Metal IDs4,6,8|
|975|Scanner App Update|1|Metal IDs1,5,7|
|1579|Scanner App Update|1|Metal IDs9,10,11|
|229|Meat|400|Resource tab|
|279|Wood|200|Resource tab|

The four scanner updates are **not crafting recipe books**. Their type27/MetalScan component interprets the shared database `learn` column as metal IDs (`ItemObj.cs:2007`, `UseItemCmpt.LearnMetalScan`), not formula IDs. Root's right-click opened the Scan app without populating the Replicator, consistent with the source. The outer1289 box has learn0, and Replicator's constructor creates an empty KnownFormula list. Thus the inspected new-game path supplies **zero known crafting formulas** with AllScriptsOFF, matching the initial live empty panel. Other later interactions can learn formulas; do not equate this initial state with a permanent restriction.

With AllScriptsON, SingleGameInitData additionally adds and auto-uses1743, whose `learn` field contains the explicit large formula list. Do not use that list for OFF. Default clothes are initialized separately from `[113,149,210,95,131,192]` and sex-filtered: male113/149/210 or female95/131/192 (issued overalls/pants/shoes). These are equipped directly, not six extra loose bag entries. Story instead opens1358; Build uses1291 (1725 in DemoVersion). Adventure's package is therefore a better source baseline than the C empty-handed bootstrap, but it cannot be copied piecemeal while omitting recipe acquisition and claiming full mode parity.

## Early supplies and first craftable tools

`MissionManager.AdventureInit` at `Assets/Scripts/Mission/MissionScript/MissionManager.cs:3552–3570` starts9027 dialogue on the generated start NPC. Database AdRandomQuest_List chains9027→9028→9029→9030→9031. **9031 Arrival** has no item reward or acceptance grant. Its target4502 asks to talk to generated `AdvNPC002`, displayed as Eitai in this observed world; this is a generated identity, not a universal hardcoded Eitai spawn. Description asks that NPC for supplies. AdQuestChain31 requires9031 and begins with9032 at weight100.

**9032 The Delayed Supplies** provides the practical starter tools/books on acceptance (`Get_MissionItem`), not on completing its trip to Town004. TalkOP4090–4094 explains the delayed scripts and offers a starter pack. Source grants one of each:

| Direct equipment / object | Recipe books and learned formula IDs |
| --- | --- |
|Wood Shovel12, Wood Pickaxe19, Wood Axe26, Wood Sword1, Wood Shield34, Wood Bow43, Sleeping Bag1550|552 Herbal Juice→147;512 Arrow→107;519 Revival Shot→114,42;1551 Sleeping Bag→522,192;406 Wood Shovel→1;412 Wood Pickaxe→7;418 Wood Axe→13;424 Wood Sword→19;433 Wood Shield→28;510 Bullet→105,57,296,200;612 Wood Bow→207,272|

These books are granted as items; receiving them does not itself mean all formulas have been learned. `UseItemCmpt.cs:98–133` calls AddFormula on use and only reports new learning when at least one ID was newly added. The current sandbox's eight universally available recipes skip both this initial empty state and the explicit supply/books progression. A faithful minimal Adventure loop can start with the verified package and reproduce this supply acceptance/learning path; it does not require invented bare-hand tree wood.

This section resolves source grants and root-observed startup only. The subsequent [live alignment report](SANDBOX_ALIGNMENT.md) separately records actual9032 acceptance, shovel learning/crafting/equipment, excavation, construction and Adventure save/reload, plus a Single Player Build comparison. Later quest outcomes remain untested. Source text says later scripts come from jobs and exploration, but that dialogue alone is not implementation proof for every reward source.

Read-only database provenance: `Assets/Resources/DataBase/localData.bytes`, SHA256 `c3d63ee865a7e10e309e11e9ee561c93feef39c379a584dcf68722a523cd4376`; starter source SHA256 `0ae63830550b059bf94085e55639af54a2c6525c882aa4f78287ce4deb931fe1`. Tables queried: PrototypeItem, Translation, AdRandomQuest_List, AdQuestChain, AdQuest_EnterArea, Scenario.
