# Tutorial UI replication guide

This guide separates original source UI behavior from the C tutorial’s implemented lesson state. The C tutorial reaches mission756 through real item transfers, drag assignment, equipment changes, crafting, mining and construction. Its original rectangular presentation has now been replaced by recovered source UI assets/layouts, while preserving those headless transactions and event gates. Manual UI checks and the full progression state checks now pass; the final tracked15,000-frame run after visual polish passed with exit0. Original screenshots under `docs/reference/` are the visual checks; prefab data supplies geometry and widget identities. No original UI parity is claimed merely because its textures have been extracted.

## Reproducible source layout

[TRAINING_UI_LAYOUT.json](TRAINING_UI_LAYOUT.json) records583 widgets from eleven original binary Unity prefabs, decoded with UnityPy1.25.4. Each prefab has its source path and SHA256, and each widget retains its serialized object ID, hierarchy path, active state, anchor side, complete transform chain after the anchor, widget fields, external GUIDs and resolved bitmap-font path. This is source data, not a screenshot trace. `unrotated_anchor_position` and `unrotated_widget_scale` are convenience values only: use the retained rotations when a widget is mirrored, and apply runtime window positioning separately. Duplicate hierarchy names refer to distinct serialized objects and must not be collapsed.

The main source directory is `Assets/Scripts/Chenzhi/NewUI/Resources/Prefab/`. `GameUI/GameMain.prefab` contains both `ButtomAnchor/MainMid` and `ButtomAnchor/NpcTalkBox`. Dialogue is not a standalone `NPCTalkWnd` prefab. The main inventory prefab is spelled **ItemPackgeWnd**. Additional extracted prefabs are WarehouseWnd, CompoundWnd, PlayerInfoWnd, MissionTrack, MissionTrackItem, MissionTrackWndTutorial, MinMapTutorial, NewGrid_N and QuickBarItem.

Old NGUI represents many widget dimensions through `Transform.localScale`; these are not Unity world metres. Coordinates below use UI pixels, +X right, +Y up. For a bottom-center anchor `(dx,dy)`, top-left-screen coordinates are `(screenWidth/2+dx, screenHeight-dy)`. A centered sprite’s rectangle begins half its width/height before that point. The main GameUISystem UIRoot has `automatic=1`, minimumHeight320, maximumHeight2560 and manualHeight800; the manual value is not justification for stretching the entire1280×720 UI as an800-pixel canvas. Anchors and window clamps should be preserved at other resolutions. Older `GameUIRoot(new)` has a different maximum1080 and is not the authoritative GameUISystem root.

`GameUISystem.prefab` object11421344 supplies runtime default window centers, consumed by `GameUISystem/GameUI.cs`: inventory `(300,-25)`, equipment/player info `(-350,-25)`, warehouse `(-300,0)`, replicator `(0,0)`, NPC interaction `(-320,-25)`, all relative to screen center. Source windows can be dragged and recent settings can change placement. The initial defaults match the preserved1280×720 inventory/storage screenshots.

## Atlases, fonts and draw rules

The source atlases are `_Atlas/_UIBase/NewUIAtlas` and `NewIcon`, with serialized sprite outer/inner rectangles and padding. The current asset exporter is [export_training_ui.py](../../rebuild/tools/export_training_ui.py), run as `/tmp/pe-asset-tools/bin/python rebuild/tools/export_training_ui.py --output FRESH_DIR` with UnityPy/Pillow. The initial two fresh exports produced the same23 files byte-for-byte, and four asset tests passed; the later gender-atlas and effect additions have separate checks in [UI_EFFECTS.md](UI_EFFECTS.md). It produces `rebuild/build/reference-assets/ui/atlas.peui`, original atlas PNGs, bitmap font files and `manifest.json`. The native export now contains2474 selected sprite names across three PNG atlases, including the two source VCE gender icons used by character information. Keep original atlas UVs, padding, nine-slice borders, widget color and alpha. A tinted copy of a frame is often intentionally layered over its white copy; it is not another panel or a second half.

