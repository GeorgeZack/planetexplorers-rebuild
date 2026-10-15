# Feature contracts

Read the relevant section and its linked audits before editing. Both are binding: this file owns cross-module invariants; audits own detailed source algorithms, numeric tables and evidence. Update both when supported behavior changes. [AGENTS](../AGENTS.md) owns general rules.

## Project status and increment history

[REPLICATION](REPLICATION.md) owns current scope, [HANDOFF](HANDOFF.md) next work, and [PLAYTEST](PLAYTEST.md) observations. Historical test counts are not current coverage. Tutorial behavior never automatically transfers to Adventure.

## Save format

Current format: **PECSv23**. Every version preserves the prior literal payload and appends the fixed suffix below, except the documented v12 reinterpretation. Validate before migration; malformed input leaves live gameplay and transient owners unchanged. Never fabricate progress, wear or history. Unused fixed records must be canonical zero.

| Version | Suffix and compatibility |
|---|---|
| 23 | 8 bytes: hunger f32 in 0..500, starving u32 0/1. Older saves: prototype hunger 300, not starving. Drain/remainder/condition timers and food cooldown remain transient. [Hunger](audit/HUNGER.md) |
| 22 | 2,052 bytes: count and 64×32-byte world loot records. Older saves: no loot. [Loot](audit/HUNT_LOOT.md) |
| 21 | 84,484 bytes: count and twenty 128-byte speaker/4096-byte content records. Reject malformed UTF-8, tails, duplicates and nonzero unused records; older history empty. [History](audit/CONVERSATION_HISTORY.md) |
| 20 | 32 bytes: machine u32; item/count/instance u32; durability f32; running u32; remaining ticks u64 low/high. Empty all zero. Entity 20006 owns one qualified nonaliased item in a supported Adventure crop. Idle ticks 0; running 1..6000000 with damaged item; bounded high word 0. Older saves: empty machine. [Repair](audit/CAMP_REPAIR.md) |
| 19 | 26,888 bytes: layout/count and 1680×16-byte item/count/instance/wear records. Four 420-cell Adventure categories; retained 32-cell prefix is a validated occupied-cell projection, not downgrade support. Validate old flat bags before stable category migration; preserve IDs, wear, quickslots and counter. Legacy/Tutorial stay 32 cells. [Inventory](audit/SANDBOX_INVENTORY.md) |
| 18 | 648 bytes: installed/count and 32×20-byte prototype/XYZ/depleted records. Older saves: no plant patch, retain trees. [Vegetation](audit/SANDBOX_VEGETATION.md) |
| 17 | 100 bytes: count 8 and stable-order entity/balance/added u32 triples. Balances nonnegative signed32; latches 0/1; non-Adventure zero. Older Adventure initializes archived purses/latches, not purchase history. [Wallets](audit/TOWN_WALLETS.md) |
| 16 | 4-byte raw Comfort f32, finite 0..500; older default 500. Summed delay/buff phases/timestamps transient. |
| 15 | 4-byte shield-instance selection; bag remains sole item/wear owner. Validate Wood Shield34 and reject primary/outfit/deployed aliases; exhausted shield remains owned with zero defense/protection. Older: no shield. Normal use gated. [Shield](audit/SHIELD_OWNERSHIP.md) |
| 14 | 36 bytes: three item/instance/wear clothing records. Older valid Adventure initializes intact visual starter outfit and advances counter by 3; no historical wear reconstruction. Never repair malformed/missing/broken v14 records. Legacy zero. [Outfit](audit/STARTER_OUTFIT.md) |
| 13 | 4-byte medpack cooldown f32, finite 0..10; older default 0. |
| 12 | No bytes; hunt states 4/5 preserve pending 4105/4106, 6 completed. Earlier decoders reject these states. |
| 11 | u32 hunt kills. Older default 0; none/travel require 0, encounter state2 permits 0–3, states3–6 require 4. Pre-v11 cannot hold state3. Creature HP/threat/clocks transient. |
| 10 | 1,812 bytes: hunt state/XYZ, deployed count and 64×28-byte exact deployment records. Older: no hunt/deployment. Reject bag/equipment/quickslot aliases. |
| 8–9 | v8 adds dimensions, nonlegacy world payload, Annot completion and 17 stable stock records. v9 adds medicine state and count267 plus 267 stocks. Earlier saves infer incomplete/uninitialized new state. |
| 1–7 | v1–6 load depth64; v7 retains depth64/128. Welcome page0..9: 0..8 disallow active Arrival/supplies, 9 enables 9031/9032. v4/5 Adventure infer page9, Legacy0; v1–3 migrate Legacy. Only v4 Adventure permits the bounded geometry-swept tree-overlap relocation; v5 already satisfies collision. Never generalize migration into movement or discard saved velocity/clocks. |

