# Source-first rebuild audit

October 6, 2026. Baseline: private rebuild commit `25877e7f759538b7cd6a0358a0c71d2debbe20d4`. This audit inspects original C# code, serialized data, existing captures and the C implementation. It launches neither game. It is a prioritized parity audit, not an exhaustive verification of every Unity subsystem.

Subsequent implementation: [passive recovery](PASSIVE_RECOVERY.md) now supplies the first work-order item in Adventure Sandbox, including its source eight-frame attribute gate and fresh/load lifecycle. Optimized core and focused sanitizer/host checks pass; physical recovery remains unverified. The subsequent [stamina/comfort increment](STAMINA_RECOVERY.md) implements the second item, with source pulse ordering and v16 comfort persistence. The baseline findings below remain the audit record; the next increment integrates [historical capsule contacts](MELEE_CAPSULE_HISTORY.md) for sandbox blades and hare forearms using offline-qualified collider centers. Its verification and remaining scheduling limits are recorded separately.

## Decision

Continue gameplay development from source and offline data. Reserve original-game sessions for a named unresolved engine behavior, missing generated-world state, or acceptance of an integrated feature. Most control rules, item values, transactions, progression and numerical behavior do not require another playthrough to discover.

The current shield investigation establishes a working way to evaluate the original masked controller. It should remain a separate animation workstream. Its remaining loop qualification does not prevent implementing and testing source action relations, equipment ownership, passive recovery, damage bookkeeping and persistence.

Detailed findings:

- [Combat, equipment and player state](SOURCE_FIRST_COMBAT.md).
- [Progression, economy and world state](SOURCE_FIRST_PROGRESSION.md).
- [Assets, animation and extraction tools](SOURCE_FIRST_EXTRACTION.md).

These reports distinguish implemented C behavior from source-derived contracts and unresolved runtime evidence. Table values must be traced through actual loaders and callers; a default value alone does not establish the final behavior.

## Evidence lanes

| Work | Primary evidence | Verification before gameplay testing |
|---|---|---|
| Rules and transactions | C# call chain plus exact database rows | Deterministic C tests for thresholds, order, cancellation, rollback and save migration |
| Content and geometry | Database, XML, prefab/controller serialization and archive records | Export with original IDs/hashes; mutation, regeneration and loader checks |
| Engine-evaluated animation | Serialized curves, Avatar/controller data and existing evaluated poses | Reuse qualified samples; batch only genuinely missing engine evaluations; compare actual clocks and matrices |
| Generated instance state | Archived saves, generated records or a named missing runtime snapshot | Decode existing checkpoints first; distinguish generator logic from the particular saved world's output |
| Feel and end-to-end behavior | Short original/native visual or ordinary-input comparison | Run after integration; record scenario, inputs and outcome separately from code tests |

Passing a numerical test does not prove visual fidelity. A video does not prove an internal formula, hidden ID or exact collision clock. Neither should be used in place of the other when the primary evidence is already available.

## High-value finding: recovery rules

Read-only inspection of `Assets/Resources/DataBase/localData.bytes` confirms that player `initProp.id=1` includes buffs30200046,30200052 and30200175. Buff30200046 conditionally adds1% maximum HP when the player has gone at least4 seconds without damage and HP/maxHP is below.33. Its interval is1 second. The C game currently lacks this passive recovery owner. This is a source-backed survival gap, not a proposed difficulty adjustment.

The buff scheduler matters: source performs an initial evaluation and subsequently checks strict elapsed-time boundaries, with at most one periodic evaluation per global eight-rendered-frame attribute pass. The first execution can also perform its initial evaluation in that same pass; see the [completed lifecycle audit](PASSIVE_RECOVERY.md). Stamina recovery is also buff-timed; its comfort-dependent delay is not equivalent to the current continuous C recovery. Implementation should recover the scheduler, hurt/consumption timestamps, death rules and save behavior together. It should not simply add an unconditional HP-per-second increment. See the combat report for the call chains and test requirements.

The same review confirms that Knife33's zero wear and the inspected player/hare zero base critical chances are source values. They are not defects to change in order to obtain four kills.

## Implementation work order

This order favors corrections to existing gameplay before expanding the world. The detailed reports contain the source paths, row IDs and proposed tests. These are proposed increments, not features delivered by this documentation pass.