GameMain dialogue name uses `ui_Font_18_bold.prefab`; body uses `ui_Font_18_talk.prefab`. Both resolve to the `ui_Font_18_bold` bitmap glyph sprite, source size18/base14 and625 glyph metrics. The prefab UILabel local scale is20 for both, but that authored size is not the final runtime size. `GameUISystem.prefab` component11400048 is `UISwitchFont`; `UISwitchFont.Start():9–18` visits all descendant labels, resolves the language font through `UIFontMgr`, then calls `MakePixelPerfect`. Thus these labels render at the font’s native18-pixel size. The same path changes authored37-pixel window headings to native32 and authored20-pixel tabs to18. The C renderer loads six of the eight exported bitmap fonts:18 regular/bold,14 regular/bold,26 for names and32 for headings. Mission body text uses native18. Body wrapping is620 source pixels. Extracted bitmap metrics are preferable to replacing these with a system font; Orbitron Light/Bold TTFs also exist in `Assets/Resources/Font/`, but matching the family alone does not reproduce NGUI advances, baseline, effects or wrapping. NGUI color tags must be interpreted or removed deliberately, not rendered literally.

NPC overhead names have their own runtime font switch. `OverHead.prefab` NameLabel11401772 uses `ui_Font_26`, white, no text effect and centered pivot; its authored30-pixel scale becomes native26. Its parent Namlb has scale0.01 and localY0.125; `NpcOverHead` supplies minDist4, maxDist30, heightAdjust0.04 and scaleAdjust9. These are distinct from screen-space panel labels. The C name renderer uses white centered26-pixel text without a background or shadow and source-style projected distance sizing. Its head point comes from the recovered mesh bounds, an approximation of source `uiHeadTop`, rather than a recovered head attachment bone. The inventory heading’s native32-pixel advances match the original captured width; exact vertical NGUI pivot/baseline equivalence is still under comparison.

The exporter preserves malformed original border values in its provenance JSON and clamps the native nine-slice border to a nonnegative value. Source `sign_player` has bottom−6 and `WorldMapDes` bottom−30. This normalization is an explicit renderer compatibility adjustment. Duplicate `build_voxel` sprite names are disambiguated in the exporter. Source atlas recovery is verified; equality to every installed runtime atlas has not been established by this audit.

## Bottom HUD and quickbar

Source: `PE_GameUI/GameMain/UIMainMidCtrl.cs`, `UIComponent/UIGridBoxBars.cs`, `PE_GameUI/Widget/QuickBarItem_N.cs`, and GameMain/MainMid prefab entries.

| Element | Sprite | Bottom-center position | Size | Notes |
|---|---|---|---|---|
| Dark quickbar backing | shortbar1bg | (0,49) |845×54|depth−1|
| Frame | shortbar1 | (0,54) |845×110|white depth1 plus blue tint depth2|
| Health backing | bloodlan_6bg |(0,57)|164×30|depth1|
| Health fill | bloodlan_6 |(0,57)|164×30|depth2; actualHP/maxHP|
| Stamina backing | bloodlan_7bg |(0,37)|138×9|depth0|
| Stamina fill | bloodlan_7_1 |(0,37)|138×9|depth1; source blue tint|

Separate health/stamina addition and loss sprites provide delayed feedback. The health loss layer is `bloodlan_6_1`; stamina loss is a horizontally mirrored `bloodlan_7_1`. Do not infer active values from the serialized fillAmount0; the runtime controller drives them. Other HUD widgets include energy, jetpack, comfort, hunger, oxygen, shield, ammunition and buff icons. The C tutorial has HP and stamina state, plus juice cooldown/buff state; unsupported survival values must not be displayed as simulated values.

There are **ten displayed grid cells: one equipped-item display and nine assignable shortcuts**, not ten assignable shortcuts. Serialized `mItemCount=10`. InitGrid splits the cells five/five around the central status panel. Index0 receives `UpdateKeyInfo(-1)` and icon `itemhand`; only indices1..9 receive drag/right-click handlers. There are three shortcut pages. Page changes wrap; backing offset is `(page−1)*(itemCount−1)`. `QuickBarItem_N` uses `num_1..num_9` and shows `QuickKey_N` under an occupied slot. The rightmost `buttonb` is the separate Build Mode button, not shortcut0.

Left-click begins item selection/drag, right-click invokes the item action, and a successful drop assigns a reference without consuming the inventory item. Original `UIMainMidCtrl.OnDropItem` fires the herbal-juice tutorial callback before the later assignment branch; the C tutorial intentionally requires an actual successful assignment. Keep the stronger C transaction gate rather than completing a lesson on an invalid drop. Current training only supports bound slot1/juice916; rendering additional empty source slots must not silently create unimplemented shortcut behavior.

## Dialogue panel and speaker identity