[Route/save](audit/ADVENTURE_ROUTE.md) owns older layouts. Use inventory capacity/category APIs, not the legacy `PE_INVENTORY_SLOTS=32` in general hosts. Repair loads never reconstruct items or debit materials again. Conversation history records formatted presented pages and actual selected responses; its transition latch survives Clear, while window/scroll/input state is transient. Tutorial history is session-local; drawing never mutates history/quests.

## Actor presentation and locomotion

[Directional/facing](audit/PLAYER_STRAFING.md): select by measured velocity in body space, dominant axis with forward/back ties; preserve accepted jump edge and continuous gait phase. Free third person turns toward travel and retains stopped heading; captured modes face camera. Body yaw is transient, independent of saved camera yaw. Source-rate6/body-right turn above150° and .12s pose-weight smoothing are native adapters. Body, attachments, Torch mounts and incoming defence share weights/clocks/yaw. Exact tool/melee actions override locomotion and lock facing; load resets Idle/facing.

[Complete groups](audit/DIRECTIONAL_PLAYER.md): require all twelve named locomotion clips and matching attachment/defence clocks. Never overwrite newer running/sprint extensions with an old ten/twenty-clip export. Stage coherent groups and back up installations. Native strafe selection is not the full source Animator.

[Knife recovery](audit/KNIFE_RECOVERY.md): natural completion retains a native 150ms inactive pose at rate1; shared body/attachment/defence snapshot reserves contributor13 beyond twelve locomotion leaves. No damage/root/combo change. Cancellation, stow, death, airborne, reset/load or instance change clears it; pause freezes it.

[Materials](audit/ACTOR_MATERIALS.md): shared actor renderer and source-bound PEAM1 own alpha/culling. Hair discards before depth; opaque eye alpha is gloss. Active shader replacements/cutoff outrank stale saved fields. Preserve caller culling, geometry order, PNGs and animation groups. Missing sidecars use legacy adapter; malformed present ones fail closed. Static models share `pe_material_view.h` ([building materials](audit/BUILDING_MATERIALS.md)).

[IK drive](audit/PLAYER_IK_DRIVE.md): presentation only, from the same drawn clip clocks/weights. Disable arm mapping with drawn hand equipment; no solve at HP0 or inventory preview. All scenes solve together or retain the baked pose together. Missing optional groups retain baked presentation; malformed present PEIK1/driver groups fail startup. Drivers bind exact geometry/order; regenerate after scene reinstall. Blade/defence capsules, root motion and contacts stay baked. Halt and Grounder are the connected presentation offset sources; hit offsets remain disconnected. Blended pose, rigid mount/finger/toe following and rest initiation are adapters; nothing is saved.

[Reaction preview](audit/REACTION_PREVIEW.md) remains fixture-only. Separate group validation/readback does not authorize gameplay admission. [Portable paths](audit/PORTABLE_PATHS.md) requires logical source labels and exact hash-checked relocation without rewriting archived manifests.

## Camera