| Order | Deliverable | Work possible without running the original | Remaining dependency or acceptance check |
|---|---|---|---|
| 1 | Passive low-health recovery and a shared buff clock | Trace initialization/load/death, export the exact HP rule, record supported damage timestamps, test threshold/tick/save behavior | Resolve the clock/load contract from source first; then a short native retreat/recovery check |
| 2 | Stamina cadence and comfort-dependent delay | Port buff scheduling and consumption timestamps; model the required comfort state coherently; differential tests across frame partitions | Native movement/combat integration; avoid adding isolated hunger depletion before its supporting systems |
| 3 | Historical melee capsule sampling | Port previous/current endpoint sampling, reset rules and successful-hit deduplication; test fast motion, teleports, first activation and repeated contact | Short native combat comparison using existing poses; this changes both incoming and outgoing contact opportunities |
| 4 | Sword/shield action integration and contact playback | Test source action admission/cancellation, exact equipment ownership, serialized contact-speed curves and conditional mitigation | Visible blocking still needs qualified masked poses. Keep that engine-evaluation task separate from the source logic |
| 5 | Next quest graph and defense spawn planner | Export prerequisites, choices, targets and rewards; implement an injected-RNG planner and explicit runtime entity identities | Full defense needs NPC participation, species4/7 behavior/assets and supported geometry over the whole spawn annulus |
| 6 | Selling, buyback and repair quotes | Export condition-based prices, wallet rules and material rounding; implement atomic exact-instance transfers and quote tests | Inspect archived realized wallets first; timed repair additionally needs its machine owner and interaction flow |
| 7 | Creature animation and camera extraction | Resolve streamed curves/bindings; decode camera graph and numerical nodes; inventory missing clips and reuse existing captures | Qualified engine batches only for unresolved evaluation; final visual/camera acceptance remains separate |
| 8 | Crafting/catalog expansion and general equipment replacement | Export dependency closures, workspace/component rules and slot constraints; reconcile scheduling/capacity adapters explicitly | Matching appearance parts and supported product behavior before exposing additional items |
| 9 | General procedural world generation | Port seed derivation, Mono RNG oracle, town planning and terrain subproblems from source/XML/data | Substantial separate project; exact archived-world reproduction also requires complete settings and asset inputs |

**Baseline recommendation:** items1–3, beginning with the source initialization/load clock audit. Items1 and2 are now implemented; the capsule-history increment addresses item3. After its checks, item4 remains the next source-first workstream. Unity's scaled gameplay time must not be confused with the native accelerated world/day clock. Recovery should have one owner, and every supported committed hit must update it. Preserve the user-selected final4100 medicine acknowledgment and existing deliberate transactional retry behavior while changing unrelated systems.

The missing recovery, differing stamina cadence and instantaneous collision sampling are credible contributors to combat differences. This audit does not establish their individual effect on difficulty or promise that fixing them makes the four-hare fight easy. Do not tune damage or enemy counts before correcting the identified contracts.

## Extraction deliverables and completion criteria

For each increment, retain a small source manifest with exact row/asset IDs, input hashes, active caller paths and explicit unsupported branches. Produce generated metadata through an exporter, then test the C owner against boundary cases or an offline managed-code oracle where practical. Keep reference inputs immutable.

Use the existing archives to recover realized identities, equipment, mission targets and world settings before requesting a new capture. A missing historical random result is different from a missing algorithm: defense spawning and town planning can be implemented with injected draws while exact-session reproduction remains provisional. Native persistent IDs and item ownership must remain coherent across vendors, repair, equipment and encounter state.

For animations, prepare one inventory of missing leaves, layers, mounts and defence bones for all upcoming consumers. Reuse qualified samples and batch the remainder within the verified limits. The prepared shield repeatability/grid workflow is still unrun; neither this audit nor the three-sample smoke authorizes a production block-loop claim.

This documentation pass used three parallel source audits and root cross-checks. Local link/line checks and structural-index regeneration validate navigation, not game behavior. No new graphics, combat completion or engine evaluation is claimed.

## Thirty minutes of user play

**Viable and useful for broad behavioral coverage, with limits.** The earlier user-operated Zink session was responsive and produced useful movement/block/attack evidence. That does not guarantee a crash-free half hour, and software rendering was reported unplayably slow. Use the documented process-local hardware-rendered reference setup and an isolated checkpoint.

The useful default is game-window video plus timestamped notes, with the user as the sole input operator. The assistant can inspect periodic frames during play and review the recorded sequence afterward. This does not imply continuous human-like visual attention or frame-perfect analysis while the user plays. No microphone or desktop audio is needed unless desired.

The existing detailed pose observer cannot record a continuous half hour: [launcher bounds](../../rebuild/tools/reference_pose.py) cap it at3600 frames or120 seconds, and [the probe](../../rebuild/tools/ReferencePoseProbe.cs) also caps output at256MiB. Its1800-second setting is the pre-arm waiting limit, not the recording duration. The successful3600-frame shield recording lasted74.367614 seconds and used226.2MiB. Extrapolating the same uncompressed data density gives about5.35GiB for30 minutes, before video; that is an estimate, not a validated long-session mode. The current workflow has no automatic rolling/rearm recorder.

