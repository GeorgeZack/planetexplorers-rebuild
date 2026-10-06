# Adventure Town Defense9077

October6,2026. **Observed in the original:** the user's ordinary client session advanced from Bug Hunt into the defense briefing, displayed two waves and a15-credit objective, and reached7/15 before the recorded scene froze. A later operator run using `-force-gfx-direct` survived to defense completion without player attacks after acceptance; fresh manual9 records9077 completed once. **Source-derived:** exact quest, dialogue, two-wave schedule, three species and beacon rules below. **Exported:** guarded metadata. **Implemented in C:** an independent headless clock only; no defense host, captured wave anchors or random cohort is claimed. Original preparation reload is observed separately below; exact timers and partial-kill reload remain unverified. A later fresh manual8 archive pins the beacon center and protected NPC; see the final section.

The [metadata exporter](../../rebuild/tools/export_adventure_town_defense.py) generates [source contracts](../../rebuild/generated/pe_adventure_town_defense_source.json) and [C constants](../../rebuild/generated/pe_adventure_town_defense_source.h). The cohort availability flag remains0; there are no fabricated coordinates, entity IDs or scales. Six exporter checks pass, including rejection of changed modern schedule/species bindings and explicit separation from the similarly named legacy tables.

## Quest and briefing

`AdRandomQuest_List9077` is **Attack of the Killer Animals**, prerequisite completed9127, target7507, AutoReply1, no item reward, no ending conversation, no `NextQuest`, and cannot be abandoned. Its description refers to `Town%004`. Closing9127's final4106 begins the9077 opening; the normal talk trigger is attached to the last opening page, so acceptance occurs after4110. Numeric talk IDs here are source identities, not IDs visible in the video.

| Talk | Bound role | Source text |
| --- | --- | --- |
|4107|4, Sajiva in this reference|We have incoming! A pack of monsters is coming right for our town!|
|4108|3, Naoko|“AdvNPC%000”, I thought you got them all? Are they here for revenge?|
|4109|4|Whatever the case, they're gonna be on us soon!|
|4110|3|Everyone get ready to fight!|

Use the generated English strings for exact punctuation and dynamic tokens. Speaker tokens are-9897→role4 and-9898→role3; these are not monster or entity IDs. The earlier completion story4 moves role4 toward the player; its existence does not itself spawn defense waves.

Target `AdQuest_TowerDefence7507` has label Town Defense, `Time=30`, `Pos=5_4`, `NpcList=0`, `ObjectList=0`, `Count=15`, `td_id=520`, `ReceiveQuest=0`. `PosType5` means **town/camp ID4**, not role4 and not world position(5,4). [`VArtifactUtil.GetTownPos`](../../Assets/Scripts/PuJi/Town/Tool/VArtifactUtil.cs) resolves `GetTownByID(4).TransPos`; [`CreateMonsterBeaconByTDID`](../../Assets/Scripts/Scene/EntityMonsterBeacon.cs) falls back to the current player position if that lookup fails. The resulting position is recorded as `finallyPos`.

The empty database NPC list does not prove no protected NPC. [`MissionManager`](../../Assets/Scripts/Mission/MissionScript/MissionManager.cs) binds the random quest giver and replaces the tower NPC list with that entity. Beacon start makes listed NPCs non-invincible; `UpdateTowerMission` fails the mission if a protected entity is dead. The later fresh active9077 archive pins that binding toNaoko20018; see the final section. No range/time-limit fields are parsed for this Adventure target, so do not invent a radius-failure or survival-timer objective.

## Correct schedule and species

The active single-player path is `MissionTowerDefense.ProcessMission`→`SceneEntityCreator.AddMissionPoint`→[`SceneEntityMissionPointTowerDefence.Start`](../../Assets/Scripts/Scene/SceneEntityMissionPointTowerDefense.cs)→`EntityMonsterBeacon`. It loads **MonsterBesiege520** and **MonsterBesiegeSpawn** through [`AISpawnTDWavesData`](../../Assets/Scripts/AiScripts/AiSpawn/SPClass.cs).

**Do not use `MonsterSiege_SpawnId520`, `MonsterSiege_SpawnTypeId` or `AdQuest_TD_Info` as this schedule.** Other/legacy paths reference those similarly named tables; row520 there describes six waves and would contradict the actual two-wave beacon and video.

| Wave | Delay before wave | Group | Prototype | Source integer count | Angular range |
| --- | --- | --- | --- | --- | --- |
|0|Initial30 + row delay0|501|13, Lepus hare|8–9 (`Random.Range(8,10)`)|0–360°|
|0|same|503|4, coelodonta rhino|2 (`Random.Range(2,3)`)|0–360°|
|1|10-second cooldown, then20-second prep|510|7, lupus wolf|8–9 (`Random.Range(8,10)`)|0–360°|
|1|same|503|4, coelodonta rhino|2 (`Random.Range(2,3)`)|0–360°|

