# Original hunt encounter observation and reload contract

October5,2026. This audit now includes **two bounded live original reload captures** and a source-pinned six-agent export, in addition to the earlier offline tooling checks. The earlier [original save5 evidence](../reference/hunt-original-encounter.json) establishes completed9075, active9126/target1518 and `MONSTER0=13_0`; it does not contain the six live agents' positions, scales or HP. The native host still stops at encounter-pending9126. See the [post-medicine audit](ADVENTURE_POST_MEDICINE.md) for the full implementation plan and [PLAYTEST](../PLAYTEST.md) for separate physical arrival evidence.

## Source reload path

[`MissionManager.InitPlayerMission`](../../Assets/Scripts/Mission/MissionScript/MissionManager.cs) restores each archived `m_RecordMisInfo` entry. For its `STEP` flag it calls `SetQuestVariable(...,true,true)`; other flags, including `MONSTER0`, go through `ModifyQuestVariable`. [`PlayerMission.SetQuestVariable1`](../../Assets/Scripts/Mission/PlayerMission.cs) rebuilds the active mission and invokes `MissionOperationAdRand` for each target. The kill-monster branch calls [`MissionMonsterKill.ProcessMission`](../../Assets/Scripts/Mission/MissionTowerDefense.cs), which calls [`SceneEntityCreator.AddMissionPoint`](../../Assets/Scripts/Scene/SceneEntityCreator.cs). That creates a new `SceneEntityMissionPointMonsterKill` and calls `Start()`.

[`Start`](../../Assets/Scripts/Scene/SceneEntityMissionPointMonsterKill.cs) does not subtract the saved kill count from its spawn count. It samples a new player-relative15m center, then six independent prototype13 agent positions within radius5. The position sampler uses uniform radial distance, not uniform area. `center` exists only as a local variable; `_agents` stores the actual created agents. The source therefore supports fresh agent generation on mission restoration, while keeping the quest counter. This is distinct from restoring the six prior actors' IDs, health or exact positions. Observe an actual mid-encounter reload before asserting all visible consequences, especially with nonzero kill progress.

The scene archive cannot supply those initial live actor records through the paths inspected so far:

- [`SceneEntityPosAgent`](../../Assets/Scripts/Scene/SceneEntityPosAgent.cs) implements `ISceneObjAgent`, not `ISceneSerializableObjAgent`. [`SceneMan.Export`](../../Assets/Scripts/Scene/SceneMan.cs) writes only agents implementing the latter interface from its active/inactive queues.
- [`MonsterEntityCreator.CreateMonster`](../../Assets/Scripts/Scene/MonsterEntityCreator.cs) allocates these entities with `FetchNonRecordAutoId`, explicitly marked “not save” in source.
- [`SceneEntityCreatorArchiver`](../../Assets/Scripts/Scene/SceneEntityCreatorArchiver.cs) writes fixed-spawn activation and population-rectangle NPC counts. This is not a serializer for the mission point's private `_agents` list.

Initial Y also needs exact interpretation. `SceneEntityPosAgent.CheckSetAvailablePos` resolves the deferred source Y from dependency ray hits and stores chosen surfaceY+1 in `agent.Pos`. Multiple-hit selection can consume source RNG. `MonsterEntityCreator` then skips the additional prototype spawn-height adjustment when a beacon is present; mission9126 uses a beacon. Record `agent.Pos` separately from the moving entity's actual position. Do not flatten the latter onto a guessed surface, add the ordinary hare+.5m offset, or infer the15m center by averaging the six scattered positions.

## Bounded observer

