# Tutorial, player movement, and reference fixtures

Audited 2026-10-03 against the supplied source and databases. This document separates source evidence, serialized asset values, observed original-player behavior, and the current C implementation. The original desktop is controlled by the parent investigation; this audit did not launch a game or send desktop input. See [REFERENCE_RUNTIME.md](../REFERENCE_RUNTIME.md) for launch configuration and screenshots.

## Observed reference behavior

The installed Linux V1.1.4 / Unity 5.2.4f1 player reaches the main menu and Andy's simulation-deck tutorial. The parent investigator's `02-controls-intro.png` shows a camera mission requesting F5 to enter Control Mode, then F5 again for First Person Mode; the tip describes the default third-person camera and right-mouse camera drag. The parent subsequently completed camera task 739 by changing both modes and basic movement task 747 with W, Space, Shift+W, and middle mouse; captures `11-firstperson` and `15-move-done` under the investigation's `/tmp/pe-play` record these steps. The parent subsequently observed storage transfer, the Andy reply, the additional five-juice inventory grant, quick-bar assignment, and one juice use: the original showed nine remaining, a cooldown overlay, and Andy's “Confirmed.” dialogue (4074), entering the knife lesson. In that observed original layout the juice occupied numbered slot 2; the C subset uses slot 1. These observations agree with the source and localization below. Subsequent original-client observations confirm the knife grant, equipment and drawing, plus the Lepus Hare combat prompt. A successful hit or kill, measured movement speeds, jump trajectory, and exact stamina depletion have **not** been established by these observations. Do not treat a recovered default as a measured shipped-runtime result. Temporary screenshot paths are investigation artifacts, not build dependencies; the parent playtest document owns the retained evidence.

## Tutorial entry and mission graph

Primary sources:

- `Assets/Scripts/Jaluca/TrainingField/TrainingTaskManager.cs`: `MissionStart` waits for the main player, opens mission 739 dialogue, and dispatches task initialization/completion by ID.
- `Assets/Scripts/Jaluca/TrainingField/TrainingRoomConfig.cs`: task IDs and animation timing constants. Its old `IDs = {719..724}` helper is not the complete current tutorial graph.
- `Assets/Resources/DataBase/localData.bytes`: `Quest_List`, `Quest_EnterArea`, `Quest_LootItem`, `Quest_UseItem`, `Quest_KillMonster`.
- `Assets/Scripts/Mission/Mission.cs`: parses `Quest_List`, including the mission transition following the colon in `TalkED`.
- `i18n.db`, `Translation`: English task strings. IDs 3102669–3102674 specify F5 modes, WASD, Space, Shift+WASD, and middle-mouse autorun.
- `TutorialMode/Mainland/WorldEntity.xml`: tutorial scene doodad prototype 141, instance 7, at `(12,1.5,12)` with the authored rotation. This is a scene-placement record, not a player spawn.

The database completion chain is `739 → 747 → 718 → 740 → 741 → 742 → 743 → 744 → 749 → 745 → 746 → 719 → 748 → 757 → 721 → 751 → 752 → 753 → 756`. IDs are not chronological. The build-save branch contains `754 → 750 → 755 → 756`, but the inspected main chain goes directly from 753 to 756; do not assume 754 is reached without another trigger. This discrepancy is a useful original-runtime observation target.

