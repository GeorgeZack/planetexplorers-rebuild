# Source-first review at the pushed rebuild checkpoint

Current follow-up: [audit after pushed3791d89e](SOURCE_FIRST_CHECKPOINT_REVIEW.md), including newly decoded vendor wallets, generic pounce extraction and inventory/construction gaps. The priorities below describe this earlier review and its subsequent implementation notes.

October 6, 2026. Baseline: `5ec481e34060d78a461f3ea36d6a9689de18cdc9`, verified as both local `main` and private `rebuild/main` before this review. The working tree was clean. This is a fresh C#/database/tool/C comparison, with three parallel specialist reviews; neither game was launched. It updates the next-work decisions in [the earlier source-first audit](SOURCE_FIRST_REBUILD.md), whose baseline findings remain historical evidence.

## Decision

Most of the next gameplay work can proceed without another original-game playthrough. Develop transactions, action admission, mission dependencies and planners from their active source consumers and data. Reuse existing animation and world captures. Keep full humanoid evaluation as a separate research task: its remaining limb solver should not block unrelated gameplay work.

The earlier audit's passive recovery, stamina/comfort, historical capsule contact and contact-speed corrections are now implemented. Shield action rules have a headless owner; visible blocking remains gated. Reopening the original to rediscover those contracts would add little. Their native integration acceptance is still distinct from source verification.

Detailed current reviews:

- [Combat, movement, equipment and revival](SOURCE_COMBAT_REVIEW.md).
- [Progression, town services and encounter dependencies](SOURCE_PROGRESSION_REVIEW.md).
- [Extraction coverage, reusable captures and a recording plan](SOURCE_CAPTURE_REVIEW.md).

## Next implementation order

Subsequent implementation: [the bounded Revival Shot lifecycle](SANDBOX_REVIVAL.md) now connects the dedicated transaction, death modal, recovery remount and transient contact grace. Ragdoll/get-up presentation and physical acceptance remain open. The table below retains the audit's proposed order; [walking-attack admission and ordinary root motion](SANDBOX_WALKING_KNIFE.md) are also implemented with focused checks; physical acceptance remains pending.

These are proposed increments, not features delivered by this documentation change. The order favors useful supported gameplay over expanding the list of visible but unusable content.

Further implementation: [sword branch/movement policy](SWORD_MOTION_POLICY.md) now shares the source selector between display input and core admission and factors the horizontal request. The4,101-case offline magnitude oracle fixes one strict-boundary discrepancy. The follow-up source proof and PESR1 export now supply ordinary moving admission and combo displacement; no new original session was needed.

| Order | Deliverable | What can be done offline | Remaining acceptance or dependency |
|---|---|---|---|
| 1 | Dedicated Revival Shot937 transaction and death recovery state | Export the actual skill chain; implement inventory debit, vital changes, cancellation and repeat/reload guards with one owner | Source get-up readiness, ragdoll transition and death/revive action relations need a supported presentation contract. Town-return respawn is separate and requires a valid destination. |
| 2 | Correct grounded ordinary-attack admission while walking slowly | Trace source branch thresholds, movement takeover, cancellation and combo behavior; reuse existing Attack6–9 poses and inspect their retained root data | Qualify each used root/motor branch before enabling it. Ordinary movement must not simply continue through an attack because a gate was removed. |
| 3 | Quest graph and Town Defense planner, before exposing9077 | Export prerequisite/role/target/reward dependencies; implement deterministic injected-random spawn decisions, identity and save policy | Playable defense also requires allied NPC combat, species4/7, supported spawn geometry and qualified presentation. Keep acceptance gated until these are connected. |
| 4 | Selling/buyback, then repair quotes and timed repair | Read realized archive wallets first; export prices/condition rounding; implement exact-instance transfers, capacity and alias checks | Repair additionally needs a machine/job owner. Reuse source UI assets; perform a short native transaction test after integration. |
| 5 | Hit-reaction and broader movement owners | Export force/threshold/action rules and test selection, displacement and interruption | Visible reactions and physics need their own qualification; do not claim ragdoll fidelity from a damage calculation. |

Source-rooted planning and exports for orders3–4 can proceed alongside1–2. General armor replacement, recipe expansion, camera graph and procedural world generation remain worthwhile but larger workstreams. No new run is needed merely to enumerate their source inputs. General hunger/oxygen should arrive with their dependent item, environment, consequence and save behavior, rather than as isolated depletion meters.

Two concrete rechecks explain the first priorities:

**Revival already has an item but no native lifecycle.** The C starter package includes937×10; the current death message instead directs the player to load a save. Read-only database inspection follows `PrototypeItem937 → skMain20150031 → skTriggerEvent53,59`: raw Comfort−25 and HP=`HPMax*0.333f`. Despite the skill's descriptive name, this is not full healing. `_cond=False` is the skill's loop condition, not an admission failure. `Action_Revive` separately floors HP to1, restores stamina/oxygen and owns get-up/freeze state. `UIRevive` starts that action before `UseItemCmpt.Revive` uses the item. The port must reconcile those owners and failure cases, not add937 to the ordinary live-player consumable switch. See the combat review for source links and tests. This recheck confirms an earlier finding; it does not constitute new observed gameplay.