[Geometry integration](audit/CAMERA_ALIGNMENT.md), [query](audit/CAMERA_QUERY.md) and [environment](audit/CAMERA_ENVIRONMENT.md): camera-only queries retain source static masks/IDs and labelled native terrain/triangle policy. Never change movement sweeps for camera behavior. Commit scalar/recovery/rise atomically once per active display; draw, picking, labels and actions share that immutable pose. Failure preserves all owners; previews never commit rise. Reflected screen/ray math belongs to `pe_camera_projection`.

Evaluate 66 tree-excluding activity rays and Follow once per display, feeding back final clamp only. Aim is read-only. Preserve previous-clamp latch order, source hold interval and actual-dt SmoothDamp. Reset on successful load; do not save camera fields. Logical GeometryClamp near is distinct from the native .03m visible/pick near; restore graphics clip state afterward and retain player reach. Eye/capsule/rise/crop policies remain adapters.

## Tools, trees and camera modes

[Tools](audit/SANDBOX_ACTIONS.md): `pe_sandbox_actions` alone advances items12/19/26/1527 clocks; rendering reads snapshots. Pickaxe19 needs complete Mine body/held/stowed assets; never substitute Spade or also run generic mining on the same input. [Tree collision](audit/SANDBOX_WORLD.md) must use the same source capsule for headless constants, visual PESC, movement and save validation. Camera excludes tree layers; movement/tool/camera masks remain distinct.

[Camera modes](audit/SANDBOX_CAMERA.md): `pe_sandbox_camera` advances source scalar SmoothDamp once per display. F5 cycles free/captured third person/first person and clears Tab override; genuine UI/pointer ownership still wins. Free-mode RMB captures after 3px/.25s; short stationary release interacts/stows. This gesture is a native adapter. UI, focus loss and mode changes cancel it; visible gameplay pointer never disables keyboard movement. Full CameraForge graph remains incomplete.

## Melee, playback and capsule history

[Blade host](audit/SANDBOX_RUNNING_KNIFE.md) and [walking](audit/SANDBOX_WALKING_KNIFE.md): `pe_sandbox_melee` owns exact-instance female grounded TP Knife33. One retained outgoing interval drives clocks, root, blade and defence after shared collision. Never add WASD displacement, double-advance through hunt code, subtract reset stage clocks or apply rootY/delta rotation as movement. Running408/20110014 requires full actual speed4.5<speed<=7; ordinary Attack6–9 covers through4.5 with movement held. Do not reclassify/cancel an admitted action from its own root velocity. Unsupported branches reject before stamina debit. Absent optional ordinary roots retain the documented stationary adapter; malformed supplied assets fail closed. PESR1 source knots and PERM1 captured curves remain distinct.

[Lunge motor](audit/SWORD_LUNGE_MOTOR.md): move using prior latched request, derive request from previous root displacement with correction12 and HumanPhyCtrl10m/s hold, record current displacement, latch. Pose displacement reaches the body two ticks later. Drop ending displacement, retain one trailing tick; immediate cancel zeros horizontal velocity, never loaded saved velocity. Fixed pacing/drag/collision are adapters.

[Sprint](audit/SANDBOX_SPRINT_KNIFE.md): exact body/drawn/stowed/defence401 clocks and separate `pe_sprint_root_motion` curve enable Knife33/20110013 only; Attack7–9 continuations retain outgoing interval/yaw and once-only contacts/playback. No substituted ordinary/408/zero curve. Tail transient. Missing assets reject before debit; malformed/partial present assets fail startup. No Sword/Shield, reaction, water, airborne or first-person scope expansion.

[Playback](audit/ATTACK_PLAYBACK.md): source serialized .65s curve, not class .15s default. One display timestamp/pre-contact response snapshot spans catch-up ticks. Trigger notification precedes guide dedupe/damage; trigger identity is distinct from guide targets. Retain response through cancel/combo/rebinding, reset on successful load/new host, never save. Clamp base speed only, not final curve product.