| ID | Intended interaction and source completion condition | Supplied items / next step |
| --- | --- | --- |
| 739 | Switch control mode with F5; targets 4114/4115 represent Control and First Person modes. `NewMoveTask.OnControlModeChange` marks each and completes after two recognized callbacks. | Next 747. Its counter increments even for a repeated recognized mode; a faithful new mission interpreter should track distinct objectives and document this repair. |
| 747 | Perform Move, Jump, Sprint, and enable AutoRun. `NewMoveTask.Update` checks action states and `MainPlayerCmpt.AutoRun`, marking targets 4116–4119. | Next 718. |
| 718 | Andy's mission-panel/dialogue explanation; no spatial target. | `TalkOP` 4342–4346; next 740. |
| 740 | Move within 3 m of `(7,1.5,12)`, both in `NewMoveTask` and target 4071. | Next 741. |
| 741 | Open the warehouse and obtain herbal juice. Target 2091 requests item 916 ×5; opening the warehouse shows dialogue 300219. | Next 742. `HasItemObj` checks any item 916 and hides storage, but its completion dispatcher has no GetMedicine case; it does not prove one-item quest completion. C transfers the whole five-item stack. |
| 742 | Return to Andy; target 4072 is `(32,4,12)` within 3 m, scene 3. | Next 743. |
| 743 | Open inventory; `UseItemTask` listens to the package-open event. | Grants herbal juice 916 ×5; next 744. |
| 744 | Drag herbal juice to the quickbar. `UseItemTask` listens to its drop event; `UIMainMidCtrl.OnDropItem` emits that event only for prototype 916. | Next 749. |
| 749 | Consume one herbal juice 916; `Quest_UseItem` target 5018. | Next 745. |
| 745 | Equip a weapon. The script accepts any newly equipped item with non-null `weaponInfo`, despite the knife-specific prompt. | Grants knife 33 ×1; next 746. |
| 746 | Kill the target defined by `Quest_KillMonster` 1025, `MonsterID=13_1_1`. | Next 719 after dialogue 300230. Monster name/scene spawning need separate content interpretation. |
| 719 | Gather the tutorial herbs. `HoloherbTask` completes after its serialized herb count has been depleted. | Next 748. |
| 748 | Equip the supplied safety axe and cut a tree; targets 4122 and 2093, the latter requiring wood 279 ×1. | Grants safety axe 1527 ×1; next 757. |
| 757 | Use the wood-shovel script, target 5023 (`Quest_UseItem` item 406 ×1). | Grants script 406 ×1; next 721. |
| 721 | Create a wood shovel in the replicator; target 2089 requires item 12 ×1. Opening the replicator also shows dialogue 300241. | Grants wood 279 ×20; next 751. |
| 751 | Equip a digging tool and obtain dirt; target 2032 requires dirt 268 ×5. Tool-equip event checks editor type descendant of 5 and displays dialogue 4078. | Also grants wood shovel 12 ×1; next 752. This extra grant must not be removed merely because the preceding task produced one. |
| 752 | Enter build mode; `EmitlineTask.OnInBuildMode(true)` completes the task. | Grants dirt 268 ×9000; next 753. |
| 753 | Construct against the tutorial receiver/beam puzzle. `EmitlineRayCaster` assigns −100 when a ray still hits either receiver, otherwise +1; positive aggregate `testScore` completes it. | Next 756 in `Quest_List`. This is a ray-occlusion condition, not a simple block-count target. |
| 754 | Save a building ISO; listens to `mBuildBlock.onSaveIsoClick`. | Next 750; entry into this branch remains to be verified. |
| 750 | Open the building menu; listens to `mMenuCtrl.onMenuBtn`. | Next 755. |
| 755 | Place an ISO; listens to `BSIsoBrush.onBrushDo`. | Next 756. |
| 756 | Creation-editor introduction, dialogue 4074/300254/300255. | No `TalkED` successor in this record. The old creation-event completion code is commented out in `TrainingTaskManager`. |

These are source facts, not a report that the entire chain was played. Tutorial mission grants make its inventory path different from the C generated-world bootstrap. Current training implements the kit transfer and extra inventory-lesson grant, then one juice use; the sandbox's wood-gathering bootstrap remains separate.

## Tutorial-specific gathering and terrain

`HoloherbGather.cs` checks squared player distance `<7` (about 2.646 m), stops movement/sprint/rotation, and starts the gather action. `TrainingRoomConfig` sets total gather time 1.966 s and the removal event at 1.1 s. `HoloherbTask` removes the collider and counts depleted herbs. These scripted hologram actions are distinct from general biome gathering.

`HolotreeCutter.cs` requires an axe, the equipment-hold mask, and the Cut input. Its forward ray is 2 m. The staged timings are 0.6 s entry, a 1.1 s loop with hit at 0.6 s, and 0.533 s exit. The tutorial cutter and task have serialized integer HP counters; their decrement logic should not be replaced with the general resource-table tree-damage equation without checking the actual prefab. Current C trees use the general source attributes with explicit procedural-tree substitutes, not this tutorial action.

`TerrainDigTask.CreatTerrian` supplies a small deterministic mining mound using raw material 1: density 255 over x −2..15/z 2..21 at y=0; density 210 over x −1..14/z 3..20 at y=1; and enumerated upper samples at y=2/3 with densities 190,170,150,130,110. It is suitable for a direct density/marching-cubes fixture with a coordinate offset. It cannot be reproduced by a generic generated hill. This source task's creation/removal calls edit existing terrain and are separate from the tutorial map file import.

