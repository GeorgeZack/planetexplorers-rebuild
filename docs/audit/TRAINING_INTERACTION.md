# Tutorial interaction text and conditions

This audit covers interface lesson 718, movement arrival 740, storage lesson 741, return to Andy 742, inventory lesson 743, quick-bar lesson 744, medicine use 749, and the transition into equipment lesson 745. It distinguishes dialogue IDs, localization IDs, database objectives, and scripted callbacks. The implementation in [`pe_training_text.c`](../../rebuild/src/pe_training_text.c) is a small immutable English lookup; it does not execute missions or imply that every original UI interaction is implemented.

## Text provenance and API

The source lookup chain is **`Quest_List.TalkOP` → `Scenario.talk_id` → first component of `Scenario.talk_content` → `Translation.ID`**. `TalkRespository.LoadData` in [`NpcTalk.cs`](../../Assets/Scripts/Mission/NpcTalk.cs) splits `talk_content` into `localization_id:sound_id:clip_name`. There is no `Quest_Dialogue` table in this database. In particular, 4342–4346 are dialogue IDs, not localization IDs; directly looking them up in `Translation` returns no rows.

English strings were verified equal in the repository's two localization sources:

| Source | SHA-256 |
| --- | --- |
| `Assets/Resources/DataBase/localData.bytes` | `c3d63ee865a7e10e309e11e9ee561c93feef39c379a584dcf68722a523cd4376` |
| `i18n.db` | `965648c34c5767aeeba8187c121ba128a24e5915eb532d18f666371e24f40299` |

[`pe_training_text.h`](../../rebuild/src/pe_training_text.h) exposes:

- `pe_training_dialogue_text(id)`: original English text for a dialogue ID.
- `pe_training_dialogue_entry(id)`: that text plus dialogue ID, localization ID, and original speaker NPC ID.

Both return borrowed immutable data, perform no I/O or allocation, and return `NULL` for an unknown dialogue ID. Mission ID 718 is intentionally not accepted as if it were dialogue 4342. The original wording and spelling are retained, including “one if for” in dialogue 4346. Frontends should wrap the strings for readability instead of changing their meaning.

The first 17 of the module's 25 entries cover the interface through inventory introduction:

| Dialogue | Localization | Source speaker | Purpose |
| --- | --- | ---: | --- |
| 4342 | 3004342 | 0 | Introduce the interface |
| 4343 | 3004343 | 0 | Quick bar and health/stamina/comfort/hunger |
| 4344 | 3004344 | 0 | Lower-right menu |
| 4345 | 3004345 | 0 | Minimap and world map |
| 4346 | 3004346 | 0 | Mission log and conversation history |
| 300216 | 3300216 | 9041 | Move to the materials at the other side of the deck |
| 300218 | 3300218 | −9995 | Right-click storage instruction |
| 300219 | 3300219 | −9995 | Transfer items between inventories |
| 300220 | 3300220 | 9041 | Bring the materials back |
| 300221 | 3300221 | 9041 | Medicine explanation |
| 300222 | 3300222 | 9041 | Place commonly used items on the quick bar |
| 300223 | 3300223 | −9995 | Open inventory using menu or I |
| 1558 | 3001558 | −9999 | Player: “What to do next?” |
| 4067 | 3004067 | −9999 | Player: “I have the items from the storage box.” |
| 4068 | 3004068 | −9995 | Close a UI window |
| 4069 | 3004069 | −9995 | Mission marker explanation |
| 4070 | 3004070 | −9995 | Right-click/E character interaction |

NPC 9041 is Andy. The other speaker values are preserved source IDs/sentinels; the lookup deliberately does not invent character names for them. Audio IDs, clip playback, localization switching, and animated conversation presentation are outside this module.

## Mission and interaction semantics

### 718: interface introduction

`Quest_List` sets `TalkOP=4342,4343,4344,4345,4346` and `TalkED=0:740`. `GameUI.CheckTalkIDShowTutorial` uses dialogue IDs 4343, 4344, 4345, and 4346 to show the quick-bar, menu, minimap, and mission-track overlays respectively. These are dialogue-driven UI cues, not four movement objectives. The five source English pages should retain their order before advancing to the arrival lesson.

### 740: reach the materials

The opening line is dialogue 300216. `Quest_EnterArea` target 4071 specifies scene 3, position **(7,1.5,12)**, radius **3 m**, and next quest 741. Independently, `NewMoveTask.Awake` sets the same target, and `Update` completes the movement task when three-dimensional `Vector3.Distance` is **≤3 m** and mission 740 is active. The source uses full 3D distance, not an x/z-only circle.

### 741: open storage and obtain medicine