Source: `PE_GameUI/NGUIScripts/UINPCTalk.cs`, GameMain/NpcTalkBox and `Scenario`→`Translation` database rows. The screenshot [native-tutorial.png](../reference/native-tutorial.png) shows Andy’s initial explanation; [inventory-extra-grant.png](../reference/inventory-extra-grant.png) shows a Tips explanation over inventory.

| Element | Sprite/font | Bottom-center point | Size/pivot |
|---|---|---|---|
| Dialogue background |talkbg|(0,180)|850×150 centered|
| Portrait |npc_big_Unknown replaced by speaker icon|(-345,202)|130×130 centered; MakePixelPerfect may use actual sprite size|
| Name backing |namebg|(-341,124)|160×28 centered|
| Speaker label |ui_Font_18_bold|(-343,124)|centered, authored scale20 → runtime18|
| Body |ui_Font_18_talk|(-245,235)|top-left, authored scale20 → runtime18, maxLineWidth620|
| Advance button |btn_down|(405,135)|22×20 centered|

Two talkbg objects share the same850×150 geometry: white plus tint `(0,0.1294118,1,0.3019608)`. They are **not mirrored half panels**. Body and portrait are in a foreground content transform; text must remain above the background and preserve clipping/scrolling for long entries. At1280×720 the panel bounds are approximately x215..1065,y465..615, consistent with screenshot chrome; transparent sprite margins explain smaller visible ink bounds.

Startup is source-backed: TrainingTaskManager.MissionStart calls the normal739 opening dialogue. Quest739.TalkOP is300215 (Andy’s simulation-deck welcome); its acceptance StoryInfo1_351 triggers Plot351→300258 (Tips explaining third-person RMB/F5). Plot470 only rotates Andy275degrees. The C core now presents those two lines in order, rejects stale/out-of-order acknowledgements and holds camera objective input until both have been acknowledged. The unrelated300211 line is not part of tutorial startup.

Speaker−9995 is explicitly **Tips**, not Training (`UINPCTalk.cs:933,1602`). Andy’s entity resolves `npc_big_Andy`; unresolved speakers fall back to `npc_big_Unknown`. Other NPCs use their own `ExtGetFaceIconBig`, not Andy’s portrait. Non-NPC entities can use a dynamically rendered150×150 photo. The C metadata already carries speaker IDs and should drive this choice.

The source background click calls OnClose when no mutex is active; normal dialogue also processes `UI_SkipDialog1` (PeInput.cs:318 maps this to Jump_SwimmingUp, default **Space**), while special/half-special speech can advance from audio length. Do not equate all source speech with a pause menu. The C tutorial accepts Space, Enter or a dialogue click and consumes acknowledgement before movement can treat Space as a jump. The C host blocks locomotion and camera-mode changes during every active dialogue. This is a bounded-host modal policy, not verified source-equivalent F5 blocking: `Assets/PeCamera/Scripts/PeCamera.cs:231` uses `PeInput.Get(ChangeContrlMode)`, whose `InputEnable` guard at `Assets/Scripts/Input/PeInput.cs:380–388` checks plot animation, story pause, creation editor and text-input focus, but does not directly reject every open NPC dialogue. Preserve the C stale-ID/duplicate-ack guard, and consume an acknowledgement click before world interaction or block placement.

## Mission tracker and tutorial callouts

Source `UIMissionTrackCtrl.cs:179–218` renders main mission titles purple `#9B3DF4`, side mission titles cyan `#37CBF4`, unfinished objectives white and completed objectives gray `#B4B4B4`. Counts appear only for positive maximum counts as `current/max`. Completed titles append localized ` [Com]` (Translation8000694). The tree is updated every0.5s and main missions sort before side missions. The actual tracker uses `GameUI/MissionTrackNode.prefab`, referenced by MissionTrack object11481166 (GUID5924b4b9066018144b35b7768e9d018f), not the older MissionTrackItem. Its Label11400028 resolves `ui_Font_18_talk`, with the bold glyph sheet, native18 size, top-left pivot0, maxLineWidth233 and local offset(15,−4); both mission roots and targets use this font. Each node’s height follows wrapped text within the330-pixel tracker window. The camera objective therefore wraps as “Press F5 to toggle to” / “Control Mode”. The prefab also has its own UISwitchFont11401196. The prefab tracker is290×330 with `MissionTrackBg`; GameUI.cs:343 overrides its center to `(screenWidth/2−145,35)` in screen-center coordinates, so its right edge follows the screen right edge. This is a distinct window from the mission log opened byQ.

