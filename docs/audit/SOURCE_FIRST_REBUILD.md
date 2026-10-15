# Source-first evidence workflow

Recover active source/data before requesting another game session. The original client is an evaluation oracle for engine behavior and final comparison, not the primary way to discover explicit rules.

## Evidence lanes

| Question | First evidence | Required proof |
|---|---|---|
| Gameplay rules/transactions | Active C# callers, DB rows, component overrides | Native thresholds, order, rollback, cancellation and migration checks |
| Content/geometry | DB/XML/prefab/controller/archive records | Exact IDs/hashes, exporter regeneration and loader validation |
| Engine animation | Existing evaluated poses, Avatar/controller/curve data | Qualified clocks/matrices and smallest missing engine batch |
| Generated state | Existing saves/generated records, then source generator | Separate algorithm from a particular world's random result |
| Feel/integration | Finished native feature and original comparison | Recorded mode/settings/inputs/outcome; no wider parity claim |

Serialized/loaded overrides outrank constructor defaults. Trace consumers, not similarly named legacy paths. A suspected shipped/source difference needs narrow assembly/runtime evidence; it does not invalidate all source evidence. Static table values alone do not prove active behavior.

## Capture gate

Before each original launch, state:

1. The unresolved question.
2. Why source, archives and existing captures cannot answer it.
3. The smallest useful capture, expected fields and acceptance condition.

Batch compatible questions, calculate bounds offline and reuse qualified poses. Preserve baseline manual saves, exact source/assembly/probe hashes, renderer/settings and actual teardown result. Missing generated instances may need observation; static prices/quests do not. This workflow imposes no additional approval round.

Rules, callbacks, scalar arithmetic, assets and final visual acceptance are distinct evidence. Native tests do not establish original scheduling/difficulty; video does not establish hidden formulas/IDs. Do not tune damage/enemy counts to compensate for an unqualified missing mechanic.

## Current priorities

[HANDOFF](../HANDOFF.md) is the single next-work owner. [Feature contracts](../FEATURE_CONTRACTS.md) defines supported behavior; [PLAYTEST](../PLAYTEST.md) indexes acceptance.

Earlier source-first reviews identified recovery, stamina, capsule history, playback, wallets, inventory categories, construction history, repair, camera environment and harvesting gaps. These now have feature owners/audits; the old proposed order is retired. Ordinary native hunt completion and partial reload have bounded acceptance. Do not resurrect historical missing-feature claims.

Shield composition, physical reactions, general NPC combat and Town Defense remain separate closed gates. Source-portable work may proceed without pretending staged animation or headless planners enable those systems.

## Unimplemented systems: source entry points

These are investigation boundaries, not authorization to enable gameplay.