`TrainingTaskManager.Awake` instantiates its serialized `mWareHouseObj`, while `Start` initially disables it. The reference resolves to `Assets/Resources/Prefab/Item/Scene/emergencyKit2.prefab`, GUID `a34b46db1f3f9514bb8df822943d958f`, root object 100266. Its root is at **(7,1.5,12)** with a **60° Y rotation** and warehouse `_id=3`. `InitGetMedicineScene` enables it and subscribes to `UIWarehouse.OnShow`.

The database `box` row 3 contains `itemList=916,5`: **five herbal juices**, item 916. `WareHouseObject.OnStart` initializes the package from this row, then overrides its serialized operate distance of 19 with `MaxOperateDistance=10`. `UIWarehouse` closes when the player moves farther than 10 m. The storage supports a package capacity of 30 slots per item category.

Mission 741's `StoryInfo=1_428` refers to `Plot` 428, whose `ScenarioID=300218` says to right-click storage. When `UIWarehouse.Show` fires, `NewMoveTask.OpenWareHouseTalk` queues dialogue 300219 and unsubscribes itself. Showing storage also opens the player's inventory if needed. That earlier opening must not accidentally count as the later inventory lesson before its listener is attached.

Mission 741 lists targets 4121 and 2091. Target 4121 is an event-style placeholder at `(10000,0,0)`; it should not be treated as a real destination. `Quest_LootItem` target 2091 explicitly requires **item 916 ×5** and names next quest 742.

There is a significant script/database distinction: `NewMoveTask.Update` detects **any** item 916, sets `isGetMedComplete`, hides storage, and calls `TrainingTaskManager.CompleteMission`. However, that dispatcher has **no `GetMedicine` branch**, so the call does not itself prove that one juice completes mission 741. An older `OnItemAdd` handler is not subscribed in `Start` and should not be treated as active behavior. Transferring the complete five-item stack satisfies the explicit loot objective and avoids assuming an unverified one-item completion rule. Runtime ordering between objective updates, storage hiding, and item transfers remains a comparison point.

### 742: return to Andy

The database identifies NPC and reply NPC **9041**, prerequisite mission 741, and `Get_DemandItem=916_1`. Its opening line is 300220. Target 4072 specifies **(32,4,12)** with radius **3 m**. `player_talk=1558,4067` supplies the two player-side lines included above; `TalkED=0:743` leads to the inventory lesson.

`StoryInfo=1_469` refers to `Plot` 469, which queues 4068,4069,4070: close the window, understand mission markers, and interact with a character using right-click/E. Reaching an area and completing an NPC reply interaction are distinct events; proximity alone should not be assumed to reproduce the original reply UI handshake.

### 743: inventory transition

Opening dialogue is **300221,300222**. `Get_MissionItem=916_5` grants another five item 916 on mission acceptance according to the mission data; this is separate from the contents originally transferred out of storage. `StoryInfo=1_429` points to `Plot` 429 and dialogue **300223**, the inventory-opening instruction.

`UseItemTask.InitOpenPackageScene` subscribes to `mItemPackageCtrl.e_OnOpenPackage`. The callback unsubscribes itself and calls the mission dispatcher, whose `OpenPack` branch completes 743. Target 4102's `(10000,0,0)` is another placeholder; its condition is the package-open event, not travel. The data then names quest 744. Quick-bar placement belongs to subsequent behavior and must not be inferred to be a completion requirement for 743 merely because line 300222 mentions it.

## Verified item accounting and later lessons

### Demand checks and grant timing

`Get_DemandItem=916_1` on 742 is a possession requirement, not a deduction. `PlayerMission.cs` checks counts in the `m_Get_DemandItem` loop at approximately lines 846–855. The separate `m_Get_DeleteItem` loop performs deductions on acceptance around line 2880; mission 742's `Get_DeleteItem` and `CoRemoveItem` are both zero. The five transferred juices therefore remain after returning to Andy.

In `SetQuestVariable1`, acceptance grants from `m_Get_MissionItem` are applied when `m_bHadInitMission && !HasGetRewards(MissionID)`. The mission is recorded in the grant set before adding its items, preventing ordinary repeat acceptance from granting them again. Mission 743 adds five juices before `SpecialMissionStartHandle` calls `TrainingTaskManager.InitMission`, which attaches its inventory-open listener. Original runtime observation supplied by the root agent corroborates this accounting: after Andy's reply and lines 300221/300222, the inventory showed a stack of **10** and a **Herbal Juice ×5 (10)** grant notice. The source's earlier inventory window opening during storage must not cause a second grant.

### 744: assign herbal juice to the quick bar

Mission 743 leads to 744. Mission 744 has no `TalkOP`; `StoryInfo=1_430` points to `Plot` 430 and dialogue **300224**: `Click the "Items" tab, and drag "herbal juice" to the quick bar.` Its target 4103 is an event placeholder.

