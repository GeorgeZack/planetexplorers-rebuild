# Original hunt combat observer

This is a separate observation tool for the next9126 parity gap. It does not change the previously captured six-agent cohort or its exporter, enable native combat, alter original skill behavior, or write player saves. The [encounter audit](ADVENTURE_HUNT_ENCOUNTER.md) establishes zero-kill reload/spawn evidence; the [damage helper audit](HARE_DAMAGE.md) describes the headless capsule/HP contract that still needs a justified runtime guide and selected target.

## Source clock and selection boundary

[`SkInst.Exec`](../../Assets/Scripts/SkillSystem/SkInst.cs) sets `_startTime`, runs prepare effects/waits and enters its guide loop. It classifies trigger events, disables external collision execution, registers collision-capable instances on their caster and increments `GuideCnt` before applying that guide's animation/effects. `GuideCnt` is one-based. After the pre-time wait, it clears `_hitInGuide` and enables `_bExecExtEnable`; after the main-time wait it disables the flag, runs post-time and unregisters that guide. The loop condition can begin another guide on the same skill instance. Cooling is a distinct negative step. Merely seeing an attack clip or a running skill does not prove external collision damage is enabled.

[`WaitTimeSkippable`](../../Assets/Scripts/SkillSystem/SkUtils.cs) continues while `Time.time - start < duration` and the supplied skip flag is false. Coroutine scheduling quantizes those boundaries to Unity updates. Source Sword/Gun actions write skip flags; the inspected `BTMelee` branch starts its selected skill and waits for skill completion/attack state without those writes. Interrupted skills have a separate stop path. Database guide durations are therefore inputs to the coroutine, not evidence that a particular live guide began at an inferred normalized Animator fraction. [`ExecEventsFromCol`](../../Assets/Scripts/SkillSystem/SkInst.cs) separately checks the collision-enabled flag, successful-hit dedupe and explicit target restriction before executing the collision event.

The source sensor and target contracts are detailed in the [encounter audit](ADVENTURE_HUNT_ENCOUNTER.md). The new probe reads both raw selected `m_Enemy` and the existing `GetAttackEnemy()` result, search/attack flags, escape selection, sensor membership and each component's frame counter. It does **not** call target search, perception refresh, hostility setters, raycasts, skill runnability, or lazy singleton getters to manufacture state. These snapshots expose actual selection, not a complete proof of every reputation/carrier/darkness/serious-injury eligibility condition.

## Capture schema and limits

[`ReferenceHuntCombatProbe.cs`](../../rebuild/tools/ReferenceHuntCombatProbe.cs) installs one transient observer on the Unity main thread through the established launch-only shim. Loading the assembly and sampling affect process state/performance; gameplay inputs, saves, RNG and entity state are not changed. It uses the existing9126/1518 mission list, never global proximity to infer encounter membership. Header `kind` is `observed-original-hunt-combat`, schema1, with the exact probe source SHA256. The earlier encounter probe remains unchanged.

At `WaitForEndOfFrame`, each existing mission entity and the player records:

- Existing identity, position, rotation, scale, HP/stat/faction values and source agent anchor.
- Cached target state and its live vision/hearing components: enabled/active, radius, angle/local axis/transformed world axis, hearing ignore-block, block mask, transform path/world pose/local position/lossy scale, component counter and current detected entity IDs.
- Existing pooled caster skills: process-local observation instance ID, source skill ID, `GuideCnt`, active flag, `_bExecExtEnable`, `_startTime`, cooldown flags, explicit/current temporary target and successful guide hit count. The observer reads the private static singleton **backing field** rather than `Instance`, so an absent pool stays absent. Pooled cooling entries remain distinguishable from active skills.
- Existing model Animators: enabled/active/controller/speed, current and next state hashes, normalized times, lengths, speed/multiplier/loop, transition flag, layer weight and current clip identities/weights.

