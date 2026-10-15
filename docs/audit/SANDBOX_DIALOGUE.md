# Adventure Arrival and supplies dialogue

This bounded October5,2026 implementation adds a transient source menu/page host beside `PeGame`; it does not recreate Adventure town generation, the full NPC mission system, ; [town interactions](ADVENTURE_TOWN_INTERACTIONS.md) owns the now-integrated Annot trip and follow-up. Original source files and databases remain unchanged. New graphical validation is pending; this audit distinguishes source rules from the native host's explicit interruption and inventory policies.

## Exact source records and roles

`AdRandomQuest_List` in [localData.bytes](../../Assets/Resources/DataBase/localData.bytes) defines:

| Mission | Source title | Acceptance/menu text | TalkOP | Target / acceptance items |
|---|---|---|---|---|
|9031 |Arrival, localization3102562 |player choice4087 |4088 |4502; no items |
|9032 |The Delayed Supplies,3102565 |player choice4089 |4090,4091,4092,4093,4094 |4503; eighteen one-unit entries in Get_MissionItem |

The initial source welcome chain9027→9028→9029→9030 accepts9031. Native Adventure now runs the nine source pages1949,4081..4088 with role1 Westina and the player alternating. Final4088 acknowledgement starts9031. Older v4/v5 saves retain their previous post-welcome state. See the [welcome flow audit](ADVENTURE_WELCOME.md). `AdQuestChain`31 requires9031 and selects9032 with weight100. Later chain missions are unimplemented.