`UseItemTask.InitPutMedScene` subscribes to `UIMainMidCtrl.e_OnDropItemTask`. In `UIMainMidCtrl.OnDropItem`, the selected item, UI, slot list, and target grid must be valid, and the item must not currently be in use. The callback fires only when the selected item's prototype ID is **916**. The callback unsubscribes and dispatches completion of 744, leading to 749. Source ordering fires this event before the remainder of quick-bar assignment logic; a C implementation may deliberately commit a valid assignment before advancing, but should identify that atomicity choice rather than claiming identical callback timing. Assignment is an inventory shortcut, not item consumption.

### 749: use one herbal juice

Mission 749 opens with **4071** (“Good job!”). `StoryInfo=1_493` points to `Plot` 493 and **4298**, the instruction to press the corresponding number key. `Quest_UseItem` target **5018** requires item **916**, `UseNum=1`. Completion names mission 745. Plot 493 has no damage or attribute-modifying fields: this inspection does not support reducing health solely to make the medicine lesson visible.

The item-to-effect chain is:

| Record | Relevant values |
| --- | --- |
| `PrototypeItem` 916 | Herbal juice; `skillId=20150023`; maximum stack 100 |
| `skMain` 20150023 | Cooldown 10 s; shared cooldown type 11, shared duration 10 s; trigger event 46 |
| `skTriggerEvent` 46 | Target modifier `rnd,1,1,30200009,1` |
| `skBuff` 30200009 | Type 35100, priority 1, stack limit 1, lifetime 10 s, interval 1 s |
| Buff modifier | `exp,1,if(target.raws[1]>0f){target.raws[1]+=30f;}` |

Attribute index 1 is **HP**, as defined in `PESkEntity.AttribType`. This buff restores **30 HP per execution** while HP is positive. It has no stamina-restoration term. `AttribPair.CheckAttrib` clamps HP to the entity's HP maximum. The player's source baseline comes from `PlayerProtoDb.Get` and the first `initprop` row: **HP 500/500, stamina 100/100**. The `Hp=100` fields on the medicine's item-prototype row are not the player's health baseline.

Timing is more specific than a generic “heal 300 over 10 seconds.” `SkBuffInst.Exec` executes modifiers immediately when `_n==0`, increments `_n`, then executes one additional interval pulse if `elapsed > _n * interval`. It checks `elapsed > lifetime` **after** that pulse. With regular frame updates, a pulse just after 10 seconds can therefore occur before removal; there is no `while` loop to catch up multiple missed intervals in one update. Preserve or explicitly simplify this scheduler behavior rather than inventing an exact fixed total. Mounting another instance at stack limit 1 removes the previous same-ID buff; the skill cooldown separately constrains reuse. Successful-use dispatch, item removal, and quest completion ordering still belong to the item-action integration, not this text lookup.

### 745: equipment transition

Acceptance grants **knife item 33 ×1**. The opening dialogue order is **4074,300225,300226**: confirmation, simulated hare explanation, then the instruction to equip the knife. `StoryInfo=1_431` points to **300227,300228**, inventory-opening and Equipment-tab instructions. Target 4104 is another event placeholder. `UseItemTask.InitEquipKnifeScene` listens for an added equipment event and accepts any equipped item with non-null `weaponInfo`; it does not explicitly compare the item ID with 33. Combat progression beyond this transition is not established here.

Additional text entries are:

| Dialogue | Localization | Speaker | Purpose |
| --- | --- | ---: | --- |
| 300224 | 3300224 | −9995 | Assign juice to quick bar |
| 4071 | 3004071 | 9041 | “Good job!” |
| 4298 | 3004298 | −9995 | Consume via corresponding key |
| 4074 | 3004074 | 9041 | “Confirmed.” |
| 300225 | 3300225 | 9041 | Simulated hare introduction |
| 300226 | 3300226 | 9041 | Equip knife |
| 300227 | 3300227 | −9995 | Open inventory |
| 300228 | 3300228 | −9995 | Equipment tab and knife |

## Validation and remaining boundaries

The text module compiles independently as strict C11 with `-Wall -Wextra -Wpedantic -Werror`. A headless lookup check compares each included dialogue/speaker/localization mapping with `Scenario` and verifies the returned English against both databases. Unknown IDs return `NULL`.

The source inspection establishes text, coordinates, data links, and named event handlers. It does not establish complete mission-engine ordering, all placeholder-target side effects, controller/input equivalence, audio/UI parity, or frontend validation. The state-machine implementation and original-game playthrough should record those observations separately; this document must not substitute an inferred script behavior for an observed runtime result.