[Facing](audit/MELEE_TARGET_FACING.md): movement request with zero-input body-forward fallback, latest continuation direction, frozen per-stage target. Default-OFF `camera_facing_attacks` overrides only free-TP request through `pe_melee_facing_request`; retain assistance/frozen stages. Shared Settings remains a Pause child, including closing frame. Preferences persist outside PeGame. Acquire facing transactionally; shared yaw never rotates camera. Stable cohort/broadphase/fixed scheduling and Tutorial stationary combos remain adapters.

[Capsule history](audit/MELEE_CAPSULE_HISTORY.md): preserve separate transient blade/defence histories and exact source sample/transform rules, not ad hoc expanded shapes. [Hare skills](audit/HARE_DAMAGE.md) use a guide clock distinct from Animator windows. HP/stamina/kill changes remain transactional. Modal/death/stow/equipment/camera/load cancellation is immediate; action state unsaved.

These request contracts do not enable physical reactions. Their linked algorithms and failure/order rules are binding:

| Owner | Contract and limits |
|---|---|
| `pe_repulsed` | [Repulsed](audit/REPULSED_ACTION.md): female curves/row6, prior-direction shield angle, strict old-elapsed tests, no-op repeat, retained cleanup. Unity zero-direction angle90. Caller owns projection/admission/normalization/physical response. |
| `pe_human_phy` | [HumanPhyCtrl](audit/HUMAN_PHY_CTRL.md): source request sum/clamp10, LateUpdate latch, cancel both halves, max two FixedUpdates per Update, grounded/water/rope velocity replacement and zero-request drag. Emits forces, never integrates/collides. Lunge reuses its request latch; full physical force consumption remains disconnected. Installed CIL-bound; PhysX/Mono width/cadence unqualified. |
| `pe_hit_ik` | [Hit IK](audit/HIT_IK.md): exact transform membership/duplicates, per-part restart, injected random force, nonnormalized direction, pinned displacement subtraction, weightScale twice and zero first frame. Skip preserves clocks. Offsets are requests, not poses. |
| `pe_animfollow` | [Lifecycle](audit/ANIMFOLLOW_LIFECYCLE.md): serialized overrides, active versus callback flag, RagdollLock versus IgnoreMe, controller-before-active/prior-error order, third-call capture, repeated settled callbacks and strict thresholds. Caller owns physics/Animator/query/cadence. |
| `pe_animfollow_drive` | [Drive](audit/ANIMFOLLOW_DRIVE.md): first11/12 profiles, exact indexed rig, cached reciprocal-dt PD order/history, shared joint damper, null skips, source force/limit modes and nonrigid cadence. No host/physics/save integration. |
| `pe_ragdoll_action` | [Ragdoll](audit/RAGDOLL_ACTIONS.md): ordered requests/class freezes, exact bone lookup, Wentfly repeat, player2s Knocked, false Update return, immediate GetUp dispatch/7–8 cleanup, cancellation without invented GetUp, partial faults and deferred Force after next FixedUpdate. No live immunity/physical reaction. |
| `pe_whacked` | [Presentation](audit/REACTION_PRESENTATION.md): no stun timer; state-exit HitAnimEnd (nominal .933s), row5/no-op repeat, ResetFullBody/missing-animator behavior. BeatWeight directional clips are not qualified composition. Separate25-clip group, source mirroring already baked, never append to33/42 or mirror again. Optional group/defence validates all-or-none; readiness selects nothing. |

[Halt](audit/HALT_STOP_EFFECT.md) is live through `pe_halt_host` in both hosts. It plays no clip: .2–.9s stop effect, zero weight through4.5m/s, additive foot offsets and Move/Jump mask. Preserve left/right90° tie, zero first frame, fades and master-weight skip. Release requests Halt only during Move/Sprint, grounded/alive/outside water with hands stowed. Jump, lost ground and masks end Move without Halt. By user choice (October15), renewed movement/sprint or jump input cancels Halt before offsets/admission in the same fixed step; movement never waits for the stop effect. Other action locks still apply. Death/reset/load/revival clears. Fixed-step scheduling and jump-over-release tie are adapters. Gather is silently refused while masked; hare-drop pickup remains independent. Offsets affect drawing only in model frame, never controller/capsules/contact. All transient.