Scenario4088 uses NPC sentinel−9900, the generated `AdvNPC001` welcome role;4090/4092/4094 use−9899, `AdvNPC002`;4087/4089/4091/4093 use−9999, the player. [UINPCTalk.cs:1209](../../Assets/Scripts/Chenzhi/NewUI/Scripts/PE_GameUI/NGUIScripts/UINPCTalk.cs#L1209) maps Adventure sentinels through `adId_entityId[9901+sentinel]`. These are role/entity mappings, not NPC prototypes or universal names.

The current reference fixture has Eitai Takayama as role2, Westina Nangle as role1, and Annot as Town004. These bindings come from the inspected original save/session; the core and generated text contain none of those fixed names. The bounded frontend may bind that reference fixture and place its recovered supplier locally, but that placement is not original generated-town parity.

Text role tokens use **given names**: [EntityInfoCmpt.cs:219](../../Assets/Scripts/PeEntity/Cmpt/EntityInfoCmpt.cs#L219) sets `GameObject.name` to `givenName + '_' + entityId`; [UINPCTalk.cs:588](../../Assets/Scripts/Chenzhi/NewUI/Scripts/PE_GameUI/NGUIScripts/UINPCTalk.cs#L588) removes that suffix for `"AdvNPC%"001/002`. Town tokens resolve `VArtifactUtil.GetTownName`. Menu titles and dialogue speaker labels use `ExtGetName`, which returns `characterName.fullName` in [PeEntityExt.cs:200](../../Assets/Scripts/PeEntity/PeEntityExt.cs#L200). Native bindings therefore distinguish text names from optional full speaker names.

## Arrival completion is a proximity target

`AdQuest_EnterArea`4502 has `DistPos=3_2_0_5`, track radius60 and NotForDungeon1. [AdRandomMission.cs:572](../../Assets/Scripts/Mission/AdRandomMission.cs#L572) parses reference kind3/NPC, role2, offset radius0, completion radius5. [PlayerMission.cs:3658](../../Assets/Scripts/Mission/PlayerMission.cs#L3658) resolves role2's starting position and converts its XZ to integer coordinates; target Y comes from terrain. This is a stored target position, not continual tracking of a moving NPC.

[MissionManager.UpdateSearchMission:3341](../../Assets/Scripts/Mission/MissionScript/MissionManager.cs#L3341) rejects below-world/dungeon playerY<0, uses XZ distance for random missions and completes only when distance is **strictly less than5**. Target4502 and AutoReply1 therefore complete Arrival without an extra supply grant or dialogue acceptance. The misleading-looking English objective “Talk to …” does not change that source condition.

`pe_adventure_arrival_step` implements this bounded static-supplier condition separately from opening a menu. It requires an alive Adventure player and the actual resolved role2 position. The frontend supplies a stable start position; an unresolved role cannot silently turn a beacon into a named NPC. NPC hover/interaction eligibility remains a separate host query, not this5m mission radius. Original interaction uses a capsule/AABB hover/range pipeline rather than the former native nearest-beacon shortcut.

## Acceptance, closing and capacity

Selecting source player choice4089 in [UINpcWnd.cs:737](../../Assets/Scripts/Chenzhi/NewUI/Scripts/PE_GameUI/NGUIScripts/UINpcWnd.cs#L737) opens the quest's TalkOP pages. [UINPCTalk.MatchTalkInfo:862](../../Assets/Scripts/Chenzhi/NewUI/Scripts/PE_GameUI/NGUIScripts/UINPCTalk.cs#L862) attaches the mission trigger only to the **last** page. [OnClose:1419](../../Assets/Scripts/Chenzhi/NewUI/Scripts/PE_GameUI/NGUIScripts/UINPCTalk.cs#L1419) increments the page, then dispatches that trigger. Thus acknowledgment of4094, not menu opening, choice selection or page4092's starter-pack promise, invokes9032 acceptance.

[PlayerMission.cs:2877](../../Assets/Scripts/Mission/PlayerMission.cs#L2877) applies acceptance items while mission initialization is active and `HasGetRewards` is false. The eighteen entries remain the existing exported `pe_adventure_supplies` table and `pe_game_accept_supplies` transaction. Opening inventory or the NPC menu cannot award them. Already-claimed state survives PECS saves and cannot grant a second package.

The source normal dialogue background click and skip-dialog input call `OnClose`, which **advances**, rather than cancelling the conversation. Native `pe_adventure_dialogue_cancel` is a deliberately separate interruption policy for closing an interaction or switching application state: no grant, transient pages discarded, reopening returns to the menu and replays the source sequence. This safe cancellation is not a claim that Unity's page-close button aborts the quest. The source NPC menu close entry is localization8000010, “That's all.”, added by [UINpcWnd.AddCloseItem:486](../../Assets/Scripts/Chenzhi/NewUI/Scripts/PE_GameUI/NGUIScripts/UINpcWnd.cs#L486); its−2 branch closes the window.

Native capacity handling intentionally retains the existing atomic conservation repair: if the complete eighteen-item package cannot fit, final acknowledgment returns NO_SPACE, preserves the page4094 and all inventory/progression, and supplies a capacity message. Repeated retries cannot partially award items. Closing, freeing inventory space, and replaying permits successful acceptance. Source code computes item-category counts but the inspected acceptance path adds items individually; the native atomic behavior is not represented as exact replication of that failure path.

After acceptance, opening the supplier still produces a menu with no offer; `current` returnsNULL and selection fails, allowing the UI to retain only “That's all.” without inventing a postquest line. The supplier remains a world entity. Source9032 objective3102566 (“Go to Town004”) is applicable after acceptance. `PeGame.adventure_mission==9032` already represented the offer stage before this module; callers must use `supplies_claimed` to distinguish available from accepted, rather than prematurely showing the Town004 journey as active.

Before Arrival completes, an eligible alive Adventure player may also open this close-only menu. [UINpcWnd.ResetMissionList:530](../../Assets/Scripts/Chenzhi/NewUI/Scripts/PE_GameUI/NGUIScripts/UINpcWnd.cs#L530) adds the close entry independently of quest availability; `AdQuestChainLimit` gates the supply offer separately. The native menu accepts mission9031 or9032, while the offer explicitly requires `arrival_complete`, mission9032, and unclaimed supplies together. Opening, attempting to select an unavailable offer, or closing cannot complete Arrival or change inventory. This distinction matters because the source4.5m physical-AABB interaction range and the strict5m integer-truncated Arrival anchor can disagree near their boundaries. Actual proximity may subsequently enable the offer in an already-open menu.

## Native API and data boundary

[pe_adventure_dialogue.h](../../rebuild/src/pe_adventure_dialogue.h) exposes IDLE/MENU/TALK, actual bound entity identity, page state, open/choose/advance/cancel, current source line/speaker and bounded text formatting. It owns no inventory, mission-completion flags or persistent references. `PeGame` remains authoritative; a stale menu/page after external acceptance cannot award again. Dead players and invalid host eligibility cannot start an interaction. Reopening/loading initializes transient state; no save-format change is required.

The formatter resolves only the supported source player/AdvNPC001/AdvNPC002/Town004 tokens. Caller-supplied names are bounded, missing bindings preserve the original token, and too-small output buffers fail without changing the destination. It does not recursively substitute inserted names. Speaker full names have independent bindings.

[export_adventure_dialogue.py](../../rebuild/tools/export_adventure_dialogue.py) emits [pe_adventure_dialogue_text.h](../../rebuild/generated/pe_adventure_dialogue_text.h) and its JSON provenance manifest, preserving original scenario/localization IDs, role tokens, quest titles/descriptions/objectives and source hashes. It fails if the expected talk sequence, speaker roles, chain or arrival condition changes. Regenerate or verify with:

```sh
python3 rebuild/tools/export_adventure_dialogue.py
python3 rebuild/tools/export_adventure_dialogue.py --check
```

## Validation

The four exporter tests pass, including checked-in regeneration, changed-sequence rejection, wrong-speaker rejection and missing-English rejection. The dedicated C tests cover strict/truncated proximity, below-world/dead/invalid inputs, role/entity/eligibility, pre-Arrival close-only menu cancellation/reopening and conservation, enabling its offer only after proximity completion, menu/page ordering, no premature grant, final acceptance exactly once, cancellation at every page, atomic repeated capacity failure, freeing space/replay, saved grant state, stale UI state, full/given name separation and formatting failure atomicity. The dedicated native suite passes strict C11 warnings-as-errors with ASan, UBSan and leak detection; the exporter regeneration check also passes. Frontend validation is recorded separately in the integrated desktop checkpoint below.

Integrated C desktop verification subsequently passed: source menu and all five dialogue pages, visible saved appearance/portrait, cancellation without items, final grant once, and save/reload. See [PLAYTEST](../PLAYTEST.md); this does not imply a new original-game observation or full town parity.