The probe never starts/stops skills, changes Animator parameters, evaluates a pose, enables a sensor or advances simulation. It creates fresh output only, bounds capture to3600 frames/120 seconds/64MiB, and bounds all inspected collections. Default is600 frames/20 seconds. It reads full existing skill lists (up to4096), filtering each entity's caster; no skill type is silently reclassified as one of the five melee attacks. Observation IDs are transient and never native entity/save IDs.

## Offline checks and supervised workflow

Offline compilation against the installed Unity5.2.4f1 and Assembly-CSharp assemblies passed. An independent offline Mono metadata traversal verified the exact private pool, skill, TargetCmpt and inherited perception fields without executing the game. Eight analyzer tests pass for contact-enable transitions, guide changes, no fabricated absent skills, identity alias rejection, closure validation, nonfinite/malformed sensor fields, paused versus reversed clocks and setting changes. These checks do not yet establish a successful live combat capture.

```sh
python3 rebuild/tools/reference_hunt_combat.py
python3 rebuild/tests/test_reference_hunt_combat.py
```

Only the designated desktop operator launches the original. Preserve hashes of both archived checkpoints, choose a fresh output, load manual slot6 through the original UI and create the printed `.arm` file immediately before resuming. Capture should include living-player perception and several attack starts if ordinary gameplay permits. Death or a pause can limit observed skills; never modify health or force a skill to improve coverage. Stop with the printed `.stop` file, inspect the end record and quit through the original UI. Do not manually save this observational run.

```sh
python3 rebuild/tools/reference_hunt_combat.py --launch \
  --user-data /home/admin/.local/share/planet-explorers-hunt-reference \
  --output /home/admin/.local/share/planet-explorers-hunt-reference/pose-captures/hunt-combat-01.jsonl \
  --frames 1200 --seconds 40
python3 rebuild/tools/analyze_reference_hunt_combat.py \
  /home/admin/.local/share/planet-explorers-hunt-reference/pose-captures/hunt-combat-01.jsonl \
  --output rebuild/build/hunt-combat-01.report.json --require-melee
```

The analyzer rejects partial/error closures and malformed data, retains exact first sensor snapshots/settings variants, selected-target and HP changes, and every observed skill-state transition with matching Animator snapshots. `--require-melee` requires at least one of30100003/4/8/46/47 with observed external execution enabled. It is not a requirement that all five attacks, both guides of Attack2, or any successful contact occurred. End-of-frame samples bracket state changes; HP changes cannot be assigned to a particular simultaneous attacker solely from coincident timing. Archive a bounded source-hashed summary after live review; retain large JSONL files only in the isolated profile.

## First supervised attempt: incomplete native crash

The original was launched with the observer as PID540553 and armed during loading. It wrote18 complete frame records, all six hare pooled-skill lists empty, then crashed without an end record. The analyzer rejects this file; it is not exported as successful combat evidence. [Bounded failure evidence](../reference/hunt-combat-failed-observation.json) retains hashes and diagnostic facts. The isolated Player.log was copied to `rebuild/build/hunt-combat-01-failure-player.log` and exact probe source to `rebuild/build/hunt-combat-01-failure-probe.cs` before any retry.

The October5 23:26:18 CDT system core confirms SIGABRT after Mono reported a native SIGSEGV. The faulting thread540567 differs from the Unity main thread540553. Its recovered stack passes from anonymous machine code through NVIDIA `libnvidia-glcore.so.580.178.04`; the main thread was waiting in native synchronization. At the fault, `mov (%rsi),%edi` dereferenced RSI=0xc. No method name was recoverable for that anonymous code. Describing it as a proved Mono managed-JIT or probe Animator-getter failure would exceed the evidence. Graphics work is implicated by the worker stack; the observer's causal role remains unresolved.

No OOM event appeared in the90-second crash window. At diagnosis4.4GiB memory and13GiB swap were available, which does not reconstruct peak memory at the crash. The full thread/register diagnostic remains in ignored build output; the temporary extracted core was deleted after inspection. No system settings or installed files were modified for diagnosis.