The potential cohort is20–22 actors; the completion goal is15 deaths. Do not turn the goal15 into a spawn count or infer actor identities from the quest counter. Scripted spawn types≥500 bypass area-dependent type selection here; each referenced spawn row maps all terrain lists to its one normal prototype. The beacon changes created actors' DefaultPlayerID to8 and CampID to26; it does not replace their DamageID in that callback. Those permissions differ from ordinary fauna and the earlier pinned hunt cohort.

Base database dependency values, **not captured scaled instances**:

| Prototype | Scale range | Base HP | Attack | Defense | Installed bundle |
| --- | --- | --- | --- | --- | --- |
|13 hare|.8–1.3|212|64|29|`AssetBundles/Monster/m_lepus_hare.unity3d`|
|4 rhino|.8–1.2|2535|51|52|`AssetBundles/Monster/m_coelodonta_rhino.unity3d`|
|7 wolf|.8–1.3|495|45|30|`AssetBundles/Monster/m_lupus_wolf.unity3d`|

All three installed bundle files are present locally. Species4/7 are currently missing native actor, collision/Defence, sensor and skill recovery; substituting hares is not parity. Their original prefabs are under `Assets/Prefabs/AiPrefab/AiPrefabEntity/AiPrefabMonster/Monster/`, and behavior XML under `Assets/Resources/AiPrefab/EntityData/Monster/` using the corresponding `M_coelodonta_rhino`/`M_lupus_wolf` names. Wolf behavior includes melee30100482 and moving pounce30100483. Rhino behavior lists melee30100063–67 plus30100068–72 for additional directional attacks. Their attack shapes, Animator poses, guide clocks, mobility and threat selection must be recovered separately; the hare controller is not an automatic compatible substitute.

## Clock, Ready and death semantics

`RefreshTowerMission` is a Unity coroutine. Initial `PreTime` is `start_time30 + first delay0`; while positive it yields `WaitForSeconds(1)`, then decrements by1. When ready it clamps0 and creates every group/agent for the wave synchronously. `handlerNewWave` runs before decrementing displayed waves remaining and incrementing the internal wave index.

Between waves it waits a separate10 iterations of `WaitForSeconds(1)` while displayed `PreTime` stays0. Only after cooldown ends does it assign the next row's20-second prep. Second-wave spawning is therefore nominally30 scaled seconds after the first, plus coroutine/frame scheduling, not20. `WaitForSeconds` follows scaled Unity time; this is not a wallclock stopwatch. A fixed C clock would be an explicit scheduling adapter; a paused step must preserve residual time and never create a catch-up burst on resume.

The Ready callback directly assigns `PreTime=0`. During an outstanding prep wait, spawning still waits for that coroutine wake; Ready does not cancel the wait object. During the separate cooldown, Ready changes a value already0 and cannot skip the10-second loop or the later assignment of20. Source does not wait for all members of one wave to die before scheduling the next.

Each actual spawned beacon actor subscribes to death; any killer can trigger credit. `OnOneDeath` increments the second field of `TDMONS0` and completes7507 exactly at15. It is not necessary to kill every spawned actor. Stopping the mission destroys its beacon; under `DestroyLeftMonster`, remaining live monsters receive an outward Run request and delayed destruction, their scene agents are removed and the defense UI/map mark are hidden. Native immediate removal, if selected, would need explicit adapter labeling. NPC participation is consequential: the user's video shows residents near the fight, while the current native controller supports a sole-player threat adapter.

## Persistence and follow-ups

[`AdRandomMission.Export`](../../Assets/Scripts/Mission/AdRandomMission.cs) writes tower target serializer **kind6**, present flag, protected-NPC count/IDs and `finallyPos` XYZ. This is separate from target number7507. It does not serialize beacon wave index, prep/cooldown timer or cohort there. `PlayerMission.Export` preserves mission flag strings, including `TDMONS0`, and completion records. Beacon `Start` initially writes `0_0_0_0_0`; `InitPlayerMission` also restores recorded flags around mission initialization. This source interplay must not be simplified into an untested claim that all progress resets or that exact waves resume. Reloading original manual8 physically restored active defense at0/15 with two waves and later started the first wave; the delayed first capture does not establish exact preparation seconds or partial-kill reload behavior. No native save layout should be extended merely to mimic a guessed policy.

9077 has no automatic next quest. **9078 Return Home** requires9077, appears in chain32 group3, opens via4111→4112, and discovers4504 at role2's starting reference (`3_2_0_5`, strict5m). **9079 Early Transportation** requires9078, appears in chain31 group2, opens4113→4114→4115, then discovers4505 at role5's starting reference (`3_5_0_5`). Its acceptance plot is`1_5`; this is another later scope. These are unlocks, not permission to fabricate their destinations or auto-complete the travel. Neither follow-up was reached in the recorded user session.

