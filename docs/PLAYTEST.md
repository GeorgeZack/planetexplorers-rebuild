# Playtest and acceptance index

This file indexes what has been **observed**: in the original client, in the native production game with ordinary input, or in native graphical fixtures. Each linked audit owns its details, limits and provenance. Claims are labelled source-derived, observed in the original, implemented in C, or provisional. The full chronological log before October 15, 2026 is in git history.

## Native production game, ordinary input

Unless stated otherwise: Adventure, 1280×720, private Xvfb display, earned saves, no original launch.

| Feature | Observation | Owner and evidence |
|---|---|---|
| Durable-tool discard | Oct 15, binary `f5c07847…a8e`, save `ca79fffb…866c`. Pickaxe instance 11 was bound to quickslot 5. Delete → Escape closed only the prompt; Enter removed only instance 11 and its quickslot reference. | [discard](audit/INVENTORY_DISCARD.md#durable-tool-with-a-quickslot-session-01-october-15), [evidence](reference/discard-native-20261015/checkpoints.json) |
| Inventory editing | Oct 14. Split, swap without merging, Resource and Equipment sort, and Delete → Discard of the split stack only. Shift+drop merge not exercised. | [inventory](audit/SANDBOX_INVENTORY.md#ordinary-native-acceptance), [discard](audit/INVENTORY_DISCARD.md#ordinary-native-acceptance), [evidence](reference/inventory-native-20261014/checkpoints.json) |
| Camp repair | Oct 14. Shovel 10 quoted Wood 1 and 60 s, partial-job reload byte-identical, completion with the panel closed. Cancel had no refund; same-category replacement worked. | [repair](audit/CAMP_REPAIR.md#ordinary-native-acceptance), [cancel](audit/CAMP_REPAIR.md#cancellation-and-same-category-replacement-session-02), [evidence](reference/repair-native-20261014/checkpoints.json), [cancel evidence](reference/repair-cancel-20261014/checkpoints.json) |
| Construction history | Oct 14. Single and batch undo/redo, save and B re-entry keep history, Pause blocks it, load clears it. Rotated-ramp replay worked; ramp contact and moving replay are unproven. | [history](audit/CONSTRUCTION_HISTORY.md#ordinary-native-acceptance), [ramp](audit/CONSTRUCTION_HISTORY.md#rotated-ramp-replay-session-03), [evidence](reference/build-native-20261014/checkpoints.json), [ramp evidence](reference/build-native-20261014-ramp/checkpoints.json) |
| Selling and buyback | Oct 14. Worn shovel sold for 41 and bought back for 41 while the vendor was credited 42; Wood rows survived reopening; Load cleared the buyback. | [trade](audit/TOWN_SELL_BUYBACK.md#ordinary-native-acceptance), [evidence](reference/trade-native-20261014/checkpoints.json) |
| Food and hunger | Oct 14. Biscuits bought, eaten from the bag and from quickslot 4; retries consumed nothing. Reload restored the saved hunger value, inventory and quickslot; the food cooldown is transient and resets on load. The hunger-bar fill matched its formula. | [hunger](audit/HUNGER.md#ordinary-native-acceptance-of-the-food-loop), [evidence](reference/food-native-20261014/checkpoints.json) |
| Four-hare hunt | Oct 7. Earned kills, a byte-identical partial reload, the return, exactly one reward set and Bug Hunt completed. | [acceptance](audit/HUNT_ORDINARY_ACCEPTANCE.md) |
| Melee facing | Oct 7, `:104`. Hare 2146483653 was acquired at frame 5785 with camera yaw unchanged; 28 outgoing and 10 incoming contacts. | [facing](audit/MELEE_TARGET_FACING.md), [manifest](reference/melee-facing/check.json) |
| Conversation history | Oct 7, `:103`. A saved row was cleared and restored by F10; RMB on Tammana opened her menu. | [history](audit/CONVERSATION_HISTORY.md), [log](reference/conversation-camera/restored-after-clear.png) |
| Shared UI, history, camera | Oct 7: the user reported "looks good" for the batch, without a per-scenario record. | [shared UI](audit/SHARED_UI.md) |
| Consumables during movement | Oct 6. Med Pack and Juice taken while holding W or on autorun kept the running pose ([frame](reference/consumable-autorun-fixed.png)). Exact contact on the use frame is covered only headlessly. | [consumables](audit/SANDBOX_CONSUMABLES.md) |
| Free-mode facing | Oct 6. A/D/S running and Z-walking turned the body toward travel, and stopping kept the heading. Transition grids show intermediate poses; this is not blend-graph parity. | [result](reference/movement-facing-blend-result.json), [grid](reference/movement-tutorial-transition-grid.png) |
| Oct 5 sandbox | Supplier, welcome, settlement, timed tools and trees, Wood Pickaxe Mine, Torch and medicine continuation. | [supplier](audit/ADVENTURE_NPC.md#october-5-physical-acceptance), [welcome](audit/ADVENTURE_WELCOME.md), [settlement](audit/ADVENTURE_SETTLEMENT.md), [pickaxe](audit/SANDBOX_PICKAXE.md), [post-medicine](audit/ADVENTURE_POST_MEDICINE.md) |
| Tree collision, v4→v5 save, zoom | Oct 5, physical C input. Tree 7 stopped forward motion, with 0.001 m advance on a further 0.2 s hold. Six axe strikes depleted it for 8 Wood and the trunk could be crossed. A v4 save inside the tree reported "moved 0.55m" on load. Three wheel notches set 6 → 3.6 m, and wheel input over the panel was ignored. | [result](reference/sandbox/rebuild-tree-result.json), [hashes](reference/sandbox/tree-rebuild-evidence.json) |
| Route to Annot, shops | Oct 5, physical C input. 25 residents, 3 shops with 17 offers; the walked route reached source (−279.43, 22.50, 144.90) and Naoko's 4096 reply; Cherish's purchase persisted. | [checks](reference/town-route-graphical.json), [headless route](reference/annot-route-rebuild-headless.json) |
| Annot medicine | Oct 5. Decline then accept; the final 4100 click gave 925×10 and 554×1 with currency 395; Tammana's catalog scrolled and clamped; a Torch was bought from Sherley. | [inspection](reference/medicine-rebuild-graphical.json), [medicine](audit/ADVENTURE_MEDICINE.md) |
| Movable panels | Oct 5, both hosts at 1280×720 and 1000×700. Title-bar drag, moved close buttons and resize clamping. The quest tracker clears the 208-pixel minimap. | [evidence](reference/ui-windows/evidence.json) |
| Menu routes | Oct 5. New Sandbox → Tutorial → Continue Sandbox in one process kept separate state, and the app exited 0. | [log](reference/sandbox/rebuild-menu-routes.log) |

Also checked natively with ordinary input, details in each owner: sprint Knife, Oct 12 ([sprint](audit/SANDBOX_SPRINT_KNIFE.md)); running Knife ([running](audit/SANDBOX_RUNNING_KNIFE.md)); Med Pack 925 and Tab/F5 ([consumables](audit/SANDBOX_CONSUMABLES.md)); directional strafing ([directional](audit/DIRECTIONAL_PLAYER.md)); starter outfit wear ([outfit](audit/STARTER_OUTFIT.md)); numbered responses ([numbered](audit/NUMBERED_CONVERSATIONS.md)); camera modes and hunt return ([camera](audit/SANDBOX_CAMERA.md), [return](audit/ADVENTURE_HUNT_RETURN.md)). All except the sprint Knife are from Oct 6.

Implemented in C and run in both hosts since Oct 13, but with no user observation: the [Halt lock](audit/HALT_STOP_EFFECT.md#live-in-both-hosts) (source-derived) and the [FinalIK drive and grounder](audit/FINALIK_GROUNDER.md#live-in-both-hosts). A user look at the grounder while standing on a slope is still pending.

## Original client

Steam-installed native V1.1.4 client with isolated profiles ([runtime](REFERENCE_RUNTIME.md)). Protected manual-save hashes were unchanged unless stated.

| Topic | Observation | Owner and evidence |
|---|---|---|
| Tutorial | Observed behaviour and the lesson order. | [tutorial](audit/TUTORIAL.md) |
| Tree obstruction | Oct 5. In third-person locked camera, forward movement stopped at a trunk at P603,295 through a further 2 s of W. This is qualitative, not a capsule measurement. | [session](reference/sandbox/tree-reference-session.json), [contact](reference/sandbox/original-tree-contact.png) |
| Route to Annot | Oct 5. 338 stable LOD0 chunks sampled into 416×64×416 at source (−640, 0, 64), maximum occupied elevation 29. Entering Annot showed "Mission Completed: The Delayed Supplies". Naoko's menu named Haleiwa and offered the two medicine choices; neither was accepted. | [path](reference/annot-route-original-walk.json), [choices](reference/annot-original-medicine-choices.png) |
| Medicine 9072 | Oct 5. Decline, then accept: "Herb Collecting, Gather 5 units of Herbal Drink 0/5". The pink script vanished on use. The save shows 9072 active, the grant once, formulas 337/388/390 learned and 877 absent. Quit returned 137. | [archive](reference/medicine-original-save.json), [delivery timing](audit/ADVENTURE_MEDICINE.md#october-5-original-delivery-timing-and-retained-native-policy) |
| Hare hunt | Oct 5, reloads and town return with two kills. | [encounter](audit/ADVENTURE_HUNT_ENCOUNTER.md), [combat](audit/ADVENTURE_HUNT_COMBAT.md), [observed](reference/hunt-combat-observed.json) |
| User-operated combat and crash | Oct 6, the user as sole operator, a 30 fps region recording (segment 012, 155.7 s, SHA256 `c62078e7…7c62`). Bug Hunt 3/4 at 65 s, then return. Jet Pack, Parachute and Glider script notices, and Rope Gun. Town Defense counted 0/15 → 7/15 by 143.4 s. The game then aborted with SIGABRT after a native SIGSEGV in the NVIDIA GL path, the same signature as an earlier crash. Slots 5–7 were intact. | [crash](reference/user-playtest-crash-20261006.json), [defense](reference/user-defense-seven.png), [diagnosis](REFERENCE_RUNTIME.md) |
| Town Defense reference | Oct 6. | [defense](audit/ADVENTURE_TOWN_DEFENSE.md), [diagnostics](reference/reference-render-diagnostics-20261006.json) |
| Running-attack demonstration | Oct 6, the user operating with CPU terrain, no MSAA and single-threaded rendering. 12.3 s and 70.8 s silent recordings of separated swings and chains. These are requested inputs, not measured branches or timing. | [record](reference/running-knife-user-result.json), [chain](reference/running-knife-user-chain.png) |
| Sword, shield and backward poses | Oct 6, isolated sampler and Zink captures. | [pose group](audit/STARTER_SWORD_POSE_GROUP.md), [shield layer](audit/SHIELD_LAYER_CAPTURE.md), [Zink](reference/shield-observer-zink-20261006.json) |
| Reaction pose bake | Oct 12. | [reaction](audit/REACTION_PRESENTATION.md#bake-result-and-staged-group), [result](reference/reaction-pose-result.json) |
| FinalIK solver and grounder | Oct 12–13. | [FBBIK](audit/FINALIK_FBBIK.md), [grounder](audit/FINALIK_GROUNDER.md#observed-on-uneven-ground) |
| Gather | Oct 15. Four Gather cycles with event receipts, and the Herb leaf bake. | [captures](audit/GATHER_LIVE_CAPTURE.md) |

## Tutorial native graphical fixtures

Scripted fixtures with real controller movement; no position or HP was assigned to force progress. Each stage kept the earlier ones intact.

| Stage | Result |
|---|---|
| Interaction (1,500-frame smoke plus a real keyboard and mouse session) | Storage, Andy and quickslot 1 reached mission 745 with nine Juices after 12,753 frames. A drop outside the slot changed nothing. [Sequence](reference/rebuilt-items-complete.png) |
| Stationary Knife (3,600 frames) | Mission 719; eight attack starts and 24 capsule contacts; hare HP 0/212; player HP 500/500. [Capture](reference/rebuilt-training-combat-complete.png) |
| Herbs (6,000) | Mission 748 with five depletions and safety axe 1527. Nearest-herb input is native; source listeners are independent. [Capture](reference/rebuilt-training-herbs-complete.png) |
| Tree and crafting (9,000) | Mission 751: 11 Wood, two shovels, recipe 1 learned. Felling uses Felling1–3 baked clocks. [Capture](reference/rebuilt-training-crafting-complete.png) |
| Mining (12,000) | Mission 752: five swings at frames 3857–4177, three removals, 9,006 dirt, shovel durabilities 3528 and 3553. [Capture](reference/rebuilt-training-mining-complete.png) |
| Construction (15,000) | Mission 756: 19 additions and 1 removal, 8,987 dirt, receiver score 50. [Result](reference/rebuilt-training-building-result.txt) |
| Recovered UI (15,000 rerun) | The same end state with the atlas UI. Manual I/C/Q/Y, drag-rotation and Replicator checks passed. [Result](reference/rebuilt-ui-result.txt) |
| NPCs | [Nine tutorial NPCs](audit/NPC_IDLE_SAMPLING.md) |

Native tutorial corrections from the original comparison: walk/run/sprint at 2/5/8 m/s, sprint entry at 15 stamina, sprint cost 5/s, jump cost 10, Z toggles walk, middle mouse toggles autorun. Terrain picking uses the rendered marching-cubes triangles. Mining uses source centred sample boxes.

## Reference input notes

[reference_input.py](../rebuild/tools/reference_input.py) targets only the original window. Stable X11 keycodes through XTest work with this Unity player, while remapped Wayland key injection misreads F5. Clicks use Wayland virtual-pointer events with a 150 ms hold, inside the source's strict 250 ms right-click limit. Drag coordinates belong to the captured 1280×720 layout. Closing the inventory can leave a tutorial tip that blocks camera look, so a look command is not proof the camera rotated.

## Focused automated foundations

These audits rest on source, focused automated tests and, in some cases, archived original evidence such as realized wallets. Their tests add no physical acceptance: [passive recovery](audit/PASSIVE_RECOVERY.md), [stamina](audit/STAMINA_RECOVERY.md), [capsule history](audit/MELEE_CAPSULE_HISTORY.md), [attack playback](audit/ATTACK_PLAYBACK.md), [rotation proof](audit/HUMANOID_ROTATION_PROOF.md), [revival](audit/SANDBOX_REVIVAL.md), [sword policy](audit/SWORD_MOTION_POLICY.md), [walking Knife](audit/SANDBOX_WALKING_KNIFE.md), [wallets](audit/TOWN_WALLETS.md), [vegetation](audit/SANDBOX_VEGETATION.md), [harvesting](audit/SANDBOX_HARVESTING.md), [camera environment](audit/CAMERA_ENVIRONMENT.md), [mission events](audit/ADVENTURE_MISSION_EVENTS.md), [hare loot](audit/HUNT_LOOT.md), [debug tools](audit/DEBUG_TOOLS.md), [actor materials](audit/ACTOR_MATERIALS.md), [Repulsed](audit/REPULSED_ACTION.md), [ragdoll actions](audit/RAGDOLL_ACTIONS.md), [hit IK](audit/HIT_IK.md), [HumanPhyCtrl](audit/HUMAN_PHY_CTRL.md), [lunge motor](audit/SWORD_LUNGE_MOTOR.md), [comfort HUD](audit/COMFORT_HUD.md), [building materials](audit/BUILDING_MATERIALS.md).