[Full-body solver](audit/FINALIK_FBBIK.md), [Grounder](audit/FINALIK_GROUNDER.md) and [solver poses](audit/PLAYER_IK_POSE.md) retain their compiled operation order, clamps and source-specific transforms. Solver root is component IK, not serialized reference; no identity shortcut on zero offsets. Quaternion/interpolation helpers are adapters. Observed offset-only solver agreement (.29mm) and terrain Grounder agreement (.2mm/.23° with fitted inputs) do not qualify arbitrary poses.

`pe_grounder_host` updates both drawn hosts once per played frame before solve: set live limit/bend, pelvis/feet, then combined Grounder+Halt offsets. Never use prefab26.7° as a constant: movement curve varies5°..40°, drawn-hand spine mask zeros bend/arm mapping. Rays include host geometry and enabled trees, not actors; angle comes from capsule support. These queries/root/tool flags and Hermite evaluation are adapters. Never move physical controller/camera/contact or save state; reset load/respawn/death. Hit offsets remain disconnected.

## Hunt encounter, loot and revival

[Combat](audit/ADVENTURE_HUNT_COMBAT.md): active state2 reload uses saved count and a fresh full-HP pinned six-agent cohort. Do not claim original random respawn equivalence. Source head sensors/blocking mask; sole-player threat, max-three simultaneous attacks, fixed60Hz and noncritical hits are adapters. Modals pause combat; death offers checkpoint load. Four actual kills clean up cohort and enable return. Use exact visible body name/time with compatible23-joint defence, never idle or17-joint torso fallback; supplied mismatches fail closed.

[Loot](audit/HUNT_LOOT.md): `pe_hunt_loot_combat_step` atomically generates only from contact deaths, including simultaneous fourth-credit cleanup deaths. Never infer drops from quest deltas/disappearance. Saved world IDs differ from stackable bag instance0. Living-player pickup is independent of action/UI, strict4m; expiry uses saved seconds. Drops persist across return/menu/load and block world replacement.

[Revival](audit/SANDBOX_REVIVAL.md): item937 has a separate death transaction after display recovery. Restore .333×HPMax/full modeled stamina, cost25 Comfort, remount buffs preserving timestamps and cohort/progress. Get-up/1.5s defence grace transient; enemy clocks continue. Shared hunt admission prevents double-consuming pending intervals. Upright/no-ragdoll, atomic preflight and clamped saved Comfort are adapters, not full animation/Oxygen/town-travel acceptance.

## Equipment wear: outfit and sword

[Outfit](audit/STARTER_OUTFIT.md): fixed female95/131/192 records own identities independently of bag/tool, using the shared counter. Nonexhausted pieces add7/4/1 defense to base0. Each accepted incoming notification stages calculated damage×.1 wear on every piece, including overkill, before the next contact; commit HP/outfit/clocks together. Healing does not repair. General armor replacement/equip/removal remains unsupported; camp repair accepts eligible bag items only.

[Sword wear](audit/SWORD_WEAR.md): Wood Sword1 debits5 per admitted damage notification; recompute its10-point attack bonus before each target. Exhaustion removes bonus, not attack permission. No debit on miss/dedup. Exact bag instance is part of hunt transaction. [Sword pose qualification](audit/STARTER_SWORD_POSE_GROUP.md) and composed block readiness remain required before normal activation; staged assets/headless tests never enable it.

## Health, stamina and consumables

[Passive recovery](audit/PASSIVE_RECOVERY.md): one display update/global eight-frame attribute gate, outside fixed catch-up. Notify committed damage even at zero HP damage. Preserve fresh/load mount differences; load does not acquire a new4s hurt cooldown. Timestamps/phases transient.

[Stamina](audit/STAMINA_RECOVERY.md): same clock/lifecycle, never fixed-tick regeneration. Action owners debit and notify active cost tails. Preserve reverse buff order, previous summed delay for pulse, raw-comfort delay calculation before clamp and raw versus summed delay separation.