| Area | Recover before integration |
|---|---|
| Oxygen/water survival | `initprop1` Oxygen500/500 and buff30200051; active water-state callers and lifecycle. Hunger/Comfort are already implemented separately. |
| General NPC simulation | `PeEntityCreator.cs`/RandomNpcDb and RandNPC closure: sampled attributes, equipment/inventory, wallet, abilities, behavior, voice and revive policy. `VArtifactTownManager` supplies transformed candidates. Archived appearance is not a current combat loadout. One protected NPC damage/death/ally attack path requires authoritative state, camp permissions, exact equipment and reset/load behavior. |
| General equipment replacement | `EquipmentCmpt` checks ragdoll, sex, skill tree, overlapping masks, removal permission and return capacity before ownership changes. Test exact-instance rollback; do not expose replacement without compatible meshes/materials. Strengthening changes property/max-wear rules, not just validation limits. |
| Crafting fidelity | `ReplicatorCmpt`/synthesis_skill output, six materials, workspace, time and serialized job. Source handles at most one completion/display; native catch-up and retry semantics are explicit adapters. Expand actual dependency closure, not all formulas indiscriminately. |
| Quest continuation | Active AdRandomMission/PlayerMission graph, choices/roles/plots/rewards. Discovery uses entity position at target creation and integer XZ, not necessarily spawn anchor after plot motion. [Mission events](ADVENTURE_MISSION_EVENTS.md) owns9077–9084; no live enablement. |
| Defense actors | [Spawn contract](TOWN_DEFENSE_SPAWN.md): EntityMonsterBeacon/MonsterBesiege520, integer exclusive maxima, uniform radius20–80 (not annular-area uniformity), deferred terrain Y and actual beacon membership. NPC participation, species4/7, full-annulus geometry, dedup/failure/cleanup and reload require qualification. Never clamp to the crop or reuse hunt hares. |
| Procedural towns/world | VArtifactTownManager reads `RandomTown/VArtifactTown`, System.Random(TownGenSeed) plus ally seed; local building shuffle seed center.x+center.y. RandomMapConfig adds biome/size/rivers/plains/flatness/bridges; VFDataRTGen cache includes settings/actual seed. Recover XML, Town_artifacts/Townhouse, names/camps/art metadata; qualify Mono RNG/draw order, planner rotations/cells and terrain seams/negative coordinates/flattening before host integration. A title seed alone is insufficient. |
| Entity registry | PeCreature distinguishes assigned, recorded-auto and launch-reset nonrecorded IDs; monsters use nonrecorded IDs. Mission progress persists separately. Tower target kind6 saves protected IDs/finallyPos, not wave timer/cohort. Follow reload ordering before inferring reset/resume; array indices are not durable IDs. |

Read-only database baseline: `Assets/Resources/DataBase/localData.bytes`, SHA256 `c3d63ee865a7e10e309e11e9ee561c93feef39c379a584dcf68722a523cd4376`. Use table/primary keys, not fictitious binary line numbers. Export current exact closures; row counts are not supported-feature counts.

## Hunt selection investigation

BTCalculateAttackMode.Tick uses registered order, cooldown/geometry readiness (including IAttackTop exception) and weighted cumulative Random.value<=bound selection. Positive fallback occurs only after no selection and strictly>2s; its timestamp changes only on selection. Failed selection retains the prior attack. TargetCmpt.SwitchAttack's simpler first-ready startup is commented out and is not the active policy. Native uniform five-melee choice/max-three/sole-player threat remains an adapter; adding a per-tick pounce coin toss is not a port. Recover registration order/candidate predicates and test injected-draw boundaries before changing production selection. [Pounce preparation](HARE_POUNCE_PREPARATION.md) remains disconnected.

## Jump/fall investigation

Source `Action_HumanMove.cs` Action_Jump checks/debits scaled stamina, enforces motor minimum speed and requests upward `sqrt(20*m_JumpHeight)`. It refreshes consumption time, retains takeoff local movement and combines new input through m_JumpMoveF. Landing/water/end/gravity use strict elapsed boundaries; support is a capsule cast with serialized mask. Action_Fall separately interpolates desired air movement by5*Time.deltaTime.

The current native fixed vertical7/gravity20 and input-driven horizontal velocity are adapters, not source inertia/support grace. Export actual serialized bindings before a pure jump/fall owner; test reversal, retained momentum, strict grace, ceiling/fall handoff, debit and cancellation with one motion owner. Compare requested velocities separately from collision results; do not add guessed fall damage. This remains future work, not a change in this documentation pass.

## Completion criteria

Keep a small manifest of active source paths, IDs, hashes and unsupported branches. Regenerate metadata, test the responsible native owner and preserve transactional load/item identity. Runtime identity and item instance ownership must remain coherent across shops, deployment, worn gear, repair and encounters.

Use an offline managed/native arithmetic oracle where practical, without initializing the game. Label float/quaternion/clock substitutes. Graphics/integration checks follow working code; they never replace the algorithm/transaction proof. [Extraction workflow](SOURCE_FIRST_EXTRACTION.md) covers animation and recording boundaries.