## Video boundary and next implementation

The user's uninstrumented segment012 shows briefing4107–4110 around79–80.25seconds, then the [two-wave preparation panel](../reference/user-defense-prep.png) with Monster Num0/15. The [Rope Gun toast](../reference/user-defense-rope-reward.png) appears alongside the new defense objective. Later the tracker progresses1/15→3/15→6/15→[7/15](../reference/user-defense-seven.png); wave display reaches0 remaining. This corroborates presentation and progress, not exact spawn identities, all species, killing actor, count draws or center coordinates. The subsequent native-client crash is documented separately in [bounded crash evidence](../reference/user-playtest-crash-20261006.json). A final frozen video frame does not establish defense failure/completion.

The implemented independently testable slice is [`pe_town_defense_clock`](../../rebuild/src/pe_town_defense_clock.h), disconnected from host quest acceptance. It models initial prep30, separate cooldown10, second prep20, pending-wait Ready semantics, paused residual time and one-shot wave0/1 events. Each call accepts scaled display-frame elapsed time0..1seconds; at most one coroutine wake occurs, overshoot is discarded and the next full wait begins on that frame. This is an explicit Unity coroutine scheduling adapter, not exact engine end-of-frame timing. A finished clock means both wave requests were emitted, never that15 deaths occurred.

The clock has no actors, random draws, death credit, callbacks, quest state or save fields. Its phase and event are left unchanged on invalid input. A later host must transactionally stage cohort creation and retry failed placement without partially committing actors; that owner must also deduplicate actual member deaths. Clock readiness alone must not enable playable9077 while required species/geometry are missing. Focused optimized strict-warning and ASan/UBSan/leak checks pass for30+10+20, deferred Ready, cooldown Ready rejection, no catch-up, pause/resume, single event emission and invalid-state preservation. The standalone test is registered in `make test`; no graphical defense playtest is implied.

To enable playable defense, retain the fresh active9077 archive and obtain a bounded live beacon observation: wave index/timers, per-wave agent anchors, IDs, prototypes, scales and stats. The archived center/protected NPC are now known, but the wave cohort is not. Check the **entire20–80m source spawn annulus** against installed route terrain/building coverage, not just its center or the earlier hunt disk. Recover prototype4/7 render/capsule/sensor/skill assets and source-backed town-defense movement/permissions. Then implement actual membership credit, NPC interaction/failure policy, cleanup and a tested persistence adapter. If some original behavior remains deliberately bounded, label that boundary explicitly. No clamping, rerolling or all-hare substitute is authorized by this metadata.

```sh
python3 rebuild/tools/export_adventure_town_defense.py --check
python3 rebuild/tests/test_adventure_town_defense_export.py
make -C rebuild town-defense-clock-test
```

## Installed species feasibility, read-only October6 inspection

A UnityPy1.25.4 typetree inventory of the exact installed rhino/wolf bundles is retained in [bounded asset metadata](../reference/town-defense-species-assets.json). This reads assets only; no models, animation files or graphics were generated. The larger scratch inventory remains `rebuild/build/defense-species-feasibility.json`.

| Dependency | Rhino4 | Wolf7 |
| --- | --- | --- |
|Bundle SHA256|`393ddf14feb608886486d39532c8b3b5cc0036495188c7831a0274ee5224c295`|`f8e274a9e1c1ae479f88bf12067a440fad8bbf372258fc04bf732b582681aa38`|
|Live skin renderer pathID|9061803555269046225|95028923182282536|
|Mesh pathID|7236384840505367644|-7480331273908971926|
|Model-root transform|705351890664988904|-5710197759955247117|
|Controller pathID|5289878430810792808|6856608753594384806|
|Mesh|3678 vertices,43 skin joints,4 submeshes|3023 vertices,45 skin joints,1 submesh|
|Animation clips|37|23|
|Defence parts|17|27|
|Attack parts|17 across9 source components|2 across mouth/body components|

Both bundles also contain ragdoll skin renderers. Use the live model branch, not the first `SkinnedMeshRenderer` found. Rhino live path is `M_coelodonta_rhino/mcoelodonta_rhino_model/coelodonta_rhino_model/coelodonta_rhino`; wolf is `M_lupus_wolf/mlupus_wolf_model/lupus_wolf_model/lupus_wolf`. Every enabled rhino Defence/attack capsule transform belongs to the43-joint live skin. Wolf's body-pounce attack references transform1664068957102402253 outside its45-joint skin; export an auxiliary transform palette rather than substitute another bone. Its mouth attack and Defence parts can use the inspected live skin joints.