The incomplete early rows show live head vision20/angle60/axis(-.5,1,0), hearing15/ignoreBlock=false, both enabled/active and block mask2193408, consistent with independent installed-prefab recovery. These are explicitly diagnostic samples from an invalid capture, not accepted timing evidence. Root has not established a melee guide/contact window from this failed run.

A retry can enable `PE_HUNT_COMBAT_TRACE=1` to flush narrow per-frame/entity stage markers to the isolated log; `PE_HUNT_COMBAT_ANIMATORS=0` omits Animator sampling for a reduced diagnostic comparison. Defaults retain full observation, and both choices are recorded in the header. Animator discovery now additionally requires an existing view. Offline compilation and eight analyzer checks pass after this instrumentation. Later arming after the loaded view is stable is a proposed diagnostic variation, not a demonstrated crash fix.

## Successful retry: closed630-frame combat observation

The next original launch loaded preserved manual slot6. The player died before arming; the operator then armed observation and clicked the ordinary revive-item OK once. No player attack input or manual save occurred during capture. The probe closed after630 frames with `stop-file`; `analyze_reference_hunt_combat.py --require-melee` passes. [Bounded validated evidence](../reference/hunt-combat-observed.json) retains the raw/source hashes, first world sensor observations, stable settings, target/HP changes and skill transitions with Animator snapshots. The full report/JSONL remain local. Probe closure is separate from the original client's eventual exit result.

All six hares' observed head sensors agree with the installed export: vision20/angle60/local axis(-.5,1,0), hearing15/ignoreBlock=false, enabled/active and block mask2193408. Their world transforms vary with motion; this does not justify replacing the animated sensor with a root-centered radius. Three hares selected actual player20000 after revival and cleared that selection at player death. This is bounded target-state evidence, not recovery of every original threat/arbitration rule.

Player HP was0 at arming, then1 and166.5 during the ordinary revival, followed by119.938583,96.6578751 and0. Mission counter stayed `13_0` in every captured frame. The observer recorded three skill instances:

| Skill | Observed guide/contact state | Associated observed attack clip |
| --- | --- | --- |
|30100053|Guide1, external execution enabled; successful-hit count0→1|`attack5_pouncefront`|
|30100004|Guide1, external execution enabled; successful-hit count0→1→2|`attack1_handhitrightfront`|
|30100047|Guide1, external execution enabled; successful-hit count0→1→2|`attack4_normalattack1`|

The two melee skills were already contact-enabled at their first recorded start-time sample, consistent with their exported zero pre-time/single-main-guide definitions. Their first Animator sample still showed locomotion/attack-idle, with the attack clip appearing later; skill elapsed time and Animator normalized time are separate observations. A successful-hit count is not itself a player-only hit count: subsequent counts with a null current temporary target cannot be assigned to a victim from these fields. Coincident HP decrements and clip samples likewise do not independently establish exact causal damage attribution.

No30100008/Attack2 instance or two-guide transition occurred in this capture. The native Attack2 scheduler remains source-derived, not validated by this live trace. Original pounce is now directly observed and remains outside the native melee-only controller. Noncritical-only native damage, fixed60Hz guide waits and its selection/order policies also remain documented adapters; the successful capture does not certify them as exact Unity parity.

After capture closure, the operator clicked the second death dialogue's Cancel. A loading transition returned the original player to town; the UI subsequently showed2/4 and then a return-to-town objective without attack input. These later UI observations are outside the `13_0` capture and do not identify which entity or cause produced the deaths. The root operator saved a separate [completed-kill checkpoint](../reference/hunt-original-return.json), now decoded as9126 completed/9127 active with return target(-266,25,136); partial nonzero-kill reload has not been established by this sequence.

The successful retry client PID557717 subsequently received System → Quit Game → Yes through the original UI; its launcher returned137. The observer had already closed cleanly. This is not a clean-zero client-exit claim. Read-only post-exit hashing confirms all files in the saved target, encounter and return checkpoints remain identical to their recorded evidence.