For a future session:

| Approximate segment | User activity | Evidence gained |
|---|---|---|
| First3 minutes | Show mode, Character stats, equipment, inventory and mission state | Baseline for interpreting later damage, wear and progression |
| Next7 minutes | Walk/run/strafe/backpedal, camera changes, draw/stow, block and attack transitions | Presentation, control feel and missing transitions |
| Next10 minutes | Ordinary combat, retreat, healing, blocking; save and reload after partial progress when possible | Practical difficulty and recovery; actual partial-progress persistence |
| Next7 minutes | Continue the hunt and report it, or follow the next reachable mission | Mission order, rewards, dialogue and encounter behavior |
| Final3 minutes | Show equipment wear, counts and mission state, then save to a new slot | End-state comparison and a reusable checkpoint |

This is a checklist, not a requirement to finish the hunt within30 minutes. A death, interruption or unreachable later mission is still useful evidence; preserve the starting checkpoint. Exact load-slot selection should be confirmed from current save metadata before the session rather than inferred from old slot numbers.

One longer recording can replace repeated broad tours of menus, movement and combat. It cannot expose every animation phase combination, isolate IK effects, recover an unvisited procedural area or prove all conditional branches. If detailed telemetry is needed, prepare a lightweight event/state recorder or bounded selected bursts before asking the user to play; do not repeatedly restart a session merely to switch probes. That extension is proposed, not implemented by this audit.

**Recommendation:** proceed with the source-only increments now. A half-hour user session is worthwhile once there is a prepared comparison checklist and game-window recorder; it is not a prerequisite for those increments. Existing local video tools make ordinary recording feasible, but no continuous30-minute recording/performance run has been validated here. Recheck available storage, current checkpoint metadata and renderer configuration when preparing that session.

## Working rule for the next increments

Each proposed original-game launch should state the unanswered question, why source/offline data and existing recordings cannot answer it, and the smallest capture that will close it. Batch compatible questions in one prepared session. Record confirmed findings once and reuse them.

Keep the original goal open: numbered conversations are implemented, but ordinary native four-hare completion and a partial-kill save/reload are not yet physically verified. The original game's corresponding recording can inform comparison; it cannot substitute for those native checks.


## Follow-up checkpoint: extraction avoided another game session

Items1–3 are implemented with source/headless evidence. Item4 now adds [contact-responsive attack playback](ATTACK_PLAYBACK.md) to the bounded sandbox host and a separate [shield action owner](SHIELD_ACTION.md). The serialized response lasts0.65s, not the0.15s class default. Disassembling the installed evaluator and replaying only its bounded arithmetic established the exact curve without initializing the game:4,109 C results match bit for bit. Shield admission, waits, hold effects and cleanup are source-portable; ordinary rendered shield activation is still disabled.

The next animation step can also begin offline. The exact controller779, layer3 mask, transitions, independent clocks, Avatar skeleton/axes and raw block clips413/420 are present. Each block clip has0 streamed curves,114 dense and116 constant scalar channels:31 direct Transform bindings plus127 humanoid scalar bindings. Those humanoid channels are not directly usable bone transforms. `export_reference_player.py` already extracts the channels and labels unretargeted humanoid data; another inventory tool or game tour would not resolve the missing computation.

The useful next proof is a bounded CPU humanoid retargeter, first compared against existing unlayered player bakes, then against the three preserved composed controller evaluations. Only after those comparisons should it attempt independently advancing base/block phases and matching defence bones. Full-body clip substitution, an invented bone mask or a fit to one phase pair would not establish parity. Built-in foot IK and external FullBodyBipedIK/Grounder remain separate postprocessing questions; regional pose discrepancies alone do not identify their contributions. See [extraction audit](SOURCE_FIRST_EXTRACTION.md) and [shield composition evidence](SHIELD_LAYER_CAPTURE.md).

This gives a practical boundary: gameplay rules, transactions, action relations, scalar curves, assets and much animation decoding can proceed without play. Engine-specific humanoid evaluation may require either a bounded evaluator validated against existing samples or one prepared engine sampling batch. Ordinary control feel, integrated combat and partial-progress reload still need native acceptance. The30-minute user-play plan above remains viable for broad behavioral comparison; use game-window video with timestamped notes, not the existing high-volume pose probe for the entire session. No new game launch was used in this follow-up.