## Input and camera defaults

`PeInputSettings.cs` maps W/S/A/D movement, Space jump, Z walk/run, Left Shift sprint, middle mouse autorun, E combined interaction, F5 control-mode cycling, I inventory, Y replicator, C character stats, Q missions, J creation editor, and F9/F10 save/load menus. General Cut/Dig/Build use left mouse; right mouse serves put-away and interaction/gather/take depending on context. The C rebuild's right-click placement, C crafting alias, fixed material number keys, and instantaneous saves remain simplified bindings.

`MainPlayerCmpt.UpdateMoveState` starts with Run, then chooses Sprint while held or Walk for its Z toggle (also analog input magnitude between 0.1 and 0.9). Autorun adds forward motion when there is no directional input, is toggled by middle mouse, and is cleared by backward input below −0.5 or entering a carrier. Keyboard directions are normalized and transformed relative to the camera. Free-look and single-player pause suppress movement.

`PeCamera.cs` cycles ThirdPerson → MMOControl → FirstPerson → ThirdPerson on F5, setting cursor lock and notifying the tutorial. The naming of `mMMOControlType` is counterintuitive: `true` with `FirstPersonCtrl=false` selects unlocked-cursor third person; do not map labels from that boolean alone. `SystemSettingData.cs` initializes FOV 60°, camera sensitivity 1.5, both inversion flags false, and first person false. `PeCamera.Update` multiplies sensitivity by 3.5 for its camera graph. `ThirdPersonFollow` defaults distance 1.8–10 m, pitch −70°..55°, and reads right-mouse drag when the cursor is unlocked. Serialized camera-graph slots and saved settings can override these values. C's 70° FOV and raw-pixel sensitivity are not equivalent merely by changing the multiplier; Unity axis processing, camera graph smoothing, and rendered frame size must be accounted for.

## Movement constants: source initializers versus serialized data

The binary assets were read with UnityPy 1.25.4 in an isolated `/tmp/pe-asset-tools` environment; no Unity project asset was rewritten. Reproduce with `UnityPy.load(path)` and each object's `read_typetree()`. Store file hashes with exported movement data before making it a build-time dependency.

| Parameter | Evidence | Value and interpretation |
| --- | --- | --- |
| Normal movement | `Motion_Move_Human.cs` defaults; Normal enum=0 is absent from both model overrides | Walk 2, run 5, sprint 8 m/s before terrain, animation, and attribute effects. |
| Mining movement | `MaleModel.prefab` object 11418252 and `FemaleModel.prefab` object 11431752, `m_MoveSpeedList` style 6 | Walk 2, run 5, sprint 8. Weapon/heavy styles have different values; not all styles are implemented in C. |
| Sprint entry | Same objects, `m_MinStamina` | **15**, overriding `MoveParam.cs` initializer 10. An active sprint may continue below this threshold. |
| Sprint consumption | Same objects, `m_StaminaCostSpeed` | **5 per second**, overriding initializer 20; multiplied by `StaminaReducePercent`. |
| Sprint action behavior | `Action_HumanMove.cs`, `Action_Sprint.CanDoAction/Update` | Requires a nonzero direction; ends on exhaustion. Stamina consumption updates the latest-consumption time via `PESkEntity`. |
| Jump consumption | `Action_Jump.CanDoAction/DoAction` | Requires and consumes 10 × `StaminaReducePercent`. Jump update continuously delays regeneration while the action is active. |
| Jump impulse | `Action_Jump.DoAction` | `sqrt(20 × m_JumpHeight)`. Initializer height 2; `FemaleR.prefab` object 11400016 stores height 2.3. The live avatar path uses MaleModel/FemaleModel, so the legacy FemaleR override is not sufficient evidence to select 2.3 for the live player. |
| Air direction | `Action_Jump.Update` | 30% new direction +70% captured takeoff direction; current C does not reproduce this or animation/turning behavior. |
| Physics gravity | `ProjectSettings/DynamicsManager.asset`, object 1 | `(0,−9.8100004,0)`; human motion assigns `Physics.gravity.y`. |
| Recovery | `SkExpFuncSet.InCoolingForConsumingStamina`; player data | At least 0.1 s after latest consumption; base recovery 40/s, max 100. Active jumps and tool actions refresh the timestamp. |