**The existing hare exporter cannot simply swap bundle/renderer IDs.** Rhino idle and all10 named attack clips have nonzero **streamed animation curves**; its run/walk clips are dense. All23 wolf clips use streamed curves, including idle, walk, run, mouth attack and pounce. `export_reference_actor.py` and `export_reference_player.py` explicitly reject streamed curves. Rhino also has4 material submeshes where the hare exporter requires one. The current `ReferenceAnimatorBake` is restricted to humanoid Avatars and verified player plans even when selecting another entity, so it is not an existing generic-creature sampler.

A faithful animation path therefore needs either a validated streamed-curve decoder plus controller/Avatar binding checks, or a **separate generic-creature clone sampler** that never drives live actors. Dense rhino run/walk recovery can validate mesh/skeleton/material plumbing, but cannot justify substituting those poses for idle/attacks. PEAC palette math, mesh validation and skeleton fingerprinting are reusable once generalized explicitly; multi-material geometry needs a preserved submesh/material mapping. `export_reference_hitboxes.py` provides reusable capsule/bind transforms but its hare-specific names, groups and root checks must remain intact for existing outputs. A new species descriptor/exporter is preferable to changing those constants globally.

Installed main collision capsules are enabled nontriggers with local **Z axis**, not vertical capsules:

| Species | Radius | Total height | Local center |
| --- | --- | --- | --- |
|Rhino|2.1400001|8.0|(0,2.1400001,0)|
|Wolf|.75999999|3.5899999|(0,.75999999,-.059999999)|

Their root/model/collider ancestor scales are1 in the inspected prefab. Actual spawned scale still applies at runtime. The existing upright hare movement query cannot represent these horizontal bodies exactly; rendering, ground support, collision, player contact and saved-spawn validation must agree on the eventual implementation.

Both species' sensors are mounted on their animated `Bip01 Head`. Rhino PEVision radius25, angle60, local axis(-1,-1,0), hearing15; wolf PEVision radius20, angle60, local axis(0,0,1), hearing20. Both hearing components have `isIgnoreBlock=false`; all inspected sensors are enabled. These differ from database visual/hearing summary columns. Preserve animated transforms and permissions instead of using a root-radius hostility substitute. Their AI XML includes `TowerDefence` behavior and their own movement/attack selection; the current hare's sole-player target and three-attacker adapter does not establish town/NPC combat parity.

## Fresh original manual8 anchor

A later original run reached the defense briefing and saved fresh manual slot8 (`save7`) during preparation. [Read-only archive evidence](../reference/town-defense-original-active.json) records active9077 with `STEP=0`, `TDMONS0=0_0_0_0_0`, target7507 serializer kind6, protected NPC20018 and `finallyPos=(-284,27,134)`. Quest giver/reply are also20018. All inspected archives were unchanged during decoding. This establishes a reference beacon center and Naoko binding, **not** a spawned cohort or saved prep clock. The existing bounded archive decoder already supported serializer kind6; no compatibility patch was required.

The20–80m spawn annulus around this center has outer X range[-364,-204] and Z range[54,214]. The installed route covers X[-640,-224) and Z[64,480), so the full annulus exceeds its east edge by20m and south edge by10m even before body/collision margins. A faithful general spawn path requires additional terrain/building capture. A captured fixed cohort could only be enabled after every observed anchor/body passes real geometry support; never shrink the annulus, clamp coordinates or reroll to fit. The source-only metadata header continues to mark cohort availability0.


## Original completion and fresh manual9

In the later `-force-gfx-direct` original run, the operator reports no player attacks after the9077 opening. The visible tracker progressed0→3→5→6→13→Mission Completed while the town fight continued. The [completion screenshot](../reference/town-defense-original-complete.png) and [fresh-save confirmation](../reference/town-defense-original-saved.png) document the bounded physical outcome. This corroborates source death credit that does not require the player to land the killing blow; it does not independently identify every killer or exact damage event. The operator did not acknowledge an ending conversation or interact with another NPC after completion.

[Read-only manual9/save8 evidence](../reference/town-defense-original-complete.json) records9077 completed exactly once, **no active missions** and an empty random-mission repository. Thus9078/9079 were not automatically accepted in this checkpoint, consistent with the source's gated chain unlocks and `NextQuest=0`. No defense actor IDs, per-wave sampling decisions or exact final kill count are reconstructed from the completed mission record. This inspection does not establish source NPC invulnerability, all defenses against attacks, or a future native completion path.

All seven files in each preserved manual5/save4, manual6/save5, manual7/save6 and manual8/save7 still match their previous bounded-reference hashes. The new completion archive was unchanged during inspection. The subsequent [preparation-reload observation](../reference/town-defense-original-reloaded.png) restored active defense at0/15 with two waves, followed by the first wave starting. See [PLAYTEST](../PLAYTEST.md) for its limited timing coverage; no partial-kill reload result is claimed. The graphics launch variation's successful run is separate from a demonstrated general crash fix.
