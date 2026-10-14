# Planet Explorers C rebuild

Knife attacks now blend back to idle/walking over150ms after natural completion. Restart the rebuild and repeat one click, then wait, standing and walking forward; damage and stamina timing are unchanged. [Scope and retest](../docs/audit/KNIFE_RECOVERY.md).

The staged reaction animation review now has a private-display fixture: `make -C rebuild reaction-render-capture REACTION_CAPTURE_OUTPUT=build/reaction-review-01`. Choose a fresh output directory. It renders25 isolated leaves with sword/shield props and defence wires; it does not enable live combat. [Images, prerequisites and scope](../docs/audit/REACTION_PREVIEW.md).

Building fabric/roof transparency now uses recovered cutout and two-sided rules; sandbox NPC names hide behind walls. Restart the rebuilt game to see both corrections. After replacing settlement OBJ assets, regenerate their sidecars using the [building material instructions](../docs/audit/BUILDING_MATERIALS.md#installation-and-verification).

Hairline/eye transparency now uses recovered source alpha/culling rules in the shared actor renderer. The local sidecars are installed; restart the rebuilt game to see the correction. After replacing actor assets, regenerate their metadata using the [actor material recovery instructions](../docs/audit/ACTOR_MATERIALS.md#export-and-installation).

**Unblock testing:** in Adventure, open **Esc → Settings → Debug tools: ON**. Choose **Give 5 Herbal Drinks** to finish the current Naoko turn-in, **Combat supplies + heal**, or **Skip to hare hunt** to equip the Knife and go straight to combat. Back/Esc returns to Pause; Resume continues play. Nothing saves automatically. The toggle resets when you leave the host. Sword/Shield remain unavailable; these tools use the supported Knife. [Scope and safeguards](../docs/audit/DEBUG_TOOLS.md).

Hunt hares now drop source-derived loot. Walk within four metres while alive to collect it automatically; a full inventory leaves complete stacks on the ground. Uncollected drops survive saving/loading and hunt completion. Saves are now **PECSv22**; older saves load with no retroactive drops. Nearby ground icons are a bounded presentation adapter. [Loot rules and limits](../docs/audit/HUNT_LOOT.md).

**Esc → Settings** in either game mode, or Main Menu → Settings, has **Camera-facing attacks**, OFF by default. Turn it ON to aim free-mouse third-person attacks along the camera in both sandbox and tutorial. Change it while paused, then use Back/Esc to return to Pause and Resume; it applies immediately to subsequent attacks without leaving the session. Settings save automatically outside world saves; restart any already-running older executable to use the update.

With the setting OFF, Adventure blade attacks use movement direction (or current body facing while idle) and bounded source-based nearby-hare targeting. With it ON, the camera supplies the requested attack direction; nearby-target assistance still applies and freezes for each stage. Camera orientation itself stays independent. See [combat scope and adapters](../docs/audit/MELEE_TARGET_FACING.md). Wood Sword/Shield activation remains gated on qualified blocking presentation.

Conversation history is available from the speech-bubble icon on the minimap in both modes. It keeps the latest20 unique messages, supports scrolling/scrollbar dragging and Clear, and persists with sandbox saves (PECSv21). Older saves start with an empty log; tutorial history lasts for that session. [Contract and controls](../docs/audit/CONVERSATION_HISTORY.md). The [Adventure camera update](../docs/audit/CAMERA_ALIGNMENT.md) integrates camera-only wall queries and shares the rendered pose with cursor targeting; its close-rendering and timing adapters are documented.

## Review batch: shared tutorial and sandbox UI

Both modes now use one normal HUD, minimap border/decorations and quest tracker renderer. Shared text, inventory/dialogue frames and menu shortcuts follow the same owners; mission/map contents still reflect each mode. Click health for Character or the minimap mission icon for Missions. Movable panels retain header dragging and viewport clamping. [Ownership and paired checks](../docs/audit/SHARED_UI.md) are required for future common UI changes.

Run `./rebuild/build/planet_explorers` from the repository root and select Tutorial or Sandbox in the main menu. Compare both at the same size, then shrink the window: inspect the HUD, minimap and wrapped quest title; drag Inventory/Character and test their close buttons. A modal's closing mouse press must not start a world action while held. This manual review is ready; ordinary interactive acceptance remains separate from automated captures.

```sh
make -C rebuild ui-shared-test sandbox-ui-test
make -C rebuild ui-shared-render-capture
```

The capture target stages both real UI renderers at 1280×720, 640×480 and 320×240, compares matching HUD pixels and exports images under `rebuild/build/ui-shared-captures`. It uses the qualified private high display and never injects desktop input. Fixtures do not play either game or alter saves.

Camera-only collision and consistent pointer projection are now integrated with the documented native adapters. A useful later manual check is orbiting close to terrain/buildings in all F5 modes while checking cursor targeting. Town Defense9077 remains gated.

The latest review batch adds Restoration Bandage924 use, typed shop quantities and item-detail tooltips. The existing camp repair service and these changes can be reviewed together using the checklist below. Saves remain PECSv20. [Ordinary hunt acceptance](../docs/audit/HUNT_ORDINARY_ACCEPTANCE.md) now passes earned four-kill combat, partial reload and the return/reward flow; it used an isolated native display without requiring user play. A source-derived camera geometry evaluator is also tested offline; the live camera still uses its existing geometry adapter.

## Review batch: shop details, bandages and repair

Run `make -C rebuild`, then launch `rebuild/build/planet_explorers` from the repository root. To use an existing native save without overwriting it, copy it to a review path and launch with `--load PATH_TO_REVIEW_COPY`; F9 saves back to that copy. These are C rebuild saves, not original Steam saves.

1. **Shop quantities:** select goods in Buy, Sell or Buyback, click the quantity field, and type an amount. Enter or clicking outside accepts the edit; a separate Enter/Buy/Sell action trades. Escape cancels the edit. Try a stock-limited amount and resize the window. The field clamps to available quantity; affordability and capacity are checked at trade time.
2. **Item details:** hover bag cells, equipped items, shop rows and repair items. Check condition, learned-script labels and relevant prices/material quotes. Newly purchased tools show full condition; owned worn tools retain their actual condition. Displayed durability now uses source units; this does not repair or alter items.
3. **Bandages:** Xing Zhang in Annot already sells Restoration Bandages and their script. A bandage restores200 HP. It shares a ten-second cooldown with Portable Medpacks; Herbal Juice remains separate. Try a shortcut while walking, then a blocked retry or the other medicine. Movement and NPC simulation should continue. Bandages may be consumed at full health, matching the source.
4. **Camp repair:** with an eligible worn tool in the bag, approach the starting camp's repair machine and use E/short RMB. Insert the item, review materials, confirm Start, then close the window. The60-second job continues during ordinary play; pause/focus loss suspends it. Save outside the modal, reload, and check retained progress and exact returned item. Wear on currently equipped starter clothing, Knife33 and Safety Axe1527 is outside this service's supported scope.

No fresh original-game session is needed for this checklist. Automated checks cover the transactions and saved ownership; final native layout and ordinary gameplay acceptance remain separate. Detailed boundaries: [shop/details](../docs/audit/SHOP_QUANTITY_AND_ITEM_DETAILS.md), [bandages](../docs/audit/SANDBOX_BANDAGE.md), [repair](../docs/audit/CAMP_REPAIR.md), and [disconnected camera geometry](../docs/audit/CAMERA_GEOMETRY.md).

Adventure camera now uses source-derived activity-space zoom targets and obstruction recovery, keeping the existing native framing/collision adapter. [Camera contract](../docs/audit/CAMERA_ENVIRONMENT.md) records automated host checks and remaining visual limits. [Hare pounce](../docs/audit/HARE_POUNCE_PREPARATION.md) and [defense-wave planning](../docs/audit/TOWN_DEFENSE_SPAWN.md) are isolated foundations; they do not enable new combat or9077.

Adventure inventory now has four independent420-cell categories, with42 visible cells per page and ten pages per category. Split stacks, move or exchange cells, merge with Shift+drop, and consolidate/order a category with Sort. Delete arms item selection and opens whole-stack confirmation; use Split first for partial disposal. Legacy and tutorial inventories retain32 cells. [Inventory ownership, controls and migration](../docs/audit/SANDBOX_INVENTORY.md) describes the source rules and native conveniences.

The fixed vegetation patch now uses recovered tree hardness and resource production, including herb fallback items and optional seeds. Unit-scale Tree48 yields16 Wood and20 combined Resin/Rubber at the supported axe bonuses. Placement remains fixed; gathering is still immediate without the original Gather animation. [Harvesting contract](../docs/audit/SANDBOX_HARVESTING.md) distinguishes source calculations from the saved native RNG and presentation adapters. These changes require no new original-game capture; consolidated visual acceptance remains pending.

Vendor purchases now credit the eight archived NPC wallets, with source noon refill rules and PECSv17 persistence. [Wallet contract and migration](../docs/audit/TOWN_WALLETS.md) records the bounded old-save initialization; selling and session buyback are also supported; see the controls below.

Offline sword policy checks: `make -C rebuild sword-policy-test` exercises source selection and recovered running collision/contact. `make -C rebuild sword-source-check` verifies prefab-derived metadata and exporter cases with UnityPy (`UNITY_PYTHON` overrides the local interpreter). The subsequent [walking-attack integration](../docs/audit/SANDBOX_WALKING_KNIFE.md) enables ordinary grounded attacks with the installed source-root group. Run `make -C rebuild sword-walking-test sandbox-sword-motion-host-test` for its focused checks.

Adventure death now offers **1. Revive (1 shot)** or **2. Load saved game**, with clickable buttons and Escape for Pause. Revival uses one of the existing Revival Shots937, restores33.3% HP/full stamina and preserves earned hunt progress and surviving creatures. Ordinary native use now passes; full death/get-up animation fidelity remains unqualified. Wait for the revival panel to close before pressing2 to heal: while it is open,2 means Load. See [revival scope](../docs/audit/SANDBOX_REVIVAL.md).

A new C11 runtime built from the supplied Unity project's source and data. Its sandbox runs exploration, tool-based mining, crafting, construction, and persistence, and can import a bounded crop of an original terrain file. A separate training mode uses the recovered original room for the tutorial sequence through combat, gathering, tree cutting, crafting and mining. Construction and the receiver puzzle also pass the 15,000-frame graphical sequence, ending at the unsupported creation lesson 756. The sandbox includes a bounded six-hare encounter with real HP/contact and saved kill progress. The earlier return increment added automatic hunt-return rewards, resumable dialogue, PECSv12 and free-mouse third person alongside captured third person and first person. Full optimized/content/CLI and ASan/UBSan/leak checks pass; ordinary-interpreter optional UnityPy skips are not passes. Native checks verify the camera controls and staged return dialogue/save-resume with no repeated rewards. That return fixture used staged four-kill progress. The later [October7 acceptance](../docs/audit/HUNT_ORDINARY_ACCEPTANCE.md) separately earns all four kills and verifies partial reload and ordinary return. Earlier physical checks verify incoming damage/death/load and one outgoing knife contact. The Unity project remains unchanged as a reference.

Starter clothing now supplies the missing12 defense through three owned, wearable item instances, persisted in PECSv14. Older Adventure saves receive intact starter clothing after validation. The Character panel shows their icons and durability. Full checks and a physical damage/save/reload inspection pass; this does not establish four-kill survival or sword/shield support. See [outfit contract and evidence](../docs/audit/STARTER_OUTFIT.md).

This is a working reconstruction of selected systems, not a complete reproduction of the game. The [replication specification](../docs/REPLICATION.md), [Block45 audit](../docs/audit/BLOCKS.md), and [reference-runtime investigation](../docs/REFERENCE_RUNTIME.md) distinguish recovered behavior from missing systems and temporary substitutes. The [sandbox alignment audit](../docs/audit/SANDBOX_ALIGNMENT.md) records live Adventure/Build observations and the next implementation milestones; the controls and current implementation are described below.

The earlier consumable increment adds Portable Medpack925 use and independent saved cooldown, PECSv13 and a Tab/F5 camera correction. Full optimized/content/CLI and relevant sanitizer checks pass. Physical quickslot use, both cooldowns across save/load and Tab/F5/modal precedence pass; PLAYTEST records the staged starting fixture and optional asset skips. Earlier camera/return physical evidence retains its recorded scope. Town Defense9077 is still disabled: only guarded source metadata and a headless scheduling foundation exist. See [consumables](../docs/audit/SANDBOX_CONSUMABLES.md) and [defense scope](../docs/audit/ADVENTURE_TOWN_DEFENSE.md).

The preceding directional asset update adds left/right/back walk and run to the shared locomotion selector. Twelve runtime states use source clips from a coherent twenty-clip body/attachment group; the defence actor now has twenty-nine clips. Full optimized/content/CLI checks and focused asset sanitizers pass. The ordinary full run retains optional UnityPy skips; directional asset checks passed separately. Native A/D/S run/walk exercised all six new clips (mask4037), with drawn Knife33 and exit0. Full15,000-frame tutorial progression to756 and full ASan/UBSan with leak detection passed. See [PLAYTEST](../docs/PLAYTEST.md) for visibility limits; a separate captured-camera tutorial run exercises all six new directions (mask4033) and exits0. Moving attacks were unavailable at that preceding checkpoint; the current running increment is documented below. [Recovery and installation](../docs/audit/DIRECTIONAL_PLAYER.md) records the exact source branches and adapter limits.

The latest movement correction separates body heading from camera orbit: free third person turns toward actual travel and holds its heading when stopped; captured modes face the camera for side/back steps. Both hosts smooth locomotion pose weights with a 0.12s time constant, sharing the pose with attachments; sandbox Torch mounts and incoming player defence follow that same blend and body yaw. Tool/melee actions retain their exact unblended clocks and facing precedence. These are bounded native adapters, not the full source Animator graph. The corrected build passes full optimized build/tests and core ASan/UBSan with leak detection; the ordinary Python run retains19 optional skips, which are not passes. Physical sandbox free-mode turning/stopped heading and sandbox/tutorial captured transitions are checked. A new15,000-frame tutorial regression exits0 at mission756 with nine NPCs,19 construction additions/one removal/score50, and eight melee starts/24 contacts/hareHP0. These checks do not establish source blend parity, camera-quality acceptance or exhaustive equipment/action-facing coverage. See [facing and blend evidence](../docs/audit/PLAYER_STRAFING.md).

Blade root motion is now applied in the original controller's order (two-tick lag,10 m/s hold, cancelled tail); see [the lunge motor](../docs/audit/SWORD_LUNGE_MOTOR.md) and `python3 rebuild/tools/run_tests.py -k sword_motor -k running_attack_pipeline`.

The latest running-knife increment adds grounded female third-person Knife33 stage408 with a qualified captured root curve, shared collision/pose/contact ordering and existing Attack7–9 continuations. Matching body/knife/defence assets are installed with backups. Original live03 confirms the running branch and thresholds4.5/7; its software-rendered trace is too coarse for exact motor timing. Full optimized core and ASan/UBSan with leak detection pass, plus44 focused Python tests with0 skips. The broader test suite completed as a composed pass after a stale Torch fixture correction, retaining19 optional UnityPy skips; current content and23 CLI checks pass. The latest15,000-frame tutorial regression also passes at mission756 after a bounded attachment-count compatibility correction; tutorial attacks remain stationary. Native physical checks now record three running starts and visible lunges; a separate hunt run records one running start plus continuation, six aggregate outgoing/eight incoming contacts and player death, with no kills. Both sessions exited0. These counts do not isolate contacts to stage408. Sprint, airborne, water/first-person attacks and full Unity motor/blend parity remain unsupported. See [running implementation](../docs/audit/SANDBOX_RUNNING_KNIFE.md).

In the Adventure sandbox, equip Knife33 and draw it with a fresh world click. Run on clear ground, then click again while actual speed is above4.5 and at most7m/s; ordinary running is supported, sprinting is not. The attack moves through the existing collision path and shares its clock with blade and defence. Walking attacks are also supported through speed4.5: press Z to toggle walking, keep moving forward/side/back, and click to attack. The attack owns movement until it ends; holding WASD does not cancel it or add extra travel. Ordinary and later combo stages now use source-derived root motion. The required local body/knife/defence and running-motion assets are installed on this machine. This does not enable running attacks in the tutorial.

## Build and run

The executable finds `reference-assets` beside itself, so moving the checkout does not change its default asset lookup. Use `--assets DIR` for another asset tree. Source exporters label repository-owned inputs relatively; historical absolute records resolve only with their exact recorded hashes. See [checkout relocation](../docs/audit/PORTABLE_PATHS.md). UnityPy console launchers also follow their interpreter after a move; repair an existing environment without reinstalling packages with `python3 rebuild/tools/setup_unity_tools.py --repair`.

Sandbox blade and hare attacks now use sampled capsule history between active poses, with source collider centers recovered offline. This changes contact opportunities in both directions; it is not a damage/stat adjustment. Numerical and headless integration checks are recorded in the [capsule-history audit](../docs/audit/MELEE_CAPSULE_HISTORY.md). The native fixed-tick schedule still differs from Unity's animation/physics ordering; physical combat acceptance remains pending.

Adventure stamina now recovers in source-timed pulses. Comfort gradually decreases and increases the delay after stamina use; the Character panel shows its current value. Comfort survives saves. The raw rules and save behavior are tested headlessly; an on-screen recovery comparison remains pending. See [stamina and comfort](../docs/audit/STAMINA_RECOVERY.md).

Adventure Sandbox now includes the source low-health recovery rule: below 33% HP, going at least four seconds without a hit allows periodic recovery of 1% maximum HP. Recovery stops at or above the threshold and cannot revive a dead player. This increment has source and headless coverage; an on-screen retreat/recovery check remains pending. See [recovery lifecycle and timing limits](../docs/audit/PASSIVE_RECOVERY.md).

On the current machine, the packaged raylib uses X11 through **Xwayland inside the Wayland desktop**. Seeing an X11 backend does not mean the desktop switched to Xorg. Normal launches inherit your session environment; do not point them at a private test display. The [October7 test-display collision](../docs/audit/PRIVATE_DISPLAY_COLLISION.md) is repaired and the automated launchers now reserve checked high display numbers. Native Wayland rendering is not claimed by this build.

Requirements: a C11 compiler, Make, pkg-config, raylib and LZ4 development files, and a graphical display/OpenGL driver. Python 3 with its standard-library SQLite module is needed for content regeneration and exporter tests. Headless core tests require LZ4 but not raylib or a display. Persistence currently uses POSIX APIs; Linux is the development platform.

From the repository root:

```sh
make -C rebuild
./rebuild/build/planet_explorers
```

The main menu offers **New Sandbox**, **Continue Sandbox**, and **Tutorial**. Both gameplay modes return through **Esc → Main Menu**. Continue resumes the in-memory sandbox after a tutorial visit, or loads your save after restarting the program. New Sandbox confirms before discarding an active session. The menu uses recovered logo/fonts; its background is a new approximation.

New Sandbox starts the bounded **Adventure** reconstruction: source starter items, 500 currency, empty learned formulas and Westina Nangle's welcome. Her introduction opens automatically nearby; Enter or a dialogue click advances its nine source pages. Esc interrupts it; speak to Westina again to resume. The final reply starts Arrival. Approach **Eitai Takayama**, the recovered supplier near original world X−599/Z341 (downhill from the starting pad). Arrival completes automatically within the source5m XZ target radius; aim at her and press E or right mouse to open the NPC choice window. Select the supplies question, then use Enter, a dialogue click or the arrow through source pages4090–4094. Only acknowledgment of the final page grants the one-time package; closing early grants nothing, and a full inventory leaves the final page available for retry. Q shows current progression.

This supplier is the archived role2 identity, entity20016/RandomNpc22. The reference population now includes25 saved individuals, placed only where their archived coordinates have supported terrain. Cherish Mercer, Andre Dale and Virginia Cannan offer **Shop** through the NPC menu. Annot adds Xing Zhang (supplies), Sherley Hansen (furniture), Abe Okorie (groceries), Ntombifikile Petros (weapons) and Tammana Chu (armor). These eight shops expose284 purchase offers. Scroll the catalog, select an item, choose quantity, then Buy. Currency, inventory capacity and finite stock are checked together; stock and refresh clocks survive saves. The shop also exposes source-priced Sell and transient Buyback lists; see [selling and buyback](../docs/audit/TOWN_SELL_BUYBACK.md). Other residents may have only a close choice; generic dialogue or recruitment is not invented. [Population](../docs/audit/ADVENTURE_POPULATION.md) and [town interactions](../docs/audit/ADVENTURE_TOWN_INTERACTIONS.md) define this bounded scope.

With the expanded route installed, accepted supplies lead to Annot at source P:-284,134. Entering its strict15m XZ radius completes9032 without another item grant. Speak to Naoko Ueda for source question4095/reply4096 about the delayed supplies. After the reply, explicitly accept her request to start medicine mission9072 and receive the Herbal Drink Script. Refusing grants nothing. Learn the script from inventory, buy ingredients from Xing and use Y to craft five Herbal Drinks. Return to Naoko and acknowledge her final reply to exchange the five drinks for ten Portable Medpacks and their script. A full inventory leaves acceptance or completion available for retry; learning recipe books can free space in their source inventory category. The current catalog contains341 item definitions and108 formulas; [follow-up recipe definitions](../docs/audit/ADVENTURE_FOLLOWUP_CATALOG.md) do not activate their future missions or colony machines. Original refusal/retry/acceptance and script learning were verified. Native physical acceptance, final delivery and save/reload passed; Tammana’s final catalog rows and a Sherley purchase were exercised. Ingredient purchases and crafting five drinks were verified separately through ordinary game APIs at the normal balance, costing105 currency. These are bounded checks, not physical coverage of every shop; see the [playtests](../docs/PLAYTEST.md) and [medicine audit](../docs/audit/ADVENTURE_MEDICINE.md). After medicine, Naoko now offers the source9074 conversation and its accept/refuse choices. Accepting Bug Hunt now uses the original save4 discovery4524 destination at source P:-349,233, provided the installed route and its collision support are available. Entering the strict15m XZ radius now prepares the six-hare9126 encounter when its recovered assets and supported geometry are available. After four real kills, return within strict5m XZ of source P:-266,136. Quest9127 automatically grants one each of scripts592–595, then queues Naoko’s4105/4106 ending pages. A full inventory leaves the complete reward transaction retryable; this is an explicit native capacity adapter. Esc closes the pages without losing progress; speak to Naoko to resume, or reload a saved pending page. The next quest9077, skill-tree buff and plot4 NPC movement remain unavailable. A staged native fixture physically verifies both pending pages and completed reporting across save/load with exactly one copy of each reward; it does not verify real four-kill progression. Original screenshots show the reward toast before4105 acknowledgment and4106 leading to9077 opening. See the [return audit](../docs/audit/ADVENTURE_HUNT_RETURN.md). Original acceptance/destination/arrival and native headless acceptance/arrival/save checks are verified. A separate physical native run accepted the quest, walked to its target and reloaded the encounter-pending state. See the [post-medicine audit](../docs/audit/ADVENTURE_POST_MEDICINE.md).

I opens inventory; right-click an item to use a consumable, learn a recipe book or equip a supported tool. Learn **Wood Shovel Script** from the supplies, then open Y, select Wood Shovel and replicate one for ten wood. C opens the character and equipment panel. Drag inventory items to the nine quickslots; 1–9 activate those shortcuts. Scanner upgrades do not unlock recipes. HUD, inventory, character, mission and Replicator panels use the recovered training UI assets with real sandbox state; the minimap shows this world.

WASD moves, Shift sprints, Space jumps, Z toggles walking and middle-click toggles autorun. Sandbox starts in **free-mouse third person**: move the visible pointer normally and hold/drag right mouse to look. Looking begins after3px motion or.25s hold; a short stationary right-click interacts or stows. **F5** cycles free third person → captured mouse-look third person → first person. In the captured modes, mouse movement looks; the wheel adjusts third-person distance (1.8–10 m normally, up to20m in construction). In Adventure, equip Wood Shovel12, Wood Pickaxe19, Wood Axe26 or Safety Axe1527, stand still on the ground in third person, then hold left mouse to draw and use it. Digging/cutting transactions run at the recovered source clip events, and the visible body/tool share that exact action clock. Release finishes the action; R or right mouse stows and cancels when not interacting with the supplier. Aim at nearby terrain for the shovel/pickaxe, or face the same tree selected by the camera for an axe. NPC menus, shop purchases and dialogue consume world/tool input through button release, including closing or interrupted frames; a fresh press is required to act in the world. Legacy mode retains the earlier action path.

Torch10 is the first usable purchased deployable. Buy it from Sherley, open I and right-click it to equip. A left click draws it; R stows it. Press P to enter placement, aim at a surface within10m and left-click to place; P or right-click cancels placement. Aim at a placed Torch and press E or right-click for **Get** and **Turn**. Get consumes the Torch and grants one Charcoal1001; it does not return a Torch. Get shows a cancellable pickup progress bar and commits strictly after2 seconds of active display-frame time, matching the source tick truncation. A full inventory leaves it available for retry. Placement preserves the exact instance and wear, and deployed Torches are saved. The required recovered Torch and held/stowed asset groups must be installed; there is no invented replacement model.

Held or placed Torches show the recovered model/flame and illuminate nearby terrain, using at most eight nearby lights. This is a bounded rendering adapter: buildings and other models are not lit by this terrain shader; shadows, water extinguishing, Torch attacks and full Sword animation transitions remain unsupported. Deployment is Adventure-only, capped at64 objects; source trigger capsules do not block walking. Turn changes are preserved across native saves, while original reload rotation needs verification. Physical native checks exercised equip/draw, placement, Turn, save/reload and delayed Get→Charcoal. Held presentation still uses Normal locomotion rather than the original Sword style; the low, forward-held model can be obscured by the body from behind. See [deployment](../docs/audit/SANDBOX_DEPLOYABLES.md) and [equipment](../docs/audit/SANDBOX_PURCHASED_ITEMS.md).

B toggles cursor-based construction. In construction, Ctrl+Z undoes and Ctrl+X redoes the last five cube/ramp edits; the toolbar also has Undo/Redo buttons. Replay uses current materials and source rounding, so undo may refund less than placement cost. History survives leaving B mode, but clears on load/new game/world replacement.  Left mouse places at the pointer; Alt + left mouse removes, and holding right mouse turns the camera. The recovered toolbar has a bounded palette: 1 cube, 2 dirt, 3 stone, 4 wood, 5 ramp, 6 fence and 7 ladder. These slots are separate from normal quickslots. V selects cube/ramp, T rotates, G switches one-cell/four-cell brushes, and X also removes the selected brush. Four half-meter cells built together cost one resource and removed together refund one; a single cell costs one and refunds zero. Toolbar and panel clicks consume world input; canvas clicks remain available for construction. Creation saving and selection deletion are disabled. Tab toggles the cursor override outside construction (free-mouse mode already shows it); F5 can leave that override and clears it when no modal or owned pointer gesture blocks the mode change; panels allow gravity and crafting to continue. Esc pauses simulation.

The drawn wood shovel shows a cyan wire around the current source-style target sample. Construction shows a bounded one-cell/four-cell cube or ramp candidate for dirt, stone and wood, colored green or red by the same geometry, reach, occupancy and full inventory-cost checks used by the click transaction. Previewing is read-only; clicking still revalidates and commits the normal inventory/world transaction. Furniture and removal previews are not implemented. These indicators are native presentation aids, not recovered original indicator shaders.

In both Tutorial and Sandbox, **drag a panel by its title bar** to move Inventory, Character, Storage, Replicator or Mission. Buttons and item slots follow the panel; resizing keeps it on-screen. Positions last for the current gameplay session and reset when that host is reopened. The minimap and quest tracker remain anchored, with quest headings below the full map frame.

Knife33 supports grounded female third-person stationary, walking and ordinary-running attacks in Adventure. Equip it, click once to draw, release, then click again to swing; fresh clicks during the combo window chain attacks. Press Z to toggle walking for ordinary starts through speed4.5, or run without Shift for the branch above4.5 through7m/s. The admitted attack owns root movement; held WASD does not cancel it or add displacement. Jumping, opening a panel, stowing, death or changing camera mode cancels it. Sprint, airborne and first-person attacks remain unsupported. The six-hare host connects incoming/outgoing damage and saved kill progress; inventory/modal UI pauses combat. Death offers Revival Shot or checkpoint loading. Reload recreates the pinned full-HP cohort while retaining earned kills. Pounce, loot and full source AI remain unsupported. Four-kill ordinary completion and partial-progress reload remain pending; use the [focused hunt checklist](../docs/audit/HUNT_ACCEPTANCE.md). See [walking attacks](../docs/audit/SANDBOX_WALKING_KNIFE.md) and [hunt combat](../docs/audit/ADVENTURE_HUNT_COMBAT.md).

Use Portable Medpack925 from inventory or an assigned quickslot to restore350 HP up to the maximum. It consumes one even at full HP and has its own10-second cooldown, independent of Herbal Juice916 and its ongoing healing buff. The quickbar shows the applicable cooldown; paused simulation freezes it, and save/load preserves it. The medicine quest still grants its rewards only on final4100 acknowledgment, as requested.

Conversations in both hosts label visible responses **1–9**; press the matching top-row or keypad digit. A single page uses **1. Continue**. NPC menus number their currently visible options, so closing may be1 or2; absent response numbers do nothing while the menu is open. Conversation selection consumes the frame without using a quickslot or construction shortcut. Once the interaction closes, digits resume their normal world shortcuts. **Esc from a shop closes the whole interaction**, not just the shop page. Tutorial welcome/Tips and Naoko/Tammana response selection were physically checked at1280×720; keypad and transaction edge cases have focused headless coverage. See [numbered responses](../docs/audit/NUMBERED_CONVERSATIONS.md).

F9 saves and F10 loads outside modal interactions, including repair and stack dialogs. Close the repair window before saving; its item/job remains in the machine. Current saves use **PECSv20**: literal v19 is retained, followed by32 bytes for the repair machine key, reserved item/count/instance/wear, running flag and64-bit remaining100000-Hz ticks. Older saves begin with an empty machine. A resumed job retains its exact item and remaining time without consuming materials again or awarding offline progress. V19 retains the v18 payload layout followed by26888 bytes for the authoritative full inventory. Valid older Adventure inventories migrate into source categories in stable original slot order, preserving counts, exact identities/wear, equipment and quickslots; Legacy remains32 cells. V18 adds648 bytes for the fixed vegetation patch and herb depletion to the literal v17 payload; older saves retain their world with no automatically added plants. V17 adds100 bytes for eight identified vendor wallets and refill latches to the literal v16 payload; older Adventure saves initialize the archived starting wallets. V16 adds raw comfort to the literal v15 payload; pre-v16 saves default to500. It also preserves the separate shield reference, starter clothing identities/wear, full world dimensions, acknowledged welcome pages, Annot completion, medicine state, vendor stocks, hunt travel/kill/return-dialogue progress, deployed Torch instances and independent medpack cooldown; interrupt with Esc before saving. V14 retains literal v13 and appends36 bytes for three worn clothing records. Older valid Adventure saves initialize their previously visual outfit intact once; this is a compatibility adapter, not recovered historical wear. Current-version reload never repairs clothing. V13 appends one finite0..10 four-byte cooldown float to the literal v12 payload; pre-v13 infers0. V12 retains the literal v11 payload with no appended bytes; its existing hunt state distinguishes pending4105, pending4106 and completed reporting. Older versions reject those new states. V11 retains the literal v10 payload and appends the four-kill counter; pre-v11 defaults to0. V10 retains the complete v9 payload, then appends hunt state/destination and64 fixed deployment records. Pre-v10 saves keep their existing state and begin with no hunt or deployed objects; pre-v9 saves also begin with no accepted medicine quest or Annot stock. The original seventeen v8 stock indices remain stable. Loading an unfinished welcome resumes nearby. Older v4/v5 Adventure saves retain their existing quest state and treat the welcome as completed. There is no automatic save on exit. By default the file is `planet_explorers.pesave` in the working directory; `--load` also becomes the save destination unless `--save` is specified.

Recovered runtime assets default to `rebuild/build/reference-assets`; use `--assets DIR` to override them. `--sandbox` bypasses the main menu. Rebuild save versions 1–3 load as Legacy mode, retaining their previous movement rules and unlocked recipes; v4 Adventure retains its mode and progression through the tree-collision migration described below. New Adventure uses rendered-mesh and live-tree capsule collision and learned formulas. Tutorial progress resets when leaving that mode.

```sh
./rebuild/build/planet_explorers --sandbox --save /tmp/exploration.pesave
./rebuild/build/planet_explorers --load /tmp/exploration.pesave
```

### Starting settlement

New Sandbox prefers the installed416×64×416 reference route at source(-640,0,64), including the starting settlement and Annot. If no route terrain is installed, it uses the earlier64×32×128 settlement. `--seed` does not regenerate either recovered fixture. Existing saves and `--terrain` imports keep their own worlds. Missing required building geometry fails closed. Both crops remain finite; procedural generation, streaming, machine interactions and broad NPC behavior remain unfinished.

The six starting buildings use `export_adventure_settlement_assets.py`; the earlier terrain uses `export_adventure_terrain.py --capture PATH`. The expanded route requires complete original observations and a fresh isolated save with Annot's buildings loaded:

```sh
# Inspect partial terrain coverage; this does not install missing chunks.
python3 rebuild/tools/export_adventure_route.py --capture /absolute/capture.pevx --coverage
# Install only a complete, validated observation and its source town names.
python3 rebuild/tools/export_adventure_route.py --capture /absolute/capture.pevx
rebuild/build/unity-tools/bin/python rebuild/tools/export_adventure_route_assets.py \
  --save-dir /absolute/isolated-profile/PlanetExplorers/GameSave/save1
```

Read [route capture/installation](../docs/audit/ADVENTURE_ROUTE.md) before using `reference_terrain_route.py --launch`; one operator owns desktop input. The observer does not generate missing chunks, and installation never fills holes with air. Terrain and building recovery have separate provenance. Capture/export success is not a graphical travel pass. [Original settlement capture](../docs/audit/ADVENTURE_SETTLEMENT_TERRAIN.md) and [integration](../docs/audit/ADVENTURE_SETTLEMENT.md) remain the prior fixture's contracts.

### Adventure NPC recovery and checks

Population metadata and shop prices can be reproduced without graphics:

```sh
python3 rebuild/tools/export_adventure_population.py --check
python3 rebuild/tools/export_town_interaction.py --check
```

Actor recovery uses `export_adventure_population.py --assets NEW_DIRECTORY --male-bake CLOSED_CAPTURE.jsonl` with the UnityPy/Pillow environment; it preserves separately owned welcome/supplier scenes and rejects existing actor directories. Male and female rigs require their matching captured Avatars. See the [population recovery audit](../docs/audit/ADVENTURE_POPULATION.md) for prerequisites.

Adventure requires `adventure-welcome/recording.peas`, `adventure-supplier/recording.peas` and the in-crop residents under `adventure-population/<entity>/recording.peas` beneath the selected assets root. The supplier scene contains12 parts with the saved face/hair/bronze armor, stowed knife and forearm shield; the loader renders a portrait from that same recovered appearance. NormalIdle uses a matching original FemaleAvatar bake, not an observation of Eitai's live NPC controller. Diffuse material recovery does not reproduce the full original shader, IK or gestures.

Both locally installed groups already exist. Westina uses the archived role1 entity20008/RandomNpc17 with nine parts, iron armor and stowed copper sword/shield. See the [welcome audit](../docs/audit/ADVENTURE_WELCOME.md). To reproduce it only when the destination is absent, use the optional UnityPy environment described under asset recovery:

```sh
rebuild/build/unity-tools/bin/python rebuild/tools/export_adventure_supplier.py \
  --output rebuild/build/reference-assets/adventure-supplier
rebuild/build/unity-tools/bin/python rebuild/tools/export_adventure_supplier.py --npc welcome \
  --output rebuild/build/reference-assets/adventure-welcome
```

The exporter requires a **fresh** output directory, the preserved original Adventure `GameSave/save0` archive, installed native game resources, the closed `female-clips-02.jsonl` bake and existing training-knife fixtures. Defaults target this machine's isolated reference profile; `--save-dir`, `--game-dir` and `--bake` override those inputs. To inspect a second export, choose another fresh output path rather than overwriting the active asset group. The original archives and game files remain read-only. [Full recovery evidence and prerequisites](../docs/audit/ADVENTURE_SUPPLIER_APPEARANCE.md).

Generated dialogue and collision data can be checked without a graphical window:

```sh
python3 rebuild/tools/export_adventure_dialogue.py --check
rebuild/build/unity-tools/bin/python rebuild/tools/export_adventure_npc_collider.py --check
python3 rebuild/tests/test_adventure_dialogue_export.py
python3 rebuild/tests/test_adventure_supplier.py
rebuild/build/unity-tools/bin/python rebuild/tests/test_adventure_npc_collider.py
make -C rebuild build/test_core
./rebuild/build/test_core
```

`export_adventure_dialogue.py` generates source9031/9032 text, role tokens and arrival constants from the original database/localization; `export_adventure_npc_collider.py` generates exact physical/Defence capsules from FemaleModel. The supplier exporter includes matching PESC/provenance. `pe_adventure_npc` retains the first supported pose as the immutable arrival anchor, while later terrain/tree changes may settle the displayed NPC. NPC placement and open dialogue pages reset on load; existing PeGame progression and one-time supply ownership persist. The source physical player/NPC layer10 self-pair is disabled, so Eitai is not a player movement blocker. None of these checks establishes general town generation or graphical parity.

## Recovered terrain textures

Sandbox terrain now uses the original six diffuse atlases,84 source material slots, source triplanar projection and per-triangle material blending. Grass, soil and exposed stone remain textured after digging and reload. Imported terrain retains exact original IDs; the synthetic world's grass/soil/stone use representative source materials3/1/17. Source OpenGL mip selection and tile repetition/insets are retained; lighting, water and world generation are still approximations. [Cutaway capture](../docs/reference/sandbox/rebuild-terrain-layers-reloaded.png), [source audit](../docs/audit/SANDBOX_TERRAIN_MATERIALS.md).

The assets are installed on this machine under `rebuild/build/reference-assets/terrain`. To recover or verify them using the optional offline UnityPy/Pillow environment:

```sh
rebuild/build/unity-tools/bin/python rebuild/tools/export_terrain_assets.py
make -C rebuild terrain-material-test
make -C rebuild terrain-assets-check terrain-asset-test ASSET_PYTHON="$PWD/rebuild/build/unity-tools/bin/python"
```

The C game loads ordinary PNGs and has no Unity dependency. Unsupported imported material IDs render magenta instead of silently choosing another material. The recovered normal atlases are retained for future shader work; the inspected source OpenGL shader itself uses geometric normals for lighting.

## Original terrain import

`--terrain` reads an original `.voxelform` file through the native LZ4 reader and extracts a 64 × 32 × 64 density-sample crop at LOD 0. By default it searches for a suitable surface and crop; `--origin X,Y,Z` specifies nonnegative file-local sample coordinates. Raw material IDs and crop origin are preserved in saves. Import fails without replacing state if the file/crop is invalid or no safe player position can be found.

```sh
./rebuild/build/planet_explorers \
  --terrain 'CustomGames/Dragon Desert/Worlds/Dragon Desert/map_x0_y0.voxelform' \
  --save /tmp/dragon-desert-crop.pesave
```

Imported terrain currently has no imported trees, resource props or NPC population. Adventure can place the same bounded reference supplier if supported free space is available; this does not import a town or NPC records. New Adventure still receives its starter inventory. To explore it with tools and materials earned in the generated world, load that save first:

```sh
./rebuild/build/planet_explorers \
  --load /tmp/exploration.pesave \
  --terrain 'CustomGames/Dragon Desert/Worlds/Dragon Desert/map_x0_y0.voxelform' \
  --save /tmp/equipped-terrain-crop.pesave
```

This preserves inventory, equipment, clock, and crafting progress, and replaces world-specific terrain, structures, trees, and player position. It imports terrain samples, not a complete original scene or Unity save.

## Original game reference

The installed original Linux **V1.1.4**, running **Unity 5.2.4f1**, has reached its main menu and the tutorial with Andy on the simulation deck; screenshots verify both. The Unity editor has not been run, and the supplied project has not been rebuilt in Unity. These observations establish an executable reference, not complete game or rebuild parity. The [reference-runtime investigation](../docs/REFERENCE_RUNTIME.md) records the evidence.

For an existing original Linux installation, run from the repository root:

```sh
python3 rebuild/tools/launch_reference.py
```

The helper uses `~/.local/share/Steam/steamapps/common/Planet Explorers` by default and creates a separate profile at `~/.local/share/planet-explorers-reference`. Override either location as needed:

```sh
python3 rebuild/tools/launch_reference.py \
  --game-dir '/path/to/Planet Explorers' \
  --user-data /tmp/pe-reference-profile
```

The profile path must contain no whitespace because of the original command-line parser. `--prepare-only` validates the installation and prepares settings without starting the game. The helper changes `QualityLevel` to 6 and `AntiAliasing` to 0 in the profile's `PlanetExplorers/Config/SystemSaveData.xml`. It also sets the launched process's `XDG_CONFIG_HOME` to the profile's `engine-config` directory and resets only `UnityGraphicsQuality` to integer 0 in that directory's `unity3d/Pathea Games/Planet Explorers/prefs` before every launch. Existing files receive uniquely named backups before changes; other settings and the user's shared Unity preferences are preserved.

Both settings matter: disabling MSAA resolved the observed black window and `GLXBadContextTag` errors, while resetting engine startup quality prevents a stored higher quality from enabling MSAA before game settings load. The helper launches the native 64-bit player windowed at 1280 × 720, in English with CPU terrain extraction, and leaves console output visible. It does not download the game or change Steam settings. Ordinary Steam Play is not established by the isolated launch evidence. The C rebuild itself requires neither this installation nor Steam.

### Automated reference input

On the current Hyprland/Xwayland desktop, the bounded driver can operate the original client without a person holding the controls:

```sh
python3 rebuild/tools/reference_input.py layout
python3 rebuild/tools/reference_input.py key F5
python3 rebuild/tools/reference_input.py key Shift_L+w --duration 1
python3 rebuild/tools/reference_input.py look 200 0
python3 rebuild/tools/reference_input.py click 1045 584
python3 rebuild/tools/reference_input.py shot /tmp/reference.png
```

It targets only the original client title/executable, releases held keys, and validates client-local click coordinates. Keyboard holds are capped at five seconds. Clicks build a bundled Wayland helper on first use with `wayland-scanner`, a C compiler and `pkg-config wayland-client`; screenshots require `grim`, and keys/look require libX11/libXtst. No download or permanent desktop configuration change is involved. It needs the graphical session available and will focus the game, so avoid simultaneous desktop input while it runs. This driver supports controlled observations; it is not a general autonomous-game bot or a complete tutorial script.

## Recovered room and tools

The installed training room and six resource-tool bundles can be converted without Unity. The optional exporter requires UnityPy 1.25.4 in a separate environment:

```sh
make -C rebuild unity-tools   # pinned venv in rebuild/build/unity-tools
rebuild/build/unity-tools/bin/python rebuild/tools/export_reference_assets.py
rebuild/build/unity-tools/bin/python rebuild/tools/export_reference_assets.py --asset training-kit
rebuild/build/unity-tools/bin/python rebuild/tools/export_reference_assets.py --asset training-knife
rebuild/build/unity-tools/bin/python rebuild/tools/export_reference_actor.py
./rebuild/build/planet_explorers --reference-room rebuild/build/reference-assets/room.obj
```

Default output is the ignored `rebuild/build/reference-assets/` directory. Original bundles stay in the installation. The room preview uses source coordinates and recovered diffuse textures; WASD/mouse moves the free camera, 1 returns to the deck view, Home shows the exterior, and Escape exits. It does not load or save a world. Collision geometry is exported separately but not active in this preview. NPCs, original lighting/shaders and holographic effects remain absent.

`--reference-room` also accepts exported tools and supports `--smoke 90 --screenshot /tmp/room.png`. It rejects world/save/panel options. [Asset recovery](../docs/audit/ASSET_RECOVERY.md) documents transforms, material provenance and the guarded workaround for a raylib 6 OBJ parser hang. [Playtest evidence](../docs/PLAYTEST.md) includes the original tutorial and the rendered C room.

The auxiliary female defence actor can combine the existing closed locomotion/knife and tool bakes offline. This installs16 authored capsules on a compatible23-joint actor with19 exact clips (the original ten plus Spade1–3, Felling1–3 and Mine1–3), retaining each capture hash. Duplicate clips or incompatible skeletons fail; missing clips must never use an idle substitute. The original ten-clip export remains the default when no extra capture is supplied. With this machine's existing UnityPy1.25.4 environment:

```sh
pe_defence_bakes="$HOME/.local/share/planet-explorers-bake-reference/pose-captures"
rebuild/build/unity-tools/bin/python rebuild/tools/export_player_defence.py \
  "$pe_defence_bakes/female-clips-02.jsonl" \
  --extra-capture "$pe_defence_bakes/female-clips-08.jsonl" \
  --extra-capture "$pe_defence_bakes/female-clips-09.jsonl" \
  --extra-capture "$pe_defence_bakes/female-mine-10.jsonl" \
  --output rebuild/build/reference-assets/player-defence
```

## Playable training room

After exporting the assets above, open the recovered room with collision and the supported lesson sequence:

```sh
./rebuild/build/planet_explorers --training-room rebuild/build/reference-assets
```

This mode loads `room.obj`, its materials/diffuse textures, and `room-collision.obj` from the directory. Unlike the free-camera `--reference-room` preview, it moves a player capsule against all three exported source mesh colliders, including the hidden collision mesh. Original Unity world coordinates are preserved. The authored spawn is `(27, 2, 11)`, settled onto the actual deck; missing assets or an unsafe spawn produce an error instead of a substitute room.

The optional `--asset training-kit` export adds `training-kit.obj` and its materials for the storage prop. Room collision and lesson state remain available without that visual prop. The tutorial loads all nine original NPC models from `npcs/{entity_id}/recording.peas`, with their own recovered idle animations, source placements and Andy's scripted orientation. Names appear above nearby visible characters; Andy retains the lesson interaction. NPC bodies do not block the player, matching the original collision layers. Minimal asset sets without NPC scenes retain the earlier Andy marker; partially installed NPC groups fail with an explicit error. Side conversations, patrol/doctor/repair AI and Percy's weapon attachment remain unimplemented. See the [NPC audit](../docs/audit/TRAINING_NPCS.md) for the roster and capture/export process.

| Input | Training action |
| --- | --- |
| WASD | Move relative to the camera |
| Z | Toggle walk 2 m/s / run 5 m/s |
| Shift / Space | Dash at 8 m/s / jump, subject to stamina |
| Middle mouse | Toggle autorun; backward input cancels it |
| F5 | Cycle Third Person → Control Mode → First Person |
| Right mouse in Third Person | Hold to look around |
| Mouse in Control Mode / First Person | Look around; Tab releases or recaptures the cursor |
| 1 / keypad1 / Space / Enter / click the dialogue panel | Advance the numbered current response; its closing frame cannot also use a quickslot, jump or tool |
| E / right-click the kit or Andy | Interact during the applicable lesson and within reach |
| I | Open or close the inventory; click category icons to select items, equipment or resources |
| C / click the health display | Open or close character information independently of the bag; drag its model preview to rotate |
| Q / minimap mission button | Open or close the source-style mission window |
| Y | Open or close the tutorial Replicator |
| Drag Herbal Juice to quick-bar slot 1 | Assign the stack during the quick-bar lesson |
| 1 / right-click quick-bar slot 1 | Consume assigned Herbal Juice when its ten-second cooldown permits |
| Inventory Equipment tab, right-click an unequipped weapon | Equip the owned knife, Safety Axe or selected shovel instance |
| Character weapon slot, right-click | Unequip the same weapon back into the bag |
| Bottom menu button | Open the supported Inventory, Character, Replicator, Mission and Pause commands |
| Left-click in combat | Draw, then attack while stopped; click during combo window to chain (combat assets required) |
| Short right-click in combat | Stow the drawn knife |
| Escape | Close an active panel, otherwise pause/resume; the cursor is released while paused |
| Q while paused | Exit the training session |

The supported sequence is **739 → 747 → 718 → 740 → 741 → 742 → 743 → 744 → 749 → 745 → 746 → 719 → 748 → 757 → 721 → 751 → 752** with the corresponding optional assets. Construction additionally implements **752 → 753 → 756**, with 756 an unsupported creation-lesson boundary and the 15,000-frame graphical check passed. The 6000-frame graphical check passed through 748 with five herbs depleted and one axe. Andy’s original welcome300215 and Tips300258 precede camera input. 1 (or keypad1), Space, Enter or a dialogue click advances them; camera and movement events then complete the first two lessons, followed by the five original interface explanations. Walk to the materials at `(7,1.5,12)`, open the kit, transfer five Herbal Juices, and return to Andy at `(32,4,12)`. Complete the reply and medicine dialogue, then open inventory: accepting lesson 743 grants five additional juices, giving ten. Assign the stack to quick-bar slot 1 and press 1 to consume one, leaving nine. Accepting 745 grants one knife and opens the original equipment dialogue. Open I, choose Equipment, and right-click the knife to equip it. This advances to **746, the combat lesson**. With the matching baked body, drawn knife and hare hitboxes installed, left-click draws the knife, another click starts a stationary third-person attack, and a click during its combo window queues the next stage. A short right-click stows it. A real hare death opens completion dialogue 300230, then 719’s gathering introduction. With the recovered herb assets installed, approach each plant and press E. Five timed depletions enter **748**, grant one Safety Axe and show the original lumbering introduction. With the recovered axe/cutting and shovel assets, the next lessons continue through tree cutting, shovel crafting and real mound excavation to 752, as documented below. Without the optional combat assets, the equipment lesson remains the available boundary. Events and inventory changes are gated by the applicable lesson; repeated actions do not duplicate the kit transfer or mission grant.

The English dialogue preserves source text and IDs. The compact inventory, quick bar, dialogue panel, log, and status display represent the supported subset of the original HUD. The Items/Equipment tabs cover only juice and knife; full mission/NPC interfaces, original UI layout and input timing are incomplete. Right-click uses a target ray; the E interaction path is also subject to the controller's reach checks. See [interaction provenance](../docs/audit/TRAINING_INTERACTION.md).

The source camera starts at a 6 m follow distance, yaw 30°, pitch −20°, and 60° field of view. The C direction convention represents that same direction with yaw 150°; this does not reflect the room coordinates. Mouse pitch is limited to −70°…55° in Third Person/Control Mode and −75°…75° in First Person. Camera clipping uses a simple obstruction ray. Mouse sensitivity, immediate rotation, fixed eye/target heights, and the absence of original camera damping, activity-space adjustment, animated targeting, and zoom remain provisional.

The fixed 60 Hz controller uses a 0.30 m radius, 1.80 m tall capsule and static triangle collision with sliding and steps. Recovered movement speeds and stamina rules are present, but this is a new controller rather than the Unity Rigidbody/action system: jump impulse 7 m/s, gravity 20 m/s², instant acceleration, air steering, collision skin, and step response remain substitutes. The jump action ends on landing or after the source three-second timeout, allowing stamina recovery during a longer fall. Optional recovered starter-character assets supply six locomotion clips; absent assets retain a blue capsule. Original lighting and shader effects remain absent. See the [asset recovery](../docs/audit/ASSET_RECOVERY.md) and [tutorial audit](../docs/audit/TUTORIAL.md) for source evidence and limits.

Training is a separate session with Herbal Juice, one owned knife, a lesson resource ledger and one quickslot. HP starts at 500/500. Juice immediately restores 30 HP, applies the source timed healing buff, and starts a ten-second cooldown; it neither restores stamina nor revives a dead player. The fixed-step implementation preserves the source strict tick/expiry order, including the final tick at step 601. Reusing replaces the buff. Stationary knife damage and prototype 13 death progression are implemented when combat assets are installed. Training includes the source wood-shovel recipe in lesson 721; mission save/load remains absent and exiting resets progress. See [equipment and medicine evidence](../docs/audit/TRAINING_COMBAT.md). Training cannot be combined with `--reference-room`, terrain, save/load, or smoke-panel options. The tutorial module exposes an independent snapshot, but neither the training frontend nor PECS saves currently persist the training interaction/inventory state. The sandbox retains its separate inventory, saves, and gather/craft/build checklist.

A short graphics check exercises the camera and movement lessons; a longer check walks the supported sequence and exercises inventory/quick-bar use:

```sh
./rebuild/build/planet_explorers --training-room rebuild/build/reference-assets \
  --smoke 90 --screenshot /tmp/pe-training.png
./rebuild/build/planet_explorers --training-room rebuild/build/reference-assets \
  --smoke 1500 --screenshot /tmp/pe-training-sequence.png
make -C rebuild training-sequence-smoke
```

The 90-frame check expects mission 718 after the first six camera/movement objectives. The 1500-frame check expects mission 746 with nine Herbal Juices and the owned knife equipped after storage, Andy, inventory, assignment, use, and equipment; unmet expectations, asset loading failures, and screenshot failures return errors. Core, sanitizer, and CLI checks pass. Earlier graphics checks reached 718, and the previous 1500-frame storage/return run reached its then-supported boundary at 743 with five juices. The extended 1500-frame graphical run now passes at 746 with nine juices, an equipped knife, full HP and all entry dialogue acknowledged. A separate keyboard/mouse playthrough also passed: dropping outside the quick bar kept mission 744, an empty slot, and ten juices; a valid drag advanced to 749 with ten; pressing 1 reached 745 with nine. That earlier session completed after 12,753 frames with both camera-mode objectives and all four movement objectives recorded.

A later 38,308-frame keyboard/mouse playthrough reached 746 with nine juices, an equipped knife and 500/500 HP; unequipping and re-equipping conserved the same knife. The newest 1500-frame graphics run also exercised leisure patrol, reporting mode 1, one decision, and hare position `(3.293,1.632,17.898)`. At that stage, core/sanitizer checks, fifteen CLI tests and the new headless combat/actor/hitbox modules passed; the current suite has eighteen CLI cases. See the [playtest evidence](../docs/PLAYTEST.md) for completed observations. `make -C rebuild training-smoke` repeats the short check.

## Recovered tutorial interface

Training now uses the recovered source UI atlases, bitmap fonts, window layouts and item/portrait sprites. The bottom status/shortcut frame, Andy/Tips dialogue, inventory and storage grids, character preview, mission tracker/log, Replicator and supported menu commands replace the earlier rectangular text panels. The tutorial begins with Andy’s actual simulation-deck welcome and the source third-person camera explanation. Training and sandbox use the same recovered UI assets through separate hosts; lesson state and general PeGame state remain distinct.

The export contains 2,474 sprite records across three original PNG atlases, eight source bitmap-font exports and the original TTF files. Source effects add 13 bounded specular/scanline overlays using 11 original mask/noise PNGs and translated GLSL. The room minimap uses the original TrainingRoomMinimap bitmap, source crop/shader behavior, player heading and the nine NPC markers. Quest/localization export supplies 19 original mission titles/descriptions and 24 ordered objectives, including empty title718 and the source counter rules. See [UI audit](../docs/audit/TRAINING_UI.md), [effect provenance](../docs/audit/UI_EFFECTS.md) and [source layout](../docs/audit/TRAINING_UI_LAYOUT.json).

For a fresh UI destination, recover assets with the optional UnityPy/Pillow environment used by the other source exporters:

```sh
rebuild/build/unity-tools/bin/python rebuild/tools/export_training_ui.py \
  --output rebuild/build/reference-assets/ui
python3 rebuild/tools/export_training_ui_text.py --check
./rebuild/build/planet_explorers --training-room rebuild/build/reference-assets
```

The asset exporter requires a fresh destination; it does not overwrite the Unity project or original installation. Existing recovered UI assets can be reused. The bitmap-font metrics, portraits and frame sprites are original source assets; full equivalence to the shipped Unity rendering, texture compression and postprocessing is not claimed.

C opens only character information; I opens the bag independently. Equipped weapons leave their bag slots and appear in the character weapon slot; right-clicking that slot returns the same instance without repairing or duplicating it. Opening storage also opens the bag. Inventory category clicks, equipment right-click, script use and juice drag/drop call the existing headless transactions. The source quickbar displays an equipped-item cell plus nine assignable cells, but only assignable slot1/Herbal Juice is currently functional. The other slots/pages, full armor equipment, general inventory sorting/splitting/discard, arbitrary item use, arbitrary recipes/quantity/cancel controls, complete quest history and general options menus remain unsupported. The mission window shows the current real lesson and no fabricated completion rewards. Other storage-category buttons and general window dragging remain unsupported; character-preview dragging only rotates the model.

Headless coverage includes six generated-text tests, source layout/asset checks, character-panel independence from lesson743, conserved equipment instances and rejected modal actions. The full tests and positive UI core harness pass after adding character state and startup dialogue. Runtime font switching uses native32-pixel headings,18-pixel dialogue/tabs and26-pixel overhead names rather than authored37/20/30 scales. Manual checks passed at1280×720 for welcome/Tips, I/C/Q/Y windows, the source blue cursor, inventory category clicks and close button, mission close and menu→Replicator. A954×1048 portrait window also passed character display and drag-to-rotate preview checks. The new UI’s15,000-frame rerun reached mission756 with the expected combat, gathering, crafting, mining and construction state. The final tracked15,000-frame run after visual polish passed with exit0. See the [final run log](../docs/reference/rebuilt-ui-result.txt), [completed frame](../docs/reference/rebuilt-ui-complete.png) and [UI evidence](../docs/PLAYTEST.md#original-source-ui-integration--manual-checks-and-complete-progression). Earlier gameplay smoke results below predate this UI replacement unless explicitly stated otherwise. Dialogue blocks movement and F5 in this C host; that blanket modal policy is not established as source-equivalent.

## Recovered animated hare

The optional actor exporter above adds `hare.peac` and `hare-diffuse.png` to the reference asset directory. The C renderer skins the original 1,766 vertices against 48 recovered bone palettes. Preview it with:

```sh
./rebuild/build/planet_explorers --reference-actor rebuild/build/reference-assets/hare.peac
make -C rebuild actor-smoke
```

The preview supports 1 for idle, 2 for run, Left/Right to select any clip, R to restart, Space to pause, right-drag to orbit and the wheel to zoom. The export also contains five original melee clips and walk. Its earlier 90-frame graphics check passed through idle/run, and a 630-frame check passed while cycling all eight clips. The 630-frame check also verified the source hitbox overlay. Training optionally displays the hare at the source combat spawn, settled onto room collision, at deterministic scale 1. It now uses a source-informed leisure patrol with the recovered walk/idle clips. Its scheduler, random sequence, capsule controller and ledge avoidance are native approximations; perception, hostile pursuit, hare attacks and ragdoll remain unimplemented. The player’s stationary knife attacks can now damage this posed hare and dispatch death progression when combat assets are installed. [Movement provenance](../docs/audit/HARE_MOVEMENT.md) distinguishes source values from these choices. Optional baked knife assets follow the recovered animated waist mount; otherwise the static waist proxy remains. [Actor recovery](../docs/audit/ACTOR_RECOVERY.md) explains the sampled animation format and interpolation limits; [hitbox recovery](../docs/audit/HITBOX_RECOVERY.md) records source capsules and their skeletal mapping. The stationary training melee host now connects the player contact/timing modules to attacks and damage; the 3600-frame graphical check passed through 719. [Player animation](../docs/audit/PLAYER_ANIMATION.md) records the recovered knife events and remaining humanoid retargeting work. Original-client observations confirm knife grant, equipment and drawing; they do not confirm a successful hit or kill.

## Player visual research

The optional player exporter recovers the default male/female modular meshes, diffuse textures, skin weights, inverse binds and Avatar data into a separate ignored directory:

```sh
rebuild/build/unity-tools/bin/python rebuild/tools/export_reference_player.py
```

Output defaults to `rebuild/build/reference-player/`. This produces static Avatar-default-pose OBJs and decoded Attack6 scalar data, not a playable animated character. Headless recovery and repeatability tests pass. Subsequent original-runtime pose capture and isolated clip baking now supply the animated training character described below. See [player asset recovery](../docs/audit/PLAYER_ASSET_RECOVERY.md) for exact source identities, mathematical checks and the unresolved humanoid retargeting step.

## Sandbox controls

For a short movement, camera, collision and equipment check, use the [hands-on directional checklist](../docs/PLAYTEST.md#hands-on-checklist--current-directional-build). The current sandbox supports the bounded walking/running Knife attacks described above; the tutorial retains stationary attacks.

| Input | Action |
| --- | --- |
| WASD / mouse | Move / look |
| Shift / Space | Sprint / jump |
| Z / middle mouse | Toggle walk/run / autorun; backward input cancels autorun |
| Mouse wheel, third person | Zoom from 1.8–10 m, or 1.8–20 m in construction; modal UI consumes scrolling |
| E / right mouse while aiming at Eitai | Open the eligible supplier's source choice window |
| 1–9 / keypad1–9 in conversation; Enter / dialogue arrow | Select the matching visible response; Enter/arrow advances the first option or current page. Final4094 acknowledgment alone grants supplies |
| Hold left mouse, outside construction | Adventure shovel12/pickaxe19/axe26/1527: draw/use while grounded, stationary and in third person |
| R / right mouse, outside construction and supplier interaction | Stow and cancel the current timed tool action |
| B | Enter/exit cursor-based construction |
| Left mouse / Alt + left mouse / X, in construction | Place / remove / remove at the pointer |
| Hold right mouse, in construction | Turn the camera |
| 1–9, outside construction | Use assigned quickslots; resource/furniture shortcuts select placement items |
| 1–7, in construction | Cube / dirt / stone / wood / ramp / fence / ladder; independent of normal quickslots |
| V / T / G | Cube/ramp / quarter-turn rotation / one or four cells |
| Y | Replicator; learned recipes only in Adventure |
| I / C / Q | Inventory / character / missions |
| Inventory right-click | Use, learn or equip a supported item |
| Inventory drag | Move to an empty bag cell, exchange occupied same-category cells, or assign a quickslot |
| Inventory Shift+drop | Merge matching stacks; any excess remains in the source cell (native convenience) |
| Inventory Delete | Choose Delete, select a cell, then confirm; Enter confirms, Escape cancels, right-click exits Delete mode |
| Inventory Split | Select a stack, choose an amount by typing, +/- or wheel; Enter confirms and Escape cancels |
| Inventory Sort | Consolidate matching stacks and order the current category |
| Inventory PageUp / PageDown; page arrows / wheel | Change pages; arrows/wheel act over the inventory |
| Quickslot right-click / drag | Use / rearrange shortcuts |
| Character weapon right-click | Unequip the tool |
| Tab / F5 | Cursor override / cycle free third person, captured third person, first person; F5 clears the override |
| F9 / F10 | Save / load the configured file |
| Escape | Close a panel, or pause/resume |
| Pause buttons | Resume, Save, Load or Main Menu |

Panels stop movement but allow simulation and crafting to continue. Escape pause stops simulation. Zoom starts at the source 6 m distance and uses the scalar Unity 5.2 SmoothDamp algorithm with a .5 s smooth time. Wheel input is gated by world-input ownership; unpaused third-person smoothing continues while panels block new wheel input. Pause, lost focus and first person freeze this clock. The .8 m-per-notch adapter and native collision offset remain approximations. Original activity-space scaling, two-second clamp recovery and the remaining CameraForge graph are not ported; vegetation does not gain camera blocking or fading. See the [camera source audit](../docs/audit/SANDBOX_CAMERA.md). These are rebuild controls, not the original game's complete action map.

## Implemented systems

- A seeded test world or imported original terrain crop, with two-byte density/material samples and the original isosurface threshold of 128. CPU marching cubes uses tables exported from the original source. Four bounded render regions, up to 32 cells per axis, cache GPU meshes; byte comparisons include the normal-gradient halo so edits rebuild affected regions and necessary neighbors.
- Mesh-consistent terrain ray picking/heights, safe spawning, walk 2 m/s, run 5 m/s, sprint 8 m/s, jumping, slope steps, terrain collision, and shape-aware Block45 collision. Z toggles walk/run; middle mouse toggles autorun.
- 341 source-derived item definitions and108 formula records, with original IDs, stack limits, ingredient/output counts, durations, and tool/player properties. Adventure has four independent420-cell inventory categories with stable non-stack identities and persistent per-instance durability; Legacy/tutorial retain32 cells. Splitting, exchanging, explicit merging and source sorting preserve those owners. See [inventory scope](../docs/audit/SANDBOX_INVENTORY.md).
- Equipped pickaxes, shovels, and axes. Mining reproduces the source voxel-box indicator's threshold search, neighbor support, and snapped sample selection, including nonzero samples below the mesh isovalue. Reach is strictly under 3 m from feet plus one meter to the sample. Damage uses source attack/resource modifiers, hardness attenuation, byte-density reduction, and primary resource drop distributions. Known materials with no drops can still be removed. Unknown resource definitions are rejected without inventing a yield.
- Adventure tool host for exact items12/19/26/1527, with source entry/loop/exit gates, authored DigTerrain/Fell events, serialized tool speeds and stamina slowdown. Target locking, equipment-instance binding and cancellation prevent duplicate edits. Recovered female body and exact tool meshes read this shared clock. Wood Pickaxe19 uses its recovered Mine1–3 body/tool poses, entry speed1.5, source strike events, stamina4 and50/20 slowdown thresholds. Source controller blending/IK and arbitrary gender poses are not reproduced. See [action rules](../docs/audit/SANDBOX_ACTIONS.md) and [tool presentation](../docs/audit/SANDBOX_TOOL_PRESENTATION.md).
- Recovered tree geometry and diffuse foliage replace the generated world's cylinder/cone visuals. One original tree mesh is reused at provisional positions with unit scale; source alpha cutoff and the matching source trunk capsule support rendering and axe interaction rays. Adventure player movement collides with live source capsules; depletion removes their rendering and collision. Legacy movement is unchanged. The recovered tree is source prototype48; Adventure applies its hardness and scaled resource formulas at the supported unit runtime scale. Tree/herb yields use the saved gameplay RNG, including fallback and optional seed branches. Biome/species selection, nonunit runtime geometry, wind, LOD and stump/felling simulation remain absent. See [harvesting](../docs/audit/SANDBOX_HARVESTING.md).
- Twenty-five pinned Adventure identities with source appearance/sex/idle data, supported archived placement and sex-aware capsule picking. Welcome, Arrival and supplies retain their authoritative progression owner; three vendors add seventeen supported purchase offers and Annot discovery unlocks the bounded Naoko introduction. Dynamic crop placement and NPC visibility do not implement general population/AI.
- Source stamina capacity 100 and recovery speed 40, tool stamina/durability costs, and recovery delayed until an active action finishes plus at least 0.1 seconds. The player prefabs require 15 stamina to start sprinting and drain 5/s; a running sprint continues below 15 until exhausted. Jumping requires and consumes 10 stamina. Tool swings slow below source stamina thresholds; empty stamina does not prevent swinging, while broken tools do.
- One crafting job at a time, total input availability checked at start, inputs consumed per completed batch, and completed products held for source-style delivery: on a batch completion when more than five seconds have elapsed since delivery, or on the final batch. Three-second recipes therefore first deliver two products at six seconds in a longer job. Cancellation preserves unfinished inputs and collects completed products; insufficient output space preserves the pending job.
- Original Block45 half-meter cube/ramp geometry, packed shape/rotation/material bytes, four rotations, and grouped `ceil(cells/4)` build costs / `floor(cells/4)` refunds. Entire batches validate before inventory or world mutation. Crafted fence/ladder proxies and structures from older rebuild saves retain their separate geometry.
- A 26-hour clock at the source's normal 12× rate, starting at day 94.35.
- `PECS` save version20 with explicit little-endian fields, integrity checks, bounded semantic validation, temporary-state loading, and atomic file replacement. Rebuild save versions 1–3 migrate on load as Legacy mode (original controller and all exposed recipes known). Version 4 Adventure loads first validate non-tree geometry, then resolve an existing tree overlap using same-height radial candidates in .05 m increments, up to 3 m away. Candidate paths sweep non-tree geometry, avoid new tree intersections and move outward from initially overlapping trees. Velocity and saved clocks are preserved; grounding is recalculated. No safe candidate means the load fails without replacing current state. Version 5 requires a valid full-collision spawn and does not apply this rescue. Unity saves are not supported. Terrain metadata, RNG, tool identity/wear, stamina, pending craft output, construction, trees, player state, Adventure grants, recipe knowledge, nine quickslots, HP, juice clocks and the independent medpack cooldown persist, alongside hunt credit/return pages, deployed Torches, full dimensions, welcome progress, Annot completion and vendor stock/time. Versions1–7 retain their documented decoding; pre-v8 saves start with uninitialized shop stocks. [World ownership and save layout](../docs/audit/ADVENTURE_ROUTE.md) describe clone/move/destroy requirements for heap worlds.

## Validation

The town increment has optimized core, source/exporter and real-game UI routing coverage, plus UI ASan/UBSan/leak checks. Tests cover atomic purchases, source refresh boundaries, strict Annot arrival, v8 stock roundtrip and modal input. All25 recovered actors pass placement checks against the installed route; the old crop supports10 with15 outside its bounds. Original arrival/dialogue, physical C shop purchase/reload and graphical Annot continuation passed. The entire recorded route also passed ordinary headless movement with midroute save/reload. See [playtest evidence](../docs/PLAYTEST.md) for exact scope, test-only shop funding and process outcomes.

`make -C rebuild menu-smoke` checks the main menu. `make -C rebuild adventure-smoke` creates source-backed progression fixtures, then checks the character, Replicator and an airborne save through the graphics host. These commands open game windows. `rebuild/build/adventure_fixture --inspect SAVE` summarizes a rebuild save without graphics. The bounded `rebuild/tools/rebuild_input.py` driver targets only the three exact local application titles and the compiled executable; use one desktop operator and the current PID.

To exercise the three source-timed tool fixtures with actual mouse input, build the executable and fixture helper, then run:

```sh
make -C rebuild all build/sandbox_action_fixture
python3 rebuild/tools/playtest_sandbox_actions.py
```

This bounded Hyprland/Xwayland playtest launches only the local C rebuild, creates isolated shovel/pickaxe/felling fixtures, holds left mouse for eight seconds per action, saves through F9 and verifies resource changes, tool-instance identity and wear. It releases input and closes its owned windows on failure. Use one desktop operator and leave input undisturbed during the run. Default artifacts are fixture/result saves, logs, screenshots and `result.json` under `rebuild/build/sandbox-action-playtest`; `--output-dir DIR` selects another destination. It does not load or overwrite a normal user save. The physical-input tool playtest passed with the source 6 m camera fixture: two shovel swings consumed 10 durability and produced 2 dirt; one pickaxe Mine strike consumed 5 durability and produced 2 dirt; six wood-axe swings consumed 30 durability, depleted tree7 and produced 8 wood. All three retained the same tool instance and unrelated possessions. The pickaxe fixture loses its valid target after that sample is removed; held input does not imply further resource edits. See the [playtest record](../docs/PLAYTEST.md) for artifacts and the separate target-preview inspection status. This validates the bounded C fixtures, not original-game animation or resource parity.

```sh
make -C rebuild check          # parallel build + run of everything `test` covers
make -C rebuild -j"$(nproc)" test
make -C rebuild check-sanitize  # ASan/UBSan/leaks, one process per core suite
make -C rebuild check-content
make -C rebuild check-sources   # UnityPy-backed regeneration checks (needs unity-tools)
make -C rebuild cli-test
make -C rebuild asset-test
make -C rebuild sandbox-world-test
make -C rebuild sandbox-camera-test
make -C rebuild smoke
```

The core and test sources compile once into `build/obj` with compiler-generated header dependencies, and every test binary links `build/obj/core/libpe_core.a`; pass `-j` so objects build in parallel. `check` runs `tools/run_tests.py`, which takes the plan from `make -n test`, builds stale binaries in parallel, runs each command concurrently (one process per core suite), continues past failures, writes per-command logs to `build/test-logs/` and totals unittest skips. Use `python3 rebuild/tools/run_tests.py -k NAME` or `rebuild/build/test_core NAME...` (`--list` shows names) to iterate on one suite. A new core suite is one `PE_TEST(name)` line in `tests/test_core_list.h` for `int pe_test_name(void)` in `tests/test_name.c`. `make -C rebuild compile-commands` writes `compile_commands.json` for clangd/clang-tidy. UnityPy-backed checks use the pinned `make -C rebuild unity-tools` environment when present.

`test` runs C world, marching-cubes, original-file reader, Block45, gameplay, persistence and initial tutorial suites, plus content, launcher, reference-input and asset-conversion Python tests. The installed-asset repeatability test is optional and runs when UnityPy and the installation are available. `asset-test` checks the guarded model parser without opening a graphics context, but links raylib. `sandbox-world-test` checks recovered tree loading, ownership/cleanup, depletion visibility and interaction-ray behavior through headless graphics stubs. `sandbox-camera-test` checks the independently verified Unity 5.2 damping values, frame-rate variation, overshoot, pause/input gates and normal/build bounds. `sandbox-ui-test` covers real inventory actions and build-toolbar/modal input ownership; `sandbox-inventory-ui-test` adds category paging, split/exchange/merge/sort, exact-slot ownership and narrow-window control geometry; `sandbox-actor-test` checks shared action pose clocks and exact tool groups. These headless checks are also included in `test`; they do not substitute for graphical inspection. Launcher tests cover isolated settings, repeat-launch quality reset, backups, malformed XML, and process arguments/environment. Fixtures cover source geometry/rotations, original terrain samples, malformed compressed files, movement, tool wear, stamina recovery, primary yields, pending crafting/cancellation, grouped construction transactions, save migration, and semantic tampering with recomputed checksums. `sanitize` runs the C suites under AddressSanitizer and UndefinedBehaviorSanitizer with leak detection. `check-content` verifies both generated database content and marching-cubes tables. `cli-test` exercises argument validation and startup failures.

`smoke` requires a display. It creates a dedicated save fixture under `rebuild/build/` through normal game APIs: gather wood, craft/equip a pickaxe, mine terrain, place four cube and four ramp cells, and queue five fences. It then renders both the inventory/equipment panel and an imported original terrain crop to screenshots there. It does not replace the user's save. Individual unattended runs are also available:

```sh
./rebuild/build/planet_explorers --smoke 90 --screenshot /tmp/pe-smoke.png
./rebuild/build/planet_explorers --load /tmp/exploration.pesave \
  --smoke 90 --smoke-panel inventory --screenshot /tmp/pe-equipment.png
```

After the render-cache and placement fixes, the optimized C suites, ten Python content tests, eleven CLI tests, generated-content checks, and ASan/UBSan tests with leak detection all passed. The generated-world and imported-terrain smoke runs each completed 90 frames and were visually inspected: 9,765 and 9,644 terrain triangles respectively, with five region rebuilds each (four initial uploads and one edited region). These checks do not establish every input path, other operating systems, or original-game parity. The reference-runtime investigation records the limits of original-runtime comparisons.

The latest live-tree movement/save-v5 and smoothed-camera integration passed physical-input verification: trunk contact, six axe swings, depletion and walking through the cleared space; visible v4 overlap migration followed by v5 save/reload; and zoom6→3.6m with character-panel wheel consumption. The three Adventure graphics smokes also passed. Reproduce the bounded desktop check with `python3 rebuild/tools/playtest_sandbox_trees.py` after building `all build/sandbox_action_fixture`. It uses isolated saves and captures under `rebuild/build/sandbox-tree-playtest`, verifies fresh F9 writes and closes only its own C windows. An unlocked desktop and one operator are required. See the [playtest record](../docs/PLAYTEST.md) for original-versus-C evidence and remaining limits.

## Content provenance

`tools/export_content.py` reads the original SQLite files without modifying them. Its generated manifest records source hashes, selected IDs, schema mappings, tool/player fields, resource data, and limitations. The bounded catalog contains317 items and95 recipes: its initial tools and construction outputs are expanded by the source Adventure supply scripts and their ingredient closure. Adventure learns the corresponding formulas by consuming supported scripts; Legacy exposes all exported formulas. Supported swords, knife and bow can occupy the Adventure hand-equipment slot; Knife33 has bounded stationary sandbox combat, while broader weapon attacks remain unported. The separate training scene implements knife33 equipment and a bounded stationary combat path.

`tools/export_mc_tables.py` exports the original terrain extraction tables with provenance. Generated C data is checked in, so normal compilation needs neither Python nor runtime SQLite.

```sh
make -C rebuild content
make -C rebuild check-content
```

Mining `resource` records, smooth-terrain placement `buildblock`, and Block45 `blocktype` mappings are distinct. See the [content audit](../docs/audit/CONTENT.md) and [Block45 audit](../docs/audit/BLOCKS.md). The original [source and asset terms](../README.md) still apply; no new license is assigned to source-derived content. raylib and LZ4 remain external libraries with their own terms.

## Remaining limitations

The native world is bounded: there is no streamed full planet, original procedural generator, terrain LOD transition pipeline, water simulation, full vegetation-placement import, or general scene graph. The optional training mode imports one recovered static room with its mesh colliders. Source marching-cubes tables are reproduced, but the original GPU pipeline, material shaders, and streaming chunk lifecycle are not. The four-region render cache optimizes edits within the bounded volume; it does not stream terrain. Adventure uses capsule sweeps against the actual sandbox marching-cubes, Block45, furniture and live source tree capsules; Legacy retains its earlier cylinder/trilinear-density controller. Training sweeps capsules against its recovered room and active lesson geometry. These native controllers are not the original Unity physics implementation. Walk/run/sprint speeds and stamina rules now follow recovered player data, but jump velocity 7 m/s, gravity 20 m/s², instant acceleration, and airborne steering remain temporary. See the [tutorial and movement audit](../docs/audit/TUTORIAL.md) for serialized values, source-default discrepancies, and the original mission sequence.

Generated trees now share an original recovered mesh, but their placement, unit scale, eight-wood yield and unit resource/size factor remain provisional; source maximum health is 255. Their source trunk capsules support axe targeting and Adventure player movement collision, while camera queries exclude trees. Bare-hand loose-wood gathering remains an explicit bootstrap substitute. Trees do not fall when support is removed, and depletion hides them without a stump or falling animation. The water plane is decorative. Adventure Wood Shovel12, Wood Pickaxe19, Wood Axe26 and Safety Axe1527 dispatch through the general source-timed action host and matching poses. Legacy and unsupported tool variants retain generic cooldown pacing. The pickaxe uses its own Mine poses and exact recovered mesh; it does not substitute shovel presentation. Both the general action host and tutorial hosts approximate controller transitions discretely. The full action/skill/buff interpreter is absent. Mining currently covers the selected tools' single-sample radius and primary drops; rare/special bonuses, progression modifiers, and larger tool shapes are not implemented. Its persisted deterministic RNG is not Unity's RNG sequence.

Only Legacy mode unlocks all exposed recipes. Adventure starts with no learned formulas and implements bounded Arrival/supply/script progression, not the full campaign or settlement logic. Adventure inventory now implements the source four420-cell category capacities; Legacy and tutorial retain32 shared cells. The separate mission inventory and complete original item-component system remain outside this slice. The existing camp repair machine supports eligible ordinary bag equipment; arbitrary machines, worn-clothing removal, LifeLimit/creation repair, workspaces and full equipment slots remain absent. Crafts pause safely if inventory conditions change, retaining pending output; this intentionally avoids legacy loss/failure paths. Saves preserve movement velocity and stamina but clear held sprint/jump action flags; an airborne reload does not resume the jump action's regeneration lock. General tool-action stages are transient and are cancelled on load. The provisional legacy digging wrapper rejects an empty selected sample without wear, whereas the Adventure source-timed shovel preserves attempted-swing wear for a valid locked empty sample. Later invalid indicators suppress the attempted event; capacity-blocked edits preserve inventory, density, RNG, wear and the continuing charge.

Block45 supports only cube and ramp, bounded to 1,024 cells. Other primitives, extended blocks, face-mask culling, structural physics, and full brush/undo tools remain absent. Top placement can leave a gap of less than half a meter above smooth terrain. Fence/ladder props and legacy rebuild blocks retain full one-meter collision proxies; ladders are not climbable. Removing support does not collapse structures.

The optional training mode now connects recovered player/hare animation, stationary third-person knife contacts, source damage and a death-driven transition from 746 to 719. Its 3600-frame graphical combat check passed through 719. This is not general combat parity: normal locomotion clips remain in use while armed; drawing/stowing switches attachments immediately; camera-forward aiming is provisional; permissions use a fixed tutorial-pair whitelist; and the hare retains leisure behavior without hostile attacks or ragdoll. Capsule contacts are instantaneous, without source swept history. Moving/air/first-person attacks, controller transitions, root motion, hit reactions and procedural IK remain unfinished.

Audio, complete UI layout, general NPC AI, full campaign/dialogue, reputation, colony/farming, the creation editor, vehicles and multiplayer remain unimplemented. The training frontend now drives herbs, tree cutting, script learning, crafting and real mining, graphically verified through 752. Construction and the 50-ray receiver puzzle are graphically verified through the 756 boundary. Optional local imports provide the room, tools, kit, body, knife and hare; the Unity editor/source build remains constrained by missing third-party dependencies. Test success establishes this supported subset, not original-game parity.

## Code map

| File | Responsibility |
| --- | --- |
| `src/pe_world.c` | Density, generation, ray/collision queries, edits and material interpretation |
| `src/pe_mesh.c` | CPU extraction using source marching-cubes tables and region invalidation checks |
| `src/pe_legacy.c` | Read-only original `.voxelform`/LZ4 access and bounded crops |
| `src/pe_block.c` | Source Block45 geometry, rotations, collision and rounding rules |
| `src/pe_game.c` | Movement, tool instances, stamina, gameplay production and construction |
| `src/pe_inventory.c` | Category capacity, acquisition, split/exchange/merge/sort and stable migration |
| `src/pe_harvest.c` | Source tree hardness and tree/herb production calculations |
| `src/pe_tutorial.c` | Source mission IDs, event-driven initial lessons and independent snapshots |
| `src/pe_reference.c` | Guarded recovered OBJ/material loading and visual preview |
| `src/pe_actor.c`, `src/pe_actor_view.c` | Bounded sampled-actor loading, CPU skinning and rendering |
| `src/pe_hare.c` | Source-informed bounded leisure patrol |
| `src/pe_combat.c`, `src/pe_hare_attack.c` | Source action timing, stamina and damage/window rules |
| `src/pe_training_melee.c`, `src/pe_knife_contact.c` | Stationary training action host, posed knife contacts, per-stage deduplication and HP |
| `src/pe_hitbox.c` | Recovered skeletal capsules and headless contact queries |
| `src/pe_scene_collision.c` | Bounded triangle OBJ reader, static BVH, ray and capsule queries |
| `src/pe_training.c` | Fixed-step recovered-room controller and tutorial action events |
| `src/pe_training_dig.c`, `src/pe_training_terrain.c` | Source shovel events, actual mound edits, shared render/collision data and resource receipts |
| `src/pe_training_build.c`, `src/pe_training_receiver.c` | Transactional lesson construction and exact50-ray receiver scoring |
| `src/pe_training_text.c` | Exact source English dialogue and ID provenance |
| `src/pe_training_view.c` | Playable training cameras, input, objective HUD and graphics smoke |
| `src/pe_save.c` | Validated versioned persistence and migrations |
| `src/pe_sandbox_actions.c` | General source shovel/pickaxe/axe action events and authoritative pose clocks |
| `src/pe_adventure_npc.c`, `src/pe_adventure_npc_view.c` | Deterministic bounded supplier placement, source picking geometry, archived appearance and portrait |
| `src/pe_adventure_dialogue.c` | Source arrival test, supplier choice/pages, role-token binding and final acknowledgment handoff to PeGame |
| `src/pe_sandbox_actor.c`, `src/pe_sandbox_world_view.c` | Recovered body/tool poses and shared tree rendering/interaction queries |
| `src/pe_sandbox_ui.c`, `src/pe_sandbox_ui_input.c` | Real sandbox state, recovered panels and bounded build-toolbar input ownership |
| `src/pe_sandbox_camera.c` | Free/captured third person and first person, RMB gesture routing, desired orbit distance and verified scalar SmoothDamp |
| `src/pe_tree_collision.c`, `src/pe_sandbox_collision.c` | Source tree capsules and transactional geometry snapshots for Adventure movement/save validation; separate tree-excluding camera queries |
| `src/pe_sandbox_view.c` | Sandbox input, camera placement, regional rendering and fixed-step scheduling |
| `src/main.c`, `src/pe_app_menu.c` | Command-line entry, main-menu rendering and host routing |
| `generated/` | Exported content, extraction tables and provenance |
| `tools/` | Reproducible offline exporters and isolated original-game launch helper |
| `tests/` | Headless, content, CLI and graphics fixtures |

Next milestones are the unsupported creation lesson 756, streamed original terrain with LOD/neighbor edits, and fuller tool/resource/action fidelity. The specification defines the longer path through combat, narrative, settlements, creations, and multiplayer.

## Recorded original player poses

The optional original-runtime probe captured 566 evaluated female player poses, including all four stationary knife stages in both views. [Capture instructions](tools/REFERENCE_POSE.md), [analysis and exports](../docs/audit/POSE_CAPTURE_ANALYSIS.md), and [clock calibration](../docs/audit/PLAYER_ANIMATION.md) distinguish measured behavior from remaining Animator work. A multipart recording can be played directly in the C renderer:

```sh
./rebuild/build/planet_explorers --reference-actor-scene \
  rebuild/build/pose-analysis/female-starter-tp-attack6/recording.peas
```

Space pauses, R restarts, Left/Right step sampled frames, Up/Down select clips, right-drag orbits and the wheel zooms. `--smoke N --screenshot /tmp/player.png` runs a bounded graphics check. The loader retains the exact validated actors and decoded textures through upload, so disk changes cannot replace validated geometry. The replay includes original transitions and IK; it is sampled animation evidence, not a generalized humanoid Animator.

Training optionally loads `player-idle/recording.peas` under its asset directory. To use the recovered starter character after exporting the idle recording:

```sh
mkdir -p rebuild/build/reference-assets/player-idle
cp -a rebuild/build/pose-analysis/female-starter-idle-v2/. rebuild/build/reference-assets/player-idle/
./rebuild/build/planet_explorers --training-room rebuild/build/reference-assets
```

This legacy fallback replaces the capsule visual with the original starter outfit at the first recorded idle pose. Prefer the baked locomotion assets below for animated movement. First-person body visibility remains suppressed. Missing optional character assets preserve the capsule mode; malformed supplied assets fail loading. [Training screenshot](../docs/reference/rebuilt-training-player.png).

A matching captured stowed knife can replace the temporary waist placement:

```sh
mkdir -p rebuild/build/reference-assets/player-knife-stowed
cp -a rebuild/build/pose-analysis/knife-idle-v2/. rebuild/build/reference-assets/player-knife-stowed/
```

It appears only while the knife is equipped. The [knife exporter](../docs/audit/KNIFE_COLLISION.md) also builds a combined attack recording and collision capsule anchored to the same sampled weapon transforms. The optional [headless melee fixture](tests/REFERENCE_MELEE.md) verifies source damage against actual recovered hare geometry at an explicit controlled placement. It does not complete the combat lesson.


## Recovered player locomotion

The installed directional set contains twenty female source clips in each of eleven body/attachment scenes. Twelve are runtime locomotion states: idle; forward, left, right and backward walk/run; jump, fall and land. Four stationary knife attack clips and four source turn alternatives complete the twenty. The separate23-joint player defence actor contains those twenty plus Felling1–3, Spade1–3 and Mine1–3, giving twenty-nine clips.

In both hosts, free third person turns the body toward actual travel independently of camera orbit, then retains its heading when stopped. Captured third person faces the camera, so A/D and S select side/back steps; first person hides the body. The native heading adapter uses the source normal rate 6 and body-right intermediate turn for reversals above 150 degrees, while its measured movement target differs from the full source input filtering. Active tool/melee actions retain facing precedence.

Movement selection uses measured velocity in body space, with dominant-axis selection and forward/back ties. Normalized pose weights approach that selected state with a 0.12s time constant; all eight gait clips share continuous phase, including rapid reversals. Body and matching held/stowed equipment use the same blended clocks and transform. Sandbox Torch mounts and incoming defence capsules follow that exact blend and body yaw. Tool/melee actions instead use their single authoritative unblended action clock. Initialization/load resets transient facing and locomotion to Idle. Supplied body and locomotion attachments must contain all twelve required locomotion clips with matching metadata; old ten-clip groups need a complete upgrade.

Current local assets are already installed. To reproduce them, follow [the directional recovery audit](../docs/audit/DIRECTIONAL_PLAYER.md): generate the exact source inventory, supervise a fresh `normal-directional` capture, then use `export_directional_player.py` to stage all scenes and defence before its separate backup-preserving installation step. Stop the native client before installation. Do not copy individual older body or attachment groups over the coherent set. Local staging is `build/directional-player-20261006`; previous installed folders are preserved in `build/player-before-directional-20261006`.

```sh
./rebuild/build/planet_explorers --training-room rebuild/build/reference-assets
TMPDIR="$PWD/rebuild/build/compiler-tmp" python3 rebuild/tests/test_directional_player.py
```

The captured poses are exact source leaves; cardinal selection and exponential pose-weight smoothing are native adapters. They do not implement the source 5×deltaTime Animator parameter lerp or its two-dimensional blend graph. Several original third-person branches use turn clips instead of the fixed-facing strafe leaves used by the captured-mode adapter. Full directional blending, general root motion, weapon movement styles and the separate sprint clip remain unfinished; the bounded sandbox running Knife exception is documented in [its audit](../docs/audit/SANDBOX_RUNNING_KNIFE.md); sprint scales the selected run fallback. [Locomotion source thresholds](../docs/audit/PLAYER_LOCOMOTION.md) and [bake lifecycle evidence](../docs/audit/ANIMATOR_BAKE.md) retain their historical scope. For the preceding directional asset increment, full optimized/content/CLI checks and focused asset sanitizer checks pass; native A/D/S run/walk exercised all six new clips and exited0. The full15,000-frame tutorial reached756 and full ASan/UBSan with leak detection passed. A separate captured-camera tutorial run also exercises all six new directions (mask4033) and exits0. Moving attacks and full blend parity remain outside these checks. Those results predate the body-facing and pose-smoothing correction. The corrected build separately passes full optimized build/tests, core ASan/UBSan with leak detection and a new15,000-frame tutorial progression to756; its19 ordinary Python optional skips remain skips. New physical sandbox free-mode turns/stopped heading and captured sandbox/tutorial transitions are recorded in [PLAYTEST](../docs/PLAYTEST.md#october-6--free-mode-facing-and-smooth-movement-transitions), including recording/framing limits and the absence of exhaustive equipment/action-facing coverage. See the [current hands-on checklist](../docs/PLAYTEST.md#hands-on-checklist--current-directional-build).

## Stationary training combat assets and check

The current coherent directional installer already includes the matching drawn/stowed knife. The following older ten-clip commands document the original recovery fixture; do not install that group over current directional assets. The exporter accepts a closed isolated bake and writes a fresh directory:

```sh
python3 rebuild/tools/export_baked_knife.py \
  ~/.local/share/planet-explorers-bake-reference/pose-captures/female-clips-02.jsonl \
  --rig rebuild/build/reference-player-starter/player-female-starter-rig.json \
  --knife-collision rebuild/build/reference-assets/knife-collision.json \
  --visual-assets rebuild/build/reference-assets \
  --body-scene rebuild/build/animator-bake/player-female/all/recording.peas \
  --attachment drawn --output rebuild/build/animator-bake/player-female/drawn-all
mkdir -p rebuild/build/reference-assets/player-knife-drawn
cp -a rebuild/build/animator-bake/player-female/drawn-all/. rebuild/build/reference-assets/player-knife-drawn/
./rebuild/build/planet_explorers --training-room rebuild/build/reference-assets \
  --smoke 3600 --screenshot /tmp/pe-training-combat.png
```

If `drawn-all` was already exported, use the copy commands without rerunning the fresh-directory export. The reference directory also needs `hare.peac`, `hare-diffuse.png` and `hare.pehb`. A supplied drawn scene must match the body’s named clip metadata and contain `knife-drawn.pehb` anchored to its first material actor. Missing optional drawn assets leave combat unavailable; malformed supplied assets fail loading. The current host uses the recovered female stationary Attack6–9 clips. [Attachment and capsule provenance](../docs/audit/BAKED_KNIFE.md).

The 3600-frame fixture first completes the normal tutorial interaction sequence. It then follows the actual hare, stops within its configured 1.05 m approach distance and attacks through the same action/contact APIs. Success requires actual contacts, HP reaching zero, the one-time death receipt, acknowledgement of 300230 and both 719 intro lines, while conserving nine juices and the equipped knife. It does not set target HP or mission completion directly. **The 3600-frame graphics check passed:** eight attack starts produced 24 actual capsule contacts, hare HP fell from 212 to 0, and the run reached 719 with all six locomotion states exercised, nine juices, the owned/equipped knife and 500/500 player HP. It used real pursuit and room movement, without teleportation or direct HP assignment. [Completed sequence](../docs/reference/rebuilt-training-combat-complete.png). The standalone reconstructed-contact fixture, complete core tests, 18 CLI cases and ASan/UBSan/leak checks also pass. A second 3600-frame run repeated the same 24 contacts. The [first-contact pose](../docs/reference/rebuilt-training-melee-contact.png), captured after two display updates, shows coherent blade grip and hare placement with target HP 204/212. A successful original-client hit or kill remains unobserved.


## Training herbs and next-lesson boundary

The optional exporter recovers all five original herb meshes and the tree at their authored world transforms. Using the isolated UnityPy environment described above, write a fresh output directory (skip the export if these assets already exist):

```sh
rebuild/build/unity-tools/bin/python rebuild/tools/export_training_gathering_assets.py \
  --output rebuild/build/reference-assets/gathering
./rebuild/build/planet_explorers --training-room rebuild/build/reference-assets \
  --smoke 6000 --screenshot /tmp/pe-training-herbs.png
```

The 6000-frame fixture includes the full combat path, walks to all five actual herb positions, starts the same timed gathering actions used by E, and requires mission 748, five depleted herbs and exactly one safety axe 1527. It does not teleport or set herb completion directly. **The 6000-frame graphical check passed with exit 0:** mission 748, five herbs depleted and one axe, with nine juices, the owned/equipped knife and player HP 500/500 preserved. Combat again recorded eight attack starts, 24 contacts and hare HP 0/212; all six locomotion states ran. [Completed herb lesson](../docs/reference/rebuilt-training-herbs-complete.png). The complete core suite, 18 CLI cases and ASan/UBSan/leak checks passed (`/tmp/pe-gathering-validation.log`). The 3600-frame fixture intentionally stops at 719 without gathering input.

After acknowledging 4072/300231, E starts the nearest eligible herb within the source strict squared distance `<7`. The source attaches independent input listeners to every herb, so one press could start multiple overlapping eligible plants; the C frontend selects one, while the controller permits distinct concurrent actions. There is no knife-equipment restriction. Movement and new jumps are locked until the 1.1-second depletion, with a 1.966-second local action lifetime; gathering produces no inventory resource. The fifth depletion and the 748 axe grant commit together. Dead players cannot start gathering, and death cancels pending undepleted actions. The source 748 introduction 300233 is shown; this is **748, not 720**. Tree cutting and crafting are now connected and graphically verified below.

Rendering uses recovered diffuse geometry directly. The original hologram camera, glow/distortion, production fade and player gathering animation are not reproduced. No original-client herb interaction has been captured or verified. See [source gathering rules](../docs/audit/TRAINING_GATHERING.md).


The controller and frontend now pass the **9000-frame graphical sequence through748 → 757 → 721 → 751**. Safety Axe and Knife share source slot 16: equipping either returns the other to inventory. Drawing the axe sets the held state; the tree action requires held cutting input, a real player-forward 2m ray hit and an explicit animator-readiness signal. Seven timed hits grant one wood and the next lesson's script 406. Successful script use consumes it, learns recipe 1 and grants 20 wood, giving 21 total. Replicator Y opens the source instruction; starting the three-second recipe spends 10 wood. Completion produces one shovel, then 751 grants its second shovel, leaving 11 wood and two shovels. That 9000-frame fixture stops before mining input; the longer 12000-frame fixture continues through excavation to 752. Scripted tree damage does not invent stamina/durability costs; readiness and draw/stow remain host approximations; the three recovered Felling clips now drive matching body and axe poses. Blocked successor grants preserve earned items and paid work rather than duplicating or discarding them. Closing the replicator does not cancel crafting; cancellation and training persistence remain unimplemented.


## Install recovered cutting and shovel animations

These optional assets use closed, locally generated original-Unity captures. Follow [the supervised bake workflow](../docs/audit/ANIMATOR_BAKE.md) to obtain the ten-clip capture 02 or its validated repeat 06, cutting capture 08 and shovel capture 09. Capture files and original binaries are not bundled. Every weapon scene must be exported against the exact body scene it accompanies. The commands below document the historical ten-clip body from capture02; use the complete directional installer above for current locomotion groups, and do not overwrite a single current group with these historical outputs. Exporters require fresh output directories; if an export already exists, skip its generation and use the installation commands.

Recover the safety axe and build the separate cutting body:

```sh
rebuild/build/unity-tools/bin/python rebuild/tools/export_reference_axe.py
python3 rebuild/tools/analyze_reference_bake.py \
  "$HOME/.local/share/planet-explorers-bake-reference/pose-captures/female-clips-08.jsonl" \
  --rig rebuild/build/reference-player-starter/player-female-starter-rig.json \
  --report rebuild/build/animator-bake/female-clips-08-report.json \
  --export-dir rebuild/build/animator-bake/player-female-cut
python3 rebuild/tools/export_baked_axe.py \
  "$HOME/.local/share/planet-explorers-bake-reference/pose-captures/female-clips-02.jsonl" \
  --rig rebuild/build/reference-player-starter/player-female-starter-rig.json \
  --visual-assets rebuild/build/reference-assets/safety-axe \
  --body-scene rebuild/build/animator-bake/player-female/all/recording.peas \
  --output rebuild/build/animator-bake/player-female/axe-all
python3 rebuild/tools/export_baked_axe.py \
  "$HOME/.local/share/planet-explorers-bake-reference/pose-captures/female-clips-08.jsonl" \
  --rig rebuild/build/reference-player-starter/player-female-starter-rig.json \
  --visual-assets rebuild/build/reference-assets/safety-axe \
  --body-scene rebuild/build/animator-bake/player-female-cut/all/recording.peas \
  --output rebuild/build/animator-bake/player-female/axe-cut-all
mkdir -p rebuild/build/reference-assets/player-axe rebuild/build/reference-assets/player-cut rebuild/build/reference-assets/player-axe-cut
cp -a rebuild/build/animator-bake/player-female/axe-all/. rebuild/build/reference-assets/player-axe/
cp -a rebuild/build/animator-bake/player-female-cut/all/. rebuild/build/reference-assets/player-cut/
cp -a rebuild/build/animator-bake/player-female/axe-cut-all/. rebuild/build/reference-assets/player-axe-cut/
```

The cutting body/axe pair is required together and requires a matching locomotion axe scene. Its three named clips are Felling1–3. The tutorial host retains its independent entry/hit/exit timers; a recovered animation event does not replace scripted tree HP accounting. See [tree asset provenance](../docs/audit/TRAINING_TREE_ASSETS.md).

Recover the wooden shovel and separate digging body:

```sh
rebuild/build/unity-tools/bin/python rebuild/tools/export_reference_shovel.py
python3 rebuild/tools/analyze_reference_bake.py \
  "$HOME/.local/share/planet-explorers-bake-reference/pose-captures/female-clips-09.jsonl" \
  --rig rebuild/build/reference-player-starter/player-female-starter-rig.json \
  --report rebuild/build/animator-bake/female-clips-09-report.json \
  --export-dir rebuild/build/animator-bake/player-female-dig
python3 rebuild/tools/export_baked_shovel.py \
  "$HOME/.local/share/planet-explorers-bake-reference/pose-captures/female-clips-02.jsonl" \
  --rig rebuild/build/reference-player-starter/player-female-starter-rig.json \
  --visual-assets rebuild/build/reference-assets/wood-shovel \
  --body-scene rebuild/build/animator-bake/player-female/all/recording.peas \
  --output rebuild/build/animator-bake/player-female/shovel-all
python3 rebuild/tools/export_baked_shovel.py \
  "$HOME/.local/share/planet-explorers-bake-reference/pose-captures/female-clips-02.jsonl" \
  --rig rebuild/build/reference-player-starter/player-female-starter-rig.json \
  --visual-assets rebuild/build/reference-assets/wood-shovel \
  --body-scene rebuild/build/animator-bake/player-female/all/recording.peas \
  --mode stowed --output rebuild/build/animator-bake/player-female/shovel-stowed-all
python3 rebuild/tools/export_baked_shovel.py \
  "$HOME/.local/share/planet-explorers-bake-reference/pose-captures/female-clips-09.jsonl" \
  --rig rebuild/build/reference-player-starter/player-female-starter-rig.json \
  --visual-assets rebuild/build/reference-assets/wood-shovel \
  --body-scene rebuild/build/animator-bake/player-female-dig/all/recording.peas \
  --output rebuild/build/animator-bake/player-female/shovel-dig-all
mkdir -p rebuild/build/reference-assets/player-shovel rebuild/build/reference-assets/player-shovel-stowed rebuild/build/reference-assets/player-dig rebuild/build/reference-assets/player-shovel-dig
cp -a rebuild/build/animator-bake/player-female/shovel-all/. rebuild/build/reference-assets/player-shovel/
cp -a rebuild/build/animator-bake/player-female/shovel-stowed-all/. rebuild/build/reference-assets/player-shovel-stowed/
cp -a rebuild/build/animator-bake/player-female-dig/all/. rebuild/build/reference-assets/player-dig/
cp -a rebuild/build/animator-bake/player-female/shovel-dig-all/. rebuild/build/reference-assets/player-shovel-dig/
```

Training validates the complete four-directory shovel set together. The first two scenes share the body locomotion clocks (twenty in the current directional set; ten in the historical fixture below); the digging body/held shovel share exactly Spade1–3. Held/stowed attachments use source mountMain/mountBack. The separately exported stowed digging scene is an offline reference and is not required by the frontend. Static geometry and all digging frames have passed headless native sanitizer checks. Those asset checks are separate from the passing 12000-frame C graphical mining run below; an original-client dig outcome remains unobserved. Source state speed, controller transitions, root motion and MinePutOn/MinePutOff behavior remain separate from sampled authored poses; [shovel asset provenance](../docs/audit/SHOVEL_ASSETS.md) and [mining action rules](../docs/audit/TRAINING_MINING.md) describe the boundaries.


## Sandbox tree and wood-axe assets

The sandbox reuses the installed cutting/shovel body and tool groups above. Its tool transactions use the general source events, without the tutorial's seven-hit tree, mission grants or fixed mound. The recovered female body is required; supplied tool groups must be complete and match their body clocks. Axes currently have a fixed held attachment only, so stowed axes are invisible. [Tool presentation and exact source identity](../docs/audit/SANDBOX_TOOL_PRESENTATION.md) describe these limits.

The sandbox also requires `sandbox-world/tree.obj`, its material/texture and `tree-collider.pesc`. First recover `gathering` with the herb/tree exporter described above, then run:

```sh
rebuild/build/unity-tools/bin/python rebuild/tools/export_sandbox_world_assets.py
rebuild/build/unity-tools/bin/python rebuild/tools/export_sandbox_world_assets.py --check
```

This creates a fresh `rebuild/build/reference-assets/sandbox-world` directory, rebasing the recovered tree out of its tutorial placement into source-local meters. It verifies the matching source capsule and preserves provenance hashes. `--check` validates an existing export without rewriting it. All provisional sandbox trees share this model, while saved positions and remaining resource state determine placement and visibility. The capsule supports axe interaction rays and Adventure movement collision; depleted trees are excluded, and camera obstruction queries retain the original tree exclusion. The headless collision constants are checked into `generated/pe_tree_collider.h` and use the same source decoder as the visual PESC. They do not depend on recovered graphics or Unity at runtime. Regenerate or verify them with the optional UnityPy environment:

```sh
rebuild/build/unity-tools/bin/python rebuild/tools/export_tree_collision.py
rebuild/build/unity-tools/bin/python rebuild/tools/export_tree_collision.py --check
```

The check verifies the generated header and, when present, byte-identical `sandbox-world/tree-collider.pesc` source values. It requires the repository source prefab but no installed game. Normal native compilation does not need UnityPy.

Wood Axe26 uses its own original mesh. After the body and cutting scenes above are installed, generate these fresh outputs; do not relabel the Safety Axe scene:

```sh
rebuild/build/unity-tools/bin/python rebuild/tools/export_reference_axe.py --item-id 26
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

Each wood-axe scene contains the actual 275-vertex model bound to the same sampled `mountMain` used by its companion body. Existing destinations are preserved; use new output directories for regeneration, validate them, then deliberately install the matching set. Original game assets and sampled captures remain local prerequisites, not redistributed binaries.

## Training mining controls and verification

After crafting, use **I → Equipment** and right-click either Wood Shovel. Each keeps its own durability. Close the inventory and acknowledge Andy, then left-click to draw it. In third person, aim at the mound within three meters of the player and hold left mouse to dig. The highlighted sample is the actual source-style snapped target. Release ends the digging cycle; a short right click stows the shovel. First-person digging is currently unsupported.

```sh
make -C rebuild training-mining-smoke
```

The 12,000-frame graphical test passed through mission752: five shovel events damaged and removed three real samples, yielding six dirt, then the next lesson granted9,000. Final dirt9,006, wood 11, two shovel instances, nine juices and500/500HP were checked. The equipped shovel ended at3528/3553 durability; the second remained untouched at3553/3553. Terrain render/collision snapshots share the same marching-cubes mesh and update after edits. The source mound disappears on completion. [Digging capture](../docs/reference/rebuilt-training-mining.png). The new core, exporter,18CLI and sanitizer tests pass; source regeneration checks match.

The source animation poses are recovered, while state transitions are discrete and draw/stow is immediate. Original live digging outcomes, controller blending/IK and original terrain shading remain unverified. Training building controls and the real receiver evaluator now pass the 15,000-frame graphical sequence; sandbox construction remains separate. See [mining behavior](../docs/audit/TRAINING_MINING.md) and [shovel assets](../docs/audit/SHOVEL_ASSETS.md).


## Training construction and receiver puzzle

At 752, acknowledge the building introduction and press **B**. A successful build-mode opening queues the original completion dialogue, then accepts 753. After the receiver introduction, aim at the source terrain anchors or existing blocks:

| Control | Training construction action |
| --- | --- |
| B | Enter or leave Build Mode |
| Left mouse | Place one half-meter dirt cube |
| Alt + left mouse | Remove the targeted placed cube |
| T | Cycle cube rotation |

The training frontend currently exposes cubes only; the standalone Block45 module and sandbox also support ramps. Each one-cell placement costs one dirt; removing one cell refunds zero, following the source grouped ceiling/floor rules. Clicks are consumed once, including across fixed-step catch-up, and dialogue acknowledgements do not also place blocks. Actual cube geometry and receiver slabs participate in player collision.

The puzzle evaluates50 source rays against the nearest layer12 receiver, terrain or building geometry. A receiver hit is−100; any other hit or miss is+1, so all 50 must avoid their receivers. There is no placed-block-count shortcut. Success clears the authored anchors and lesson-written blocks without a cleanup refund, exits build mode and reaches **756**, whose creation controls remain unsupported. This source mission chain skips the older754/755 save/export lessons. [Source rules and limits](../docs/audit/TRAINING_BUILDING.md).

Optional original receiver meshes and indicator textures can be recovered locally:

```sh
rebuild/build/unity-tools/bin/python rebuild/tools/export_training_build_assets.py
make -C rebuild training-building-smoke
```

The 15,000-frame fixture **passed with exit 0 at mission 756**. It used the complete prior lesson sequence, 19 actual cube placements and one removal, reached receiver score 50 and cleared all lesson cells and terrain triangles. Dirt ended at 8,987:9,006 minus 19 single-cell placement costs, with no refund for the one-cell removal or lesson cleanup. Wood 11, two shovels (equipped durability 3528/3553; second 3553/3553), nine juices and HP 500/500 were preserved. Combat again recorded eight attack starts and 24 real contacts, ending hare HP 0/212. [Construction in progress](../docs/reference/rebuilt-training-building.png) and [completed construction lesson](../docs/reference/rebuilt-training-building-complete.png). This validates the C sequence, not an original-client construction outcome or complete game parity. The core receiver/mission/collision regressions and strict sanitizer checks pass. Missing optional receiver art falls back to matching plain boxes; the original hologram shader and particle presentation are not reproduced.


### Tutorial NPC validation

All nine NPC models and per-Avatar idle clips are installed in the current local asset set. Run `make -C rebuild training-npcs-smoke` for the 1750-frame storage/Andy sequence and camera survey of all nine characters. The check requires the complete NPC group, validates progression through mission 746, and saves individual screenshots under `rebuild/build/smoke-training-npcs.png-npc-ID.png`. It passed with9NPCs, 9 juices and the owned/equipped knife. Source recovery, regeneration commands and remaining behavioral limitations are recorded in the [NPC audit](../docs/audit/TRAINING_NPCS.md).

## Sandbox pickaxe assets

Wood Pickaxe19 requires the complete `player-pickaxe`, `player-pickaxe-stowed`, `player-mine` and `player-pickaxe-mine` groups beneath the reference-assets root. They are installed locally from the original mesh and the closed91-pose Mine capture. Held/stowed locomotion now uses the coherent twenty-clip directional body capture; action scenes use only Mine1–3 with matching body/tool clocks. Missing whole groups disable that tool action; partial or mismatched groups fail validation before upload. The [pickaxe recovery audit](../docs/audit/SANDBOX_PICKAXE.md) records exact provenance, fresh-directory export commands and remaining source-transition limitations. `rebuild/build/sandbox_action_fixture --mine OUTPUT.pesave` creates an isolated item19 physical-test fixture through the same gameplay APIs as the other tools.

### Supplier interaction check

With the recovered supplier scene installed and the desktop unlocked:

```sh
make -C rebuild all build/adventure_supplier_fixture
python3 rebuild/tools/playtest_adventure_supplier.py
```

This generates an isolated fresh Adventure fixture and tests proximity, cancellation, source dialogue, one-time supplies and save/reload through physical input. It does not open Steam or normal saves. See the [recorded result](../docs/reference/sandbox/rebuild-supplier-result.json) and [playtest](../docs/PLAYTEST.md#october-5-adventure-supplier-interaction).

### Town Defense reference observer

`python3 rebuild/tools/reference_town_defense.py` from the repository root compiles the observation-only9077 beacon probe without launching a game. The [observer contract and commands](../docs/audit/ADVENTURE_TOWN_DEFENSE_OBSERVER.md) cover an explicit isolated launch, fresh arm/stop files, bounded capture, strict wave analysis and archive preservation. It does not enable native Town Defense; creature recovery and full spawn-area coverage remain separate gates.

The shield-ownership increment introduced **PECSv15**, retaining the v14 layout plus a four-byte shield instance reference. Current saves use PECSv20 and retain that shield record. The bag alone owns its item34 durability; older saves infer no selected shield. This is a tested equipment foundation, with normal shield use still gated pending presentation and combat integration. See [shield ownership](../docs/audit/SHIELD_OWNERSHIP.md).


### Offline attack playback and shield action checks

Admitted Repulsed clocks/requests are a disconnected combat foundation; live hit reactions remain gated. `python3 rebuild/tools/run_tests.py -k repulsed` runs its core and independent arithmetic checks from the repository root. `make -C rebuild repulsed-source-check` checks its exporter and all source/prefab cases with UnityPy. [Source contract and physical boundary](../docs/audit/REPULSED_ACTION.md).

HumanPhyCtrl request consumption is likewise a disconnected foundation. `python3 rebuild/tools/run_tests.py -k human_phy` runs its core, evidence and independent arithmetic checks; `make -C rebuild human-phy-source-check` adds the UnityPy prefab cases and compares the committed CIL excerpt with the installed assemblies. `python3 rebuild/tools/cil_disasm.py INVENTORY ASSEMBLY Type` prints decoded method bodies from a saved REA member inventory. [Contract and engine boundary](../docs/audit/HUMAN_PHY_CTRL.md).

Halt and its foot-stop effect (`python3 rebuild/tools/run_tests.py -k halt`, `make -C rebuild halt-source-check`) are live in both hosts since October 13: after releasing movement with empty hands, Move and Jump are refused for 0.22–0.9 s and run or sprint stops draw a lunge. Each host's smoke summary prints its stop and locked-step counts. [Audit](../docs/audit/HALT_STOP_EFFECT.md#live-in-both-hosts).

The Whacked lifecycle and the hit/Repulsed clip selection are a third disconnected foundation: `python3 rebuild/tools/run_tests.py -k whacked -k reaction` runs the core and offline exporter cases, and `make -C rebuild reaction-presentation-source-check` adds the installed-controller cases. The reaction poses themselves are not baked; [the contract](../docs/audit/REACTION_PRESENTATION.md) gives the prepared `--preset hit-reaction` commands, which build only unless `--launch` is added.

The sandbox now applies the recovered0.65s contact-response curve to its shared melee interval. [Playback evidence](../docs/audit/ATTACK_PLAYBACK.md) separates engine arithmetic qualification, trigger notification and skill damage; no attack pose rebake is required. [Shield action admission](../docs/audit/SHIELD_ACTION.md) is a tested headless foundation only; ordinary shield equipment remains gated pending composed poses. That playback increment retained PECSv16; the subsequent wallet increment uses v17. No physical playtest of those changes is claimed.

From the repository root, run the prefab/export checks with the installed offline UnityPy environment:

```sh
make -C rebuild attack-playback-source-check ASSET_PYTHON="$PWD/rebuild/build/unity-tools/bin/python"
```

The optional [SHA-pinned arithmetic replay](../docs/audit/UNITY5_CURVE_EVALUATOR.md) reads the installed native binary without starting its engine. Ordinary optimized core tests retain native golden values and exercise shared action-clock integration.


### Offline humanoid rotation research

`make -C rebuild humanoid-rotation-test` from the root runs the experimental C arithmetic and CPU-only channel/mapping/provenance tests. The [697-pose comparison](../docs/audit/HUMANOID_ROTATION_PROOF.md) uses existing captures, source muscle channels, exact Avatar axes and source loop-pose correction. It still reports12 unresolved limb bones and does not install gameplay assets. [Oracle instructions](../docs/audit/UNITY5_HUMAN_ROTATION.md) describe the optional SHA-pinned arithmetic replay without engine initialization.


### Vendor selling and buyback

Open an archived vendor's Shop, then choose Buy, Sell or Buyback. Scroll the list, select an item and quantity, and use the action button. Selling worn equipment preserves its identity and condition for repurchase. Unequip selected weapons/tools first. Buyback lists survive closing the shop but clear when you load or start a new game; saving alone does not clear them. Player and vendor funds persist in current PECSv20 saves through the retained v17 wallet records. The native limit is32 buyback entries per vendor; unsupported condition-dependent items cannot be sold.

`make -C rebuild town-trade-test sandbox-shop-trade-ui-test` from the repository root checks transactions, save/session lifecycle and modal input without opening a graphics context. See [the trade contract](../docs/audit/TOWN_SELL_BUYBACK.md) for source evidence and pending graphical acceptance.


### Construction history checks

`make -C rebuild build-history-test sandbox-build-history-host-test` checks cube/ramp undo/redo, materials, conflict/capacity rollback, transient save lifecycle and the actual sandbox input/collision adapter without graphics. The [contract](../docs/audit/CONSTRUCTION_HISTORY.md) separates source rules from bounded native policies and the pending manual check.


### Fixed vegetation examples

New Sandbox now includes two cuttable trees, six herbs and two decorative foliage plants near the start. Get within2m, point at a herb, then press E or briefly right-click to gather. Use the Safety Axe for trees. Harvested plants and felled trees stay depleted after F9 save/F10 load. Placement stays fixed and regrowth remains deferred. Herb production now includes source fallback items and optional seeds; the single saved gameplay RNG makes retries and save/reload deterministic without claiming Unity RNG-sequence equivalence. The recovered tree uses prototype48 hardness: a fresh unit tree takes19 Safety Axe contacts or12 Wood Axe contacts and yields16 Wood plus20 combined Resin/Rubber with either supported axe. Gathering remains immediate; the original Gather animation/event timing is not implemented. See [harvesting fidelity](../docs/audit/SANDBOX_HARVESTING.md).

To add the patch once near your current position in an existing Adventure save, run `rebuild/build/planet_explorers --load YOUR_CHECKPOINT.pesave --vegetation-examples --save rebuild/build/vegetation-test.pesave` from the root. The separate save destination preserves your input checkpoint. Repeating the option does not replenish the patch. See [fixed vegetation](../docs/audit/SANDBOX_VEGETATION.md) for asset recovery, headless checks, limitations and a disposable manual fixture.


### Camp repair

The existing **Repair Machine** in the starting camp now provides the bounded [camp repair service](../docs/audit/CAMP_REPAIR.md). Aim at its actual box within8m and press **E** or use a short **RMB** click. It is at source(-613.5,29.5,404) in both supported settlement crops. Intervening geometry blocks interaction; unavailable model/collision assets or other worlds do not receive a substitute machine.

Unequip an eligible tool or weapon first. Select it in the window's bag list, then choose **Insert selected**, press Enter, or drag it into the machine slot. Review the displayed material costs, choose **Repair**, and confirm. The machine retains the same item through a60-second repair; close the window and continue playing if desired, then reopen it and choose **Take item**. Busy items cannot be replaced or taken. **Cancel repair** requires confirmation and keeps the item without refunding materials. Escape/Close and walking beyond8m close only the window. Drag its title to reposition it. The panel consumes world controls and held/released pointer input; it does not stop the repair counter. Pause, focus loss and the main menu suspend the counter; death does not. Save/load preserves the inserted item and remaining ticks, with no elapsed-time credit while the game is closed.

The list contains supported ordinary bag items with source material profiles. The starter Knife33 and Safety Axe1527 have no such materials and cannot be repaired here. Worn starter clothing cannot be removed into this service; LifeLimit/creation items and arbitrary machine placement remain unsupported. Successful insertion clears shortcuts to that exact item; taking it back does not recreate them. Make room in its source bag category before taking it back. For a different world import, first empty the machine. Native8m admission, strict occlusion and a movable simplified service panel are documented adapters; this does not claim pixel-perfect UI or animated machine presentation.

Run `make -C rebuild camp-repair-test camp-repair-save-test camp-repair-interaction-test sandbox-repair-ui-test sandbox-repair-host-test` for the owner, persistence, spatial and UI/host integration checks. Integration fixtures require the installed reference assets. `camp-repair-source-check` additionally requires the local UnityPy environment and preserved source/Steam/archive inputs. [Spatial tests and source picking](../docs/audit/CAMP_REPAIR_INTERACTION.md) cover both installed crops without launching the game. Current integration validation is recorded in [PLAYTEST](../docs/PLAYTEST.md), separately from outstanding visual/manual acceptance. [Camera geometry](../docs/audit/CAMERA_GEOMETRY_GATE.md) documents the remaining near-plane/query prerequisites using a CPU-only probe.

### Isolated automated native acceptance

[The private-input supervisor](../docs/audit/NATIVE_PRIVATE_INPUT.md) starts the normal game on a dedicated Xvfb display using an unchanged copy of an earned checkpoint. It provides bounded ordinary input, screenshots and a journal without taking the user desktop. It does not fabricate gameplay or launch the Steam original. Run `make -C rebuild hunt-save-inspector-test native-private-input-test` for26 focused tooling cases; these targets never open graphics. Actual private sessions are explicit operator actions.

Grounded female third-person Knife sprint attacks are supported when the
complete optional sprint group is installed: restart the sandbox, draw Knife
with LMB, hold Shift+W until sprinting, then make a fresh LMB press. See
[the sprint audit](../docs/audit/SANDBOX_SPRINT_KNIFE.md) for qualified assets,
backups, `sprint-attack-test` and current acceptance limits.

Disconnected AnimFollow numerical drive/rig requests are documented in
[the drive audit](../docs/audit/ANIMFOLLOW_DRIVE.md). Run
`python3 rebuild/tools/run_tests.py -k core:animfollow_drive` from the root,
or `make -C rebuild animfollow-drive-source-check` for installed evidence and
regeneration. Neither target enables physical reactions or get-up gameplay.

The disconnected FinalIK full-body solver is documented in
[the solver audit](../docs/audit/FINALIK_FBBIK.md). Run
`python3 rebuild/tools/run_tests.py -k fbbik` from the root, or
`make -C rebuild fbbik-source-check` for installed evidence, prefab
regeneration and the C comparison. It produces no visible pose in either host.

The disconnected FinalIK grounder is documented in
[the grounder audit](../docs/audit/FINALIK_GROUNDER.md). Run
`python3 rebuild/tools/run_tests.py -k grounder` from the root, or
`make -C rebuild grounder-source-check` for installed evidence and settings
regeneration. No host calls it.
`python3 rebuild/tools/reference_grounder_probe.py` only compiles the
observation probe; `--launch` starts the original and needs the user present.
`make -C rebuild grounder-probe-test` checks the probe and its analyzer offline.

Animated solver poses for the staged player clips are documented in
[the pose audit](../docs/audit/PLAYER_IK_POSE.md). Run
`python3 rebuild/tools/run_tests.py -k ik_pose`; the real-pose test needs the
staged `player-ik-pose` group and is skipped without it.

Both hosts draw the player through the zero-offset solve when the
`player-ik-pose` and `player-ik-drivers` groups are installed; see
[the always-on solve](../docs/audit/PLAYER_IK_DRIVE.md). Run
`python3 rebuild/tools/run_tests.py -k ik_drive` and
`make -C rebuild actor-blend-test sandbox-actor-test`. Each host's smoke
summary prints how many poses were drawn through the solve.