Do not replace the original short titles with instructional prose. Exact Quest_List→Translation titles:

| Mission IDs | Title |
|---|---|
|739|Camera|
|747,740,742|Move|
|741|Interaction|
|743,744,749|Items|
|745,746|Combat|
|719,748|Gathering and Foraging|
|757,721|Replicator|
|751,752,753|Digging and Building|
|756|Create|

Mission718 has MissionName0; it is the interface explanation step, not an invented titled mission. Existing C objective IDs and completion state remain authoritative; changing visual text must not change progression.

The checked-in [pe_training_ui_text.h](../../rebuild/generated/pe_training_ui_text.h) now exports all19 mission records and 24 ordered target descriptions with original localization IDs, mission type, target kind, item/monster identity and counter maximum. Regenerate with `python3 rebuild/tools/export_training_ui_text.py`; `--check` verifies both the header and its source-SHA JSON manifest without writing. Item/monster placeholders follow MissionManager.cs:2186–2575 exactly, including the source wording “Kill 1 lepus hares.” Event/area targets have maximum0, so the source719 herb objective has no appended5-item counter. Mission718 has empty title and no objective, and756 has the source title Create but no target. Runtime counts/completion remain host state, not generated text. Six tests cover original rows, native C lookups, quoted/UTF-8 text, external localization precedence, malformed source references and nonmutating stale-output checks.

`UIWndTutorialTip_N` localizes m_ContentID, sizes the background to label height+26, waits m_WaitHideTime (default5s) in **realtime**, fades using TweenAlpha duration, invokes an optional DeleteEvent, then destroys itself. The mission-button hint chains to the conversation-history hint. Quickbar and minimap explanations are transient callouts, not permanent objective cards. Exact source tip prefabs are retained in the layout JSON; their text IDs and layout remain separate from ordinary dialogue Scenario IDs.

## Inventory, storage and equipment

Source `UIItemPackageCtrl.cs:52–54,317–333,713–722` creates a **6-column ×7-row inventory**,42 cells per page, spaced55px horizontally and52px vertically. The original inventory window uses two `midbg` layers384×576, centered locally at(0,−10). At the runtime default its outer bounds are roughly x748..1132,y107..683 at1280×720; the visible screenshot ink is slightly inset. Categories include items, equipment, resources, armor and mission items; paging count derives from the selected package capacity. Do not treat the tutorial’s first few occupied slots as its full capacity.

`Show()` refreshes the selected package and emits `e_OnOpenPackage`. The mission743 listener must be active before that open event counts; an inventory auto-opened by an earlier storage interaction does not retrospectively complete743. The C training already guards this with a new open edge after lesson acceptance. Right-click dispatches use/equip/learn by actual item type, rather than completing a tutorial stage directly. Item406 is drawn using the source composite icon `shovel_wood,0,leftdown_script`: the shovel sprite plus the script corner badge, not a nonexistent generic `script` sprite. Preserve the two independently worn shovel instances and exclusive held equipment when changing the visual grid.

`UIWarehouse.cs:11–12,39–56,84–100` creates **6×5 cells**,30 per page, with the same55×52 spacing. Its `midbg` frame is384×460 at its local origin. Showing storage also shows player inventory; the default puts storage left of inventory. Categories and page changes synchronize to the package. Right-click first attempts `playerPackage.Add`; only success clears the source warehouse slot. Full inventory produces a warning and retains the source item. The training stack is916×5 and must transfer exactly once. A transfer button drawn without a real source-slot transaction is insufficient.

Equipment is not merely a second inventory category. `UIPlayerInfoCtrl.Show():617–632` and the menu/HUD character-open handlers show character information without automatically opening the inventory; storage explicitly opens inventory. Opening both together in C is a convenience policy, not this source behavior. The C host now has independent `character_open` state and `pe_training_open_character/close_character`: opening C does not open the bag or emit the mission743 inventory event. Unequip accepts either inventory or character UI; equip still requires an open inventory. Closed/dead/dialogue requests are rejected, and unequip preserves unique knife/axe ownership and both shovel instance IDs/durabilities. Character UI gates new herb/tree/dig/draw/build actions while already paid crafting and health timers continue. Optimized core regressions pass; graphical character-panel verification is separate. `UIPlayerInfoCtrl.cs:253–283` creates ten actual equipment slots in two columns,235px apart, five rows58px apart, with masks1<<i. Their icons are helmet, coat, gloves, battery, arms, back, pants, shoes, glass and shield (`Icons_dun`). Character preview uses PeViewStudio’s cloned current appearance/render target. Knife, Safety Axe and shovel share their source equipment position and replace one another; showing them simultaneously equipped would contradict the core. The C implementation now renders the independent source-shaped character panel and recovered-player preview; dragging that preview rotates it. It shows the actual equipped weapon in its slot, while the bag only shows unequipped instances. Source armor editing, equipment drag masks and the complete character statistics remain separate features.