**Walking attacks have an explicit native restriction.** `pe_sandbox_melee.c:stationary_allowed` requires no movement request and horizontal speed≤0.05. Source `Action_Sword.ApplySkill` chooses the ordinary skill when grounded velocity does not exceed its4.5 movement threshold; it does not require standstill. `UpdateSkillState` then takes over horizontal motion from animation, capping the velocity difference at12 per update. Removing the native admission test alone would leave a different motor contract. This is a code comparison, not proof of the effect on hunt difficulty.

The read-only database used here is `Assets/Resources/DataBase/localData.bytes`, SHA256 `c3d63ee865a7e10e309e11e9ee561c93feef39c379a584dcf68722a523cd4376`. These are repository-source rules; matching installed animation metadata does not prove every C# branch is identical to the shipped client. Resolve a suspected source/client difference with a narrow observation or assembly inspection, rather than treating all source evidence as uncertain.

The progression review also traces two details that a general playthrough could easily miss: accepting9079 queues plot5 and starts9080 before9079 completes; worn-item buyback charges the player a condition-adjusted price while the shop UI credits the NPC at the prototype price. The latter is a source asymmetry, not an instruction to invent conserved currency. Both need explicit regression cases or a documented intentional divergence. Pigment quest9082 already has its main crafting dependencies in the exported catalog, but its prerequisites and six missing reward scripts still need implementation/export closure.

## Extraction boundary

| Evidence available now | Use it for | What it cannot establish alone |
|---|---|---|
| C#, database, XML, prefabs and controller serialization | Rules, component dependencies, masks, prices, event order, curves, mission conditions and generator algorithms | Final engine scheduling, evaluated humanoid poses or exact generated state from an unknown random history |
| Preserved saves and route/population captures | Realized identities, mission locations, appearances, inventory/wallet state where serialized, occupied geometry | Unloaded areas, every random variant or a general generator |
| Existing closed animation bakes | Compare decoded curves and numerical ports; reuse matching body, weapon and defence clocks | Missing limb/IK/layer behavior or arbitrary phase combinations that were never qualified |
| Short original/native runs | Resolve a named engine question or check integrated controls, collision and UI | Hidden values, complete branch coverage or source attribution from a screenshot |

The offline humanoid proof already reused697 poses and compared27,880 local bases. Its joint arithmetic matches272,217 bounded native arithmetic samples bit for bit, while12 limb bones still disagree materially. That is evidence that offline recovery works, and evidence that the full solver remains unfinished. Keep both results visible. Before another substantial retargeter expansion, compare its value across future clips with the prepared one-time engine-bake path. Neither a full solver nor another batch should be assumed automatically cheaper.

Existing native crafting deliberately retains retryable atomic behavior and uses a different completion clock from the source. Catalog expansion must label that difference; an audit is not permission to silently replace the safer transaction policy. Likewise preserve the user-selected final4100 medicine acknowledgment.

## Half-hour user observation

**Viable, and useful after preparation.** Use ordinary game-window video, timestamped notes and the user as the sole input operator. Periodic assistant frame inspection plus review afterward can cover a broad sequence; it is not guaranteed continuous frame-by-frame observation. The [capture review](SOURCE_CAPTURE_REVIEW.md) lists the repository's actual recording-tool capabilities and the remaining preparation. An ordinary video recorder still needs configuration and a short playback/performance check before scheduling the session.

Suggested session:3 minutes showing equipment/stats/mission state;7 minutes movement, camera and weapon transitions;10 minutes combat, retreat, healing and a partial-progress save/reload;7 minutes reachable mission/dialogue or vendor transactions;3 minutes showing final state and saving to a new slot. Adjust for death or slow travel rather than repeatedly restarting. Preserve the starting checkpoint and use the verified hardware-rendered isolated launch. Record only the agreed game surface; no microphone is needed for this checklist.

Do not run the current full-pose probe for the whole session. It is bounded to120 seconds/3600 frames/256MiB and has no automatic rolling rearm. The earlier74-second recording was already226.2MiB. Ordinary video can show broad behavior; selected pose bursts should be planned only for a precise unresolved animation question. A30-minute video/performance run has not yet been validated.

A single prepared session should replace repeated general tours. It cannot supply unvisited content, hidden state or all animation phases. It also cannot substitute for checking the C build: four ordinary native hare kills and a partial-progress physical reload remain outstanding even if the original game is recorded completing them.

## Checkpoint discipline

Before an original launch, write down the unanswered question, the source/archive/capture already checked, and the smallest batch that closes the gap. Afterward store the answer once with its provenance. No broad replay is scheduled by this audit.

This change adds documentation and refreshes navigation only. Verify local links, cited symbols and whitespace, rebuild the structural index, then commit/push the review. There is no new runtime implementation, save-format change, graphics result or combat acceptance claim.