[Hunger](audit/HUNGER.md): Adventure only, after stamina in health frame on shared passive lifecycle. Preserve drain30200050, abnormal8 timer-before-accumulation rules (alive hunger0..50 start every1s;51..1000 end every.1s), eight foods' gains and4s group12 cooldown through shared item use. Starvation pre-adds reach every stamina cost/setup and recovery/comfort pulse together. Food cooldown resets on load.

Shared HUD draws `5_Hunger`/`bloodlan_12`, rotated180° from the right, only when Adventure snapshot marks available; Tutorial/Legacy retain empty track. Notice8000654 on every start is a native repeated-message adapter versus original lifetime-once PeTipMsg. End silent; random notices absent. Ordinary food use is observed; starvation/original comparison remains unqualified.

[Comfort HUD](audit/COMFORT_HUD.md): transient `PeSandboxUI.pe_comfort_hud`, never saved. Source-order slider, previous-frame drop>10 alpha restart and face thresholds. One update after attributes, never in drawing; skip building (source), pause/unfocus (adapter). Start/load reset with no load flash is adapter. Tutorial/Legacy remain unavailable/empty/face3. Curve is bounded native float arithmetic, not qualified engine output or attack-playback state; regenerate exporter.

[Bandage](audit/SANDBOX_BANDAGE.md)/[consumables](audit/SANDBOX_CONSUMABLES.md):924/925 share saved10s group9 medpack timer and generated200/350 HP; Juice916 group11/timed buff stays separate. Inventory/quickslots share PeGame transaction, allow full-HP consumption, reject dead. Action-only dispatch includes empty/cooldown retries. `consume_actions` suppresses new world actions while movement/camera/autorun/equipment/hunt continue; `consume_world` is modal. Pointer action ownership persists through release and escalates for real modals. Do not freeze gameplay for a heal/HUD hover.

## Welcome, supplier, population, town and quests

[Welcome](audit/ADVENTURE_WELCOME.md): archived Westina20008/RandomNpc17/role1; source1949→4081..4088 nearby, acknowledged pages resume, Arrival starts after4088. [Supplier](audit/ADVENTURE_NPC.md): archived Eitai20016/RandomNpc22/role2; immutable integer-truncated initial anchor, strict XZ<5 Arrival. Only final4094 commits supplies atomically/idempotently. Dialogue/modal encounter is transient; inventory/progression PeGame-owned. NPC layer10 self-collision is off, so no invented player blockers.