`AvatarCmpt.cs` explicitly chooses `Prefab/PlayerPrefab/MaleModel` or `FemaleModel`. The serialized sprint values in those assets therefore outrank `MoveParam` field initializers. Full model/controller provenance still matters: the physical `HumanPhyCtrl` implementation is not present among inspected C# files, and runtime animation curves modulate horizontal speed. Do not call the simplified C controller a full port.

## Implemented correction and limits

The C core now uses walk 2/run 5/sprint 8, exposes `PeInput.walk`, requires 15 stamina to start sprinting, consumes 5/s, permits continuation below 15 until exhausted, and requires/consumes 10 for jumping. Movement does not regenerate stamina during these actions; recovery resumes after the minimum delay. Tool cooldown recovery remains integrated. Zero-input Shift does not drain stamina. The root frontend owns Z and autorun handling.

`sprinting` and `jumping` are transient action flags. Version 3 saves continue preserving stamina, velocity, position, and recovery timing; loading or replacing the world clears held-action flags. Reloading an airborne character therefore preserves its trajectory but does not resume the old jump action's regeneration lock. This is an explicit limitation, not source save compatibility.

Jump velocity 7 m/s, gravity 20 m/s², immediate horizontal acceleration, full airborne steering, sampled cylinder collision, and step handling remain temporary. The jump-height and controller change should follow live component/provenance confirmation and an observed trajectory fixture. Existing serialized gravity alone does not establish identical ballistic motion through the original physics controller.

Headless movement tests exercise walk/run/sprint distance, serialized sprint entry threshold, continuation/exhaustion, stationary Shift, recovery delay, insufficient-stamina jumps, one-time jump consumption, airborne recovery suppression, landing, and clearing transient actions on save/load. Tests also retain slope/collision coverage; full original movement parity remains an observation task.

## Next controlled comparisons

Current native fixture progression reaches756 through actual contacts, herbs, felling, crafting, mining and receiver construction. [PLAYTEST](../PLAYTEST.md) separates those fixtures from original observations. Original successful combat and later Adventure user sessions must not be inferred from the early Tutorial captures; use their own feature evidence. Next work belongs only to [HANDOFF](../HANDOFF.md), not this historical lesson audit.

## Recovered warehouse and spawn fixture

`TrainingManager_New.prefab` MonoBehaviour 11405616 references `mWareHouseObj` at external file 51/path 100266; GUID `a34b46db1f3f9514bb8df822943d958f` resolves to `Assets/Resources/Prefab/Item/Scene/emergencyKit2.prefab`. Its root transform 491316 is `(7,1.5,12)`, rotation Y 60 degrees, and warehouse ID 3. The database `box` row 3 corroborates the position and `itemList='916,5'`. `WareHouseObject.OnStart` replaces the serialized 19 m operate distance with 10 m; `UIWarehouse.Update` closes beyond 10 m. It is a small emergency kit, not a warehouse building. Runtime opening it displayed five herbal juices; see [the playtest](../PLAYTEST.md).

Fresh tutorial spawn is `(27,2,11)` in `Assets/PeLauncher/PeGameLoader.cs`, `LoadTutorialPlayerSpawnPos.New`. `LoadCreature.New` passes identity rotation and unit scale, so initial model forward is +Z before camera/movement changes. This is a source spawn fixture, not a measured runtime camera yaw.

A discovered automation constraint comes from `PeInputExt.IsClickNoMove`: right mouse must release with less than 20 pixels displacement and **strictly less than 0.25 seconds** elapsed. The reference helper now holds mouse buttons 150 ms. The E interaction alternative successfully opened the kit before that timing correction.

## Playable C training integration

`pe_training_view` connects the recovered room, source lessons, shared UI, actors, tools, contact/death, mining and construction through756. Its gameplay remains separate from sandbox. Detailed contracts: [interaction](TRAINING_INTERACTION.md), [combat](TRAINING_COMBAT.md), [gathering](TRAINING_GATHERING.md), [mining](TRAINING_MINING.md), [building](TRAINING_BUILDING.md). [PLAYTEST](../PLAYTEST.md#tutorial-native-graphical-fixtures) owns fixture results. The former746/no-combat/capsule-player checkpoint is obsolete.