## Replicator

`CompoundWnd.prefab` is an880×480 centered `bigbg` window with two color layers. It has a category column, search/list area, dependency graph, selected result, quantity controls, material requirements and progress/cancel controls. It is not the C tutorial’s initial480×300 single-recipe dialog. `UICompoundWndControl.cs` categories cover all, weapons, equipment, tools, turrets, consumables, resources, parts, decoration and creations. Tutorial recipe1 is Wood Shovel, but the source browser is wider than that lesson.

The bottom quantity is a **product count**, converted to batch count by dividing by recipe productItemCount. Increment/decrement use that output unit; manual input is clamped, rounded to an output multiple and capped at9999. Material availability and package capacity gate creation. Clicking the compound button while running calls CancelReplicate; changing quantity while running is rejected. Current training’s single3s/10wood→1shovel job preserves conservation and lesson grants, but it does not implement the full source recipe graph, arbitrary quantities or cancellation UI. The broader survival-slice replicator has different capabilities; do not conflate it with this tutorial host.

Y is the original replicator key; I opens inventory, C character/equipment, Q mission log (`PeInputSettings.cs` and PeInput enum). Opening a visual panel must call the existing training APIs so script learning, recipe availability, capacity and mission progression remain enforced. Avoid spawning a second independently writable inventory model inside the UI.

## Tutorial minimap

The training room has a dedicated texture override, not just the general terrain camera view. `Assets/Scripts/Jaluca/EpsilonIndi/EpsilonMiniMap.cs:8–14` assigns `trainingRoomMinimap` to the minimap material and hides both coordinate and time labels. The source texture is `TrainingRoomMinimap.png`; its binding is verified through EpsilonIndi.prefab, and the C minimap now uses the recovered bitmap, crop/shader and marker transform. Rendering an unrelated procedural map would not match the tutorial.

General `UIMinMapCtrl` supplies the surrounding chrome, marker scaling and UV crop. GameMain serialized mapSize is190×200, bounded150..400. Camera setup is top-down, orthographicSize128; the common UV rectangle is `(dx/248/aspect+.25,dz/248+.25,.5/aspect,.5)`. The marker rotates around screen Z by minus the player world yaw, and tutorial monster NPC markers are suppressed (`AddOrRemoveLable`). The static training texture override must be accounted for before reusing this generic camera logic. The C map currently draws the room image, player orientation, nine NPC icons and source frame. Full mission-marker behavior, interactive resizing, off-map arrows, world-map transitions and weather controls remain unsupported.

## Integration and verification boundaries

The atlas/font, layout, portraits, HUD effects, mission window, static-room minimap, inventory/character/Replicator and Pause are integrated through shared UI owners. [Shared UI](SHARED_UI.md) owns drawing/layout/input invariants and Settings; hosts supply mode snapshots. Tutorial supports Juice assignment/use in slot1; the other eight drawn quickslots do not imply full sandbox shortcut behavior. Keep source dialogue/transaction semantics independent of rendering.

Native15,000-frame UI fixture reached756; [result](../reference/rebuilt-ui-result.txt), [storage](../reference/rebuilt-ui-storage.png), [quickbar](../reference/rebuilt-ui-quickbar.png), [crafting](../reference/rebuilt-ui-crafting.png), [completion](../reference/rebuilt-ui-complete.png). [PLAYTEST](../PLAYTEST.md) owns observed mode/size/input limits. Both hosts' movable windows and shared visuals supersede the early fixed-window UI checkpoint.

Visual changes require paired normal/small-size checks: draw and hit-test identical layout, preserve right/center anchors, wrapping, modal press/release ownership, stale-click guards and inventory conservation. No grants or quest transitions from redraw. Other Tutorial storage-category behavior remains unsupported unless separately qualified.