[`reference_hunt_encounter.py`](../../rebuild/tools/reference_hunt_encounter.py) reuses the established [pose probe's launch-only shim](../../rebuild/tools/REFERENCE_POSE.md). Default invocation compiles offline against the legally installed Unity5.2.4 assemblies and exits. It never attaches to an existing PID. Explicit `--launch` creates a separate original process with isolated profile settings and child-only preload, preserving normal Steam initialization. No original executable, assembly, normal save or global configuration is patched.

[`ReferenceHuntEncounterProbe.cs`](../../rebuild/tools/ReferenceHuntEncounterProbe.cs) installs one hidden transient observation component at the existing main-thread fence and samples through `WaitForEndOfFrame`. It reads `SceneEntityCreator.self.GetMissionPoint(9126,1518)`, then the existing private `_agents` list through a narrowly named reflection field. It does not enumerate arbitrary nearby hares and assume they belong to the objective. It uses existing `MainPlayerCmpt.gMainPlayer` and the source's simple `MissionManager.Instance` getter, avoiding lazy singleton creation. The component never calls mission mutation, spawn, death, damage, RNG, input, animation-playback or save APIs.

Each frame records:

- Unity frame/game/realtime, actual mission/point presence and the observed `MONSTER0` text.
- Player identity, source position/quaternion/scale and current combat attributes.
- Each mission member's list order, scene-agent ID, prototype, creation step, agent anchor/rotation/scale, `canRide`, and nullable actual entity.
- For an actual entity: entity ID, prototype/type, current world position/quaternion, effective `PeTrans.scale`, HP/maxHP/attack/defense and CampID/DamageID/DefaultPlayerID.

Agent membership, an existing entity and a rendered/visible actor are different observations. Six populated records do not by themselves establish that six creatures were visually counted on screen. The probe deliberately provides no random-center estimate. Its source SHA256 is embedded in the header; the normal launch manifest retains executable/profile/assembly/shim/output paths and limits. Reflection/gameplay getters, allocations, output IO and the observation component itself necessarily add process overhead; “observation-only” means no gameplay mutation, not zero process side effects.

Limits retain the existing pose-helper frame/time/wait options:1–3600 samples,1–120 capture seconds, and1–1800 seconds waiting to arm. This probe adds a16MiB output bound and a64-member safety bound. Stop/timeout/error destroys its own component after disposing output. A paused or unfocused Unity client may delay frame-driven teardown; close the process if necessary. Forced termination may leave a partial recording, which the analyzer will reject as an incomplete capture.

## Sole-operator capture sequence

The root desktop operator owns all original input. Do not start this helper concurrently with an existing game session merely to inspect its archive. Preserve original save4/pre-arrival and save5/zero-kill checkpoints. A capture of reloaded save5 is a **newly generated reload instance**, not recovery of the six actors that existed at the first arrival.

```sh
# Offline only; no profile writes or original process.
python3 rebuild/tools/reference_hunt_encounter.py
python3 rebuild/tests/test_reference_hunt_encounter.py
python3 rebuild/tests/test_reference_pose.py

# Explicit separate original launch. Use another filename if any capture,
# arm, stop or launch-manifest file already exists.
TMPDIR=/home/admin/source/planetexplorers/rebuild/build/compiler-tmp \
python3 rebuild/tools/reference_hunt_encounter.py --launch \
  --user-data /home/admin/.local/share/planet-explorers-hunt-reference \
  --output /home/admin/.local/share/planet-explorers-hunt-reference/pose-captures/hunt-reload-01.jsonl \
  --seconds 30 --frames 600
```

Load manual slot6 through the original UI, then create the exact printed `.arm` path. Let the original continue its normal actor creation; the observer samples incomplete membership too. If possible, avoid combat before the first complete sample, and pause after the capture to limit unplanned progression. Do not change player position/health, mission flags, actor scales or RNG through the probe. For a direct comparison of two reload instances, finish one capture, reload the same preserved checkpoint through UI in a separately prepared recording session, and compare observed anchors/scales/IDs and the unchanged mission counter. A later nonzero-kill checkpoint should be a new slot, never overwrite the zero-kill source checkpoint.

Analyze the closed recording into a fresh report:

```sh
python3 rebuild/tools/analyze_reference_hunt_encounter.py \
  /home/admin/.local/share/planet-explorers-hunt-reference/pose-captures/hunt-reload-01.jsonl \
  --require-six --output rebuild/build/hunt-reload-01.report.json
```

The analyzer requires a valid closed recording and selects the first complete six-alive membership whose agent/entity identities also match the following frame. It rejects duplicate identities, malformed/nonfinite metadata, misordered frames, incorrect mission/schema and error/truncated endings. Missing, dead, incomplete or replaced cohorts do not become fabricated spawn records. Its report preserves both anchored and live coordinates. It is evidence preparation only: it does not generate C actors or automatically enable the native encounter. Any eventual pinned C cohort must identify the exact captured instance and preserve sampled scales/attributes while using the common authoritative collision/support contract.

## Offline verification and remaining gates

The observation assembly and native shim/compiler build successfully against the installed original assemblies. Seven focused analyzer tests pass, covering complete membership, distinct anchor/live pose, missing/uncreated/dead/wrong-prototype actors, replacement identities, malformed geometry, incomplete/error capture and invalid ordering. The existing six fake-process shim tests also pass, including main-thread-only installation, executable/library guards and one-time installation. These tests do not launch or instrument the original.

Two later sole-operator runs established live callback/reflection access, six-member identity/scale/health capture and closed bounded recordings, as detailed below. Pinned source positions alone will not implement dynamic collision, general player attacks, hostile hare behavior, four-death dispatch, cleanup or source-compatible reload semantics. Keep those runtime/save changes separate from this observational tool.


## Source combat attributes and permission gates

These are source-derived defaults, not evidence that a captured actor is currently attacking. [`DbAttr.ToAliveAttrWithScale`](../../Assets/PeWorld/PeEntityCreator.cs) scales only HP, maximum HP, attack and defense. Prototype13's normal base values are212/212/64/29 with source uniform scale[.8,1.3]; Easy buff30200168 additionally halves these values. Movement speeds are not multiplied by this attribute scaling. Preserve actual captured scale/currentHP/maxHP/attack/defense, and distinguish current injury from the maximum; do not refill captured currentHP merely because the source base is212. `EEntityProto.Monster` is enum3 (`Npc` is2). The observer records the raw enum and the analyzer now validates3 for a complete cohort.

The source hare defaults are CampID45, DamageID45 and DefaultPlayerID4; player defaults are1/1/1. [`ForceSettings_Story.xml`](../../Assets/Resources/ForceSetting/ForceSettings_Story.xml), loaded by the [original game loader](../../Assets/PeLauncher/PeGameLoader.cs), maps player1 to mankind force1 and player4 to creature force2, with enemy conflict enabled. `isaddhatred` contains1 for both45→1 and1→45; this is the table read by [`DamageData`](../../Assets/Scripts/PeEntity/Cmpt/TargetCmpt.cs), not the older similarly named AI table. `initthreat`45→1 is5, while1→45 is0. Initial autonomous hatred is therefore not interchangeable with whether an explicit player attack can damage the hare. Mission kill-beacon creation does not overwrite these faction attributes, as established above.

[`SkExpFuncSet.CanDamage`](../../Assets/Scripts/SkillSystem/SkExpFuncSet.cs) requires all three conditions: `PEUtil.CanDamageReputation`, `ForceSetting.Conflict` and nonzero `DamageData.GetValue`. It resolves projectile caster ownership before those checks. The current source's [`PEUtil.CanDamageReputation`](../../Assets/Scripts/Assist/PEUtil.cs) does not apply its above-Neutral reputation veto in Adventure, while autonomous `CanAttackReputation` still rejects either-direction known reputation at or above Neutral. These are different gates. Do not copy autonomous selection permissions into direct damage blindly or treat a nonzero damage-table cell as authorization by itself.

## Perception before hostility and attack

A simple distance threshold cannot reproduce this source path:

1. [`PEVision`](../../Assets/Scripts/Assist/PEVision.cs) refreshes every20 component updates. It considers entities with views, checks horizontal distance at most `radius + target.maxRadius`, checks a **strict3D angle** between the animated sensor's transformed local axis and target center, rejects occluded targets using a ray from sensor position to target center, and excludes snakes. The angle field is compared directly; do not divide it by two as if it were a full cone width.
2. [`PEHearing`](../../Assets/Scripts/Assist/PEHearing.cs) refreshes every16 component updates, also uses horizontal `radius + target.maxRadius`, and uses the same sensor-to-center block ray unless its serialized `isIgnoreBlock` is true. It does not require a fabricated “moving/noisy player” condition. [`PEPerception`](../../Assets/Scripts/Assist/PEPerception.cs) defines its blocking mask from SceneStatic, VFVoxelTerrain, Unwalkable, TreeStatic, Building and NearTreePhysics. That mask differs from the source camera mask and must not share it accidentally.
3. [`TargetCmpt`](../../Assets/Scripts/PeEntity/Cmpt/TargetCmpt.cs) updates its selection logic every15 component updates, collects the actual hearing/vision lists, skips self/dead entries, then applies reputation, force conflict and nonzero absolute initial threat, plus carrier compatibility. Existing enemy/threat/special-hatred state remains relevant. Separate `CanAttack()` requires a view, enabled attack flag, no serious injury, no daytime-dark restriction and no selected escape enemy.
4. After target selection, actual melee still requires runnable skills, strict behavior cooldown, authored bounds-gap/facing/HP/obstruction checks, skill cooldowns and a matching animated contact window. Pounce is another action with its own rules. A successful perception sample is not a damage event.

Earlier serialized prefab inspection reports hearing15, vision20 and angle60 on the hare's animated head sensors, differing from database10.8/18/75. Those source-prefab values are not a substitute for capturing the exact installed live sensor transform/component settings and relevant target state. This observer deliberately does not add a new perception implementation or expand its capture into arbitrary private AI state. Until that contract is ported or a bounded adapter is explicitly documented, pass only a separately justified selected target to `pe_hare`; do not silently use “within20m” as hostile selection. Captured six-agent positions/scales alone do not resolve this AI boundary.


## Two observed zero-kill reload instances

The root operator launched the observer separately twice and loaded the same preserved manual slot6 (`save5`) through the original UI. [Compact comparison evidence](../reference/hunt-encounter-reloads.json) retains each selected frame, raw capture SHA256, closure reason and frame count; the full600/338-frame JSONL files remain outside the repository in the isolated profile. Both saved checkpoints (`save4` and `save5`, seven files each) still match all previously archived hashes. No manual save was written during these runs.

| Observation | Reload01 | Reload02 |
| --- | --- | --- |
| Closed frames / reason |600 / capture-limit|338 / stop-file|
| Mission counter throughout capture |`13_0`|`13_0`|
| Mission-owned scene-agent IDs |509–514|511–516|
| Actual entity IDs |2146483648–2146483653|2146483650–2146483655|
| Six actors' health |All six alive and at captured maximum HP in every frame|All six alive and at captured maximum HP in every frame|
| Player first observed→last HP |5.90397644→0|109.451553→0|

All six anchors and all six sampled scales changed between reloads. Numerical IDs overlap across processes and are not a global identity key. This supports the source's fresh-cohort behavior for **zero-kill reload**. Neither run establishes a nonzero kill counter surviving reload, how many agents reappear after kills, or an identical RNG sequence. First recorded player HP was already below500; the observer was armed after simulation resumed, so this does not describe HP at the instant of loading.

The operator saw nearby hares attack and the player die without player attacks in both runs; [the hostile reload frame](../reference/hunt-original-reload-hostile.png) preserves that observation. Six mission-owned entities were enumerated by the observer, but six simultaneous visible actors were not manually counted. The HP trace and visual attacks do not identify the source skill/contact responsible for every individual decrement. Both clients subsequently received Quit Game and its confirmation through the UI; each launcher returned137. The probes had already closed normally with the reasons above; client process exit137 is not described as a clean zero exit.

## Pinned native reference input

[`export_adventure_hunt_encounter.py`](../../rebuild/tools/export_adventure_hunt_encounter.py) validates the original report by independently analyzing its unchanged raw capture, then archives only its first complete six-alive frame and the following frame in [bounded cohort evidence](../reference/hunt-encounter-cohort.json). The two frames must retain the same agent/entity identities and source anchors. This export selects reload01 deliberately; it does not average or combine the two random cohorts.

| Source agent / entity | Source agent anchor XYZ | Observed scalar scale | HP=maxHP | Attack / defense |
| --- | --- | ---: | ---: | ---: |
|509 /2146483648|(-351.1564,19.6848335,222.139832)|.8217651|174.2142|52.592968 /23.8311882|
|510 /2146483649|(-354.059723,19.6894836,224.221024)|1.28230214|271.848053|82.06734 /37.1867638|
|511 /2146483650|(-351.383636,19.7065277,222.970779)|1.21332836|257.2256|77.65301 /35.18652|
|512 /2146483651|(-354.934631,19.5676346,219.5362)|.919498265|194.933624|58.8478851 /26.6654472|
|513 /2146483652|(-348.211334,19.9097633,225.626831)|1.1775682|249.644455|75.3643646 /34.14948|
|514 /2146483653|(-348.695465,19.853508,224.413254)|.834619939|176.9394|53.4156723 /24.2039757|

The original agent rotations are identity, hence source spawn yaw0. All three sampled live scale components are preserved in bounded evidence; tiny floating-point transform differences stay within the checked uniform-scale tolerance. The generated scalar uses observed X. The current live positions and rotations are also retained as evidence, but they had already moved and **are not used as spawn anchors**. CampID45/DamageID45/DefaultPlayerID4 and prototype13/enum3 are checked explicitly. Captured stats agree with Normal212/64/29 scaling; currentHP remains the captured value, not a regenerated refill.

Reproduce or check the committed outputs without Steam/raw captures:

```sh
python3 rebuild/tools/export_adventure_hunt_encounter.py
python3 rebuild/tools/export_adventure_hunt_encounter.py --check
python3 rebuild/tests/test_adventure_hunt_encounter_export.py
```

The initial evidence-extraction command was:

```sh
python3 rebuild/tools/export_adventure_hunt_encounter.py   --from-report rebuild/build/hunt-reload-01.report.json
```

It refuses to overwrite existing bounded evidence. Normal regeneration reads the checked-in evidence and source hashes, producing [the header](../../rebuild/generated/pe_adventure_hunt_encounter_source.h) and [provenance manifest](../../rebuild/generated/pe_adventure_hunt_encounter_source.json). Every scaled capsule must fit the source route bounds; unsupported anchors are rejected, never clamped or rerolled. Six exporter tests pass for anchored-versus-moving coordinates, duplicate/missing/replaced actors, source bounds, invalid scale/health/faction, changed provenance/counter and independent raw/report agreement with bounded extraction. Seven analyzer tests and the existing six shim tests pass. The exporter installs no simulation, graphics, random respawn logic or save fields.

A separate strict C route-preparation test now initializes all six captured anchors against actual terrain/building collision with their exact scales and checked source attributes. It stages the group before commit and rejects a blocker affecting only the final actor atomically. The test preserves each recorded source spawn; ordinary capsule-contact skin may adjust the settled position by at most.003m in XZ. That is a headless support check, not physical native creature rendering, hostile AI, combat or source-compatible reload behavior. Broader collision/host changes and final sanitizer results belong to their implementation owners.