[Population](audit/ADVENTURE_POPULATION.md): preserve actual archived names/sex/appearance/roles, not Tutorial substitutes. New crops use archived XZ/heading plus supported settling; older provisional worlds stay. No general generation/empty-town invented markers. Role2/5 appear at welcome_page>=3 (after4082), roles3/4 after Arrival. Defence radii male.6/female.5 do not block players. [Labels](audit/BUILDING_MATERIALS.md#native-npc-visibility-adapter) use current camera-anchor visibility including geometry/trees; final2cm exclusion/fail-closed is native, independent of picking/admission.

[Transactions](audit/ADVENTURE_TOWN_INTERACTIONS.md): exact entity/store keyed stock/digital prices/refresh, retain original17 indices. Strict15m discovery4503 at(-284,25,134). [Wallets](audit/TOWN_WALLETS.md): eight archived purses,26-hour-day/noon latch, maxima not caps, once-per-display update; signed32 overflow rejects entire transaction. [Trade](audit/TOWN_SELL_BUYBACK.md): exact IDs/wear, equipped/shield sale rejected, source condition debit versus full-prototype vendor credit on buyback. Bounded32/vendor pool stays transient across close/menu/clone/import; clears on new game/successful load only. Failed load preserves it; LifeLimit unsupported.

[Medicine](audit/ADVENTURE_MEDICINE.md):9071 after Annot exposes4095/4096;4097 accepts9072+script877 atomically,4098 refuses. Readiness uses current917×5 possession. User-selected **final4100 acknowledgment** consumes917×5 and grants925×10+554 atomically, intentionally later than original4099. Capacity failure retryable. Keep formula component rules/type0script877; catalog341 items/108 formulas.

[Hunt progression](audit/ADVENTURE_POST_MEDICINE.md):9074→4101/4102,4103/4104;9075 requires validated4524(-349,19.5,233) and supported geometry. Never invent/clamp/reroll targets. [Return](audit/ADVENTURE_HUNT_RETURN.md): actual four kills, strict5m XZ to4525(-266,25,136), sourceY>=0; atomically grant592–595 once before4105/4106. Capacity retry is native adapter. Prompt waits for other modals, living player/present Naoko, independent of picking distance; cancellation/NPC interaction resumes, load can auto-prompt. Pending pages persist; acknowledgments never repeat rewards. State6 stops before9077. No skill-tree buff5/plot4 movement integration.

[Shop edits/details](audit/SHOP_QUANTITY_AND_ITEM_DETAILS.md): bind exact row state. Enter/outside accepts edit only; fresh trade action required. Never retarget stale rows through index clamp/wear/stock changes. Tooltips read-only; full-condition display copies are nonowning. Generated durability component, raw units and first-formula labels govern display.

[Numbered responses](audit/NUMBERED_CONVERSATIONS.md): user-requested top-row/keypad1–9, including acknowledgment label. Draw/select same visible mapping, preserve disabled choices, consume absent digits/closing frame before quickslots/building. Do not alter source text/reward timing.

## Disconnected foundations: mission events and Town Defense

[Events](audit/ADVENTURE_MISSION_EVENTS.md):9077–9084 reducer is unsaved/disconnected. Acceptance-grant latches separate from active/completed; final acknowledgment accepts.9079 queues Get plot5 before travel; qualified9080 targets complete individually. Effects preflight/commit atomic inventory/NPC-chain batches. Recipe definitions do not enable colony/quests.

[Defense](audit/ADVENTURE_TOWN_DEFENSE.md):30s preparation→wave0→10s cooldown→20s preparation→wave1. Ready preserves pending wait; max one wake/display. Finished means two requests, not15 deaths. Keep9077 disabled until real bindings/cohorts/species/geometry/reload qualify; never reuse six hunt hares or invent positions.

## Inventory, construction and camp repair

[Inventory](audit/SANDBOX_INVENTORY.md)/[discard](audit/INVENTORY_DISCARD.md): exact item/count/instance/wear selection. Protect Quest Item/equipped/shield/outfit/deployed ownership; split before partial discard. Confirmation including open/close frames owns shortcuts/save/load. Clear only destroyed exact-instance quickslots, retaining prototype-empty adapter. Split/exchange/sort/discard observations are bounded in audits; Shift+merge/full-category/protection cases are not all physically observed.

[History](audit/CONSTRUCTION_HISTORY.md): five transient successful cube/ramp batches, never world/inventory snapshots or mining/Tutorial/creation journal. Replay ceiling debit/floor refund, reject changed cells, retain on failure. Refresh shared collision before queries even without fixed tick. Commands retain chord/modal/pointer ownership. Retain across B/menu/clone/move; clear successful load/new game/world replacement. Never save. Ramp traversal and successful moving replay remain unqualified.

[Repair](audit/CAMP_REPAIR.md): single PeGame reserved item at machine20006. Exact bag insertion/shortcut cleanup commits only on success; reject primary/shield/outfit/deployed/buyback alias. Unequip first (native restriction). Replacement exchanges incoming same-category cell; otherwise preflight return capacity. Start atomically debits ceiling materials;60s per-display truncated100000Hz timer, not world-time acceleration. Completion restores same instance. Confirmed Cancel retains item/no refund; Take never restores shortcuts. Closing modal leaves job running. Reject stale state without partial mutation.

Once per focused/unpaused display: gameplay/repair modals/death do not suspend, pause/unfocus/menu do. Load restores remaining ticks without offline time; world replacement rejects reserved item. Ordinary Durability profiles only; Knife33/SafetyAxe1527 lack materials, LifeLimit/creations/worn armor unsupported. User reserved full-bag return, restart and death checks.

[Spatial repair](audit/CAMP_REPAIR_INTERACTION.md): exact archived box at(-613.5,29.5,404), both qualified crops, matching ready model/collision. Source closes >8m; native admission <=8m plus tree-free visibility. Distinguish source19m collider/obstacle+10m from unused6m field. E/short RMB competition and closing range/death/readiness frame retain all host input ownership; no leaked transfer/tool action.

## Deployables, vegetation and harvesting

[Torch](audit/SANDBOX_DEPLOYABLES.md): exact item10 transfer, source capsule pick/local quarter turns; Get destroys it for Charcoal1001×1. Per-display truncated100000Hz, strictly>200000 ticks; pause/unfocus suspend, cancel retains. Trigger capsules never block players/camera/tools. Placement checks shared geometry/inclusive10m; Adventure max64. Mounts bind exact equipped instance. Immediate draw/stow, Normal locomotion, particles/nearest-eight lights are native adapters. No Torch melee/water extinguish/bed/armor claim.

[Vegetation](audit/SANDBOX_VEGETATION.md): user-selected fixed patch (two trees, six herbs15/37/40, two decorative146). Install once on new sandbox; old saves only via `--vegetation-examples`. Deterministic supported placement/depletion survives repeated install/load; clear on world replacement/new game. Mesh bounds pick; plants do not block. Require complete saved asset group. Immediate herb collection/primary fixed fixture yields are adapters, not random source Gather timing; input stays consumed through release without freezing movement/actors. No Tutorial grants or tree11=prototype11 assumption.

[Harvest](audit/SANDBOX_HARVESTING.md): displayed tree11_grassland is prototype48, HP255, damage multiplier.48571 at unit scale. Source hardness/primary/fallback/optional production, inventory and saved RNG commit together. Rejected final axe contact consumes no wear/contact stamina. Runtime scale/placement fixed; seeds940/943/944 are not planting. No regrowth/procedural inference. `export_harvest.py --check --check-identity` verifies identity offline. [Gather qualification](audit/GATHER_COMMIT_CAPTURE_PLAN.md) does not yet replace the immediate native adapter.

## Settlement and route

[Building materials](audit/BUILDING_MATERIALS.md): optional PERM1 sidecars bind exact OBJ/MTL/PNG hashes and ordered material names. Regenerate after either OBJ group changes; preserve geometry/collision/manifests/PNG bytes. Only qualified two-sided materials disable culling; restore caller state. Missing legacy adapter versus malformed fail-closed remains explicit.

[Paths](audit/PORTABLE_PATHS.md): executable-relative `reference-assets`, explicit `--assets` authoritative; no compiled checkout/saved asset directory. [Settlement](audit/ADVENTURE_SETTLEMENT.md)/[route](audit/ADVENTURE_ROUTE.md): source1(-627,16,323),64×32×128; route(-640,0,64),416×64×416. New sessions prefer complete route; older saves/imports retain worlds. Configure assets before init/load. Immutable building collision is process-owned/borrowed; required missing geometry fails closed. All query/render/placement/save owners agree. General hosts use dimension/data accessors; legacy64 crop/wire exceptions intentional. Reflected X projection requires consistent rays/labels/controls.

Route capture/export requires every observed chunk and unchanged archives; never fill gaps or infer unloaded towns. Capture success is separate from physical travel acceptance. General settlement generation remains unported.

## Debug tools

[Debug](audit/DEBUG_TOOLS.md): explicit user-requested session-only cheats, default OFF, once-consumed commands in open/focused/paused Adventure context. Clone/move stages whole transaction. Never enable closed equipment, fabricate skipped rewards, reset completed hunt, destroy pending loot/repair or autosave. Travel preflights assets/geometry and resets transient owners; supplies/medicine preserve encounter. Modified saves are not ordinary earned progression evidence.
