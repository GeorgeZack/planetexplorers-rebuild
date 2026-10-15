# Training enemy: mission 746

Audited 2026-10-03 from the supplied SQLite database, C# source, behaviour XML and serialized hare prefab. This is a replication contract and an implementation proposal, not evidence that combat has been implemented or measured in the original player. This audit used read-only asset inspection and no desktop input. Player weapon and damage work is tracked separately from this enemy audit.

## Mission and spawn contract

The authoritative records are in `Assets/Resources/DataBase/localData.bytes`:

| Record | Relevant values |
| --- | --- |
| `Quest_List` 746 | Target `1025`; `PreLimit=1:743`; `TalkOP=300229,4299`; `TalkED=300230:719`; `StoryInfo=1_352`; no item grant or reward |
| `Quest_KillMonster` 1025 | `MonsterID=13_1_1`, `Killer=0`, `TargetPos=0`, `ReceiveQuest=0`, description localization `3102200` |
| `Plot` 352 | Scene 3, delay 0, `CreateMons=261`, `MonsPatrolRange=261_1_30`, `NPCHatred=1_30000_2_261`, `MonsterHatred=1_2_261_4_30000` |
| `aispawn_fix` 261 | Position `(15,2,12)`; `pathid=13`; fixed point; rotation `(0,0,0)`; `produce=0`, `depend=1`, `refresh=-1`, `maxnum=-1`, `isgroup=0`; radius 80 |
| `Quest_List` 719 | Target `4066`; `TalkOP=4072,300231`; `TalkED=0:748`; `StoryInfo=1_432` |

`MissionRepository.LoadTypeMonster` (`Mission.cs:1467`) splits each monster specification into prototype list, required count, and target category. Thus `13_1_1` means **one death of monster prototype 13**, not three IDs and not fixed spawn ID 13. Target category 1 uses the monster death event; category 2 would use doodads. The source loader reads `m_mustByPlayer` from `TargetID == "1"`, rather than the `Killer` column; it is false for 1025. Do not add a player-last-hit restriction under the assumption that the column is enforced.

`StroyManager.cs:1279` activates fixed spawn 261. `SceneEntityCreatorArchiver.Init4FixedSpawnPoint` creates its agent at the database position with unit scale and database rotation, and copies `fixPosition` to `FixPos`. `MonsterEntityCreator.CreateMonster` explicitly excludes fixed-position agents from the normal `SpawnHeight=.5` adjustment. The authored initial position is therefore **(15,2,12), not (15,2.5,12)**; subsequent grounding belongs to physics. Radius 80 is not a random placement radius for this fixed-position spawn.

`SceneEntityMissionPointMonsterKill.Start/OnMonsterDeath` subscribes while the objective is active, increments the mission's matching prototype counter, and completes target 1025 once the required count is reached. It does not require the dead entity to have fixed spawn ID 261. Completion enters dialogue 300230 and then mission 719. A C implementation should emit one death transition per entity and avoid repeated increments, even though this source handler relies on the producer's death-event lifecycle.

`TrainingTaskManager.cs:269` only changes the current task enum to Fighting. **`FightingTask.cs` is not the active spawn/AI implementation.** Its `GetMonster` and `DestroyFightingScene` coroutines have no call sites in that class, and its death-checking update is commented out. The apparent two-second name lookup, CampID/DamageID reset to zero, one-second destruction delay and commented waypoint loop must not be reproduced as active tutorial rules.

## Prototype and effective attributes

Prototype 13 is `lepus hare`, localization 2370012, race Monster, identity Neutral. The view bundle is `Monster/m_lepus_hare.unity3d`; its source prefab is `Assets/Prefabs/AiPrefab/AiPrefabEntity/AiPrefabMonster/Monster/M_lepus_hare.prefab`; behaviour data is `Assets/Resources/AiPrefab/EntityData/Monster/M_lepus_hare.xml` (`MLepusHare` tree).

| Attribute | Database value / interpretation |
| --- | --- |
| HP and maximum HP | 212 each, before scale/difficulty |
| Attack / defense | 64 / 29, before scale/difficulty |
| Random scale | Uniform Unity `Random.Range(.8f,1.3f)` unless an explicit positive override is supplied |
| Walk / run / retreat speed | 2.5 / 6.5 / 2 m/s; sprint 0 |
| Rotation speed | 150 degrees/s |
| Visual / listening radius; visual angle | 18 / 10.8 m; 75 degrees (serialized sensors may differ) |
| Camp / Damage / DefaultPlayer IDs | 45 / 45 / 4 |
| Stamina | 100 maximum/current, recover speed 10, delay 3 |
| Rigid | 300 maximum/current, recovery speed 25 |
| Attack range / critical probability | 1 / 0; this range is not the behaviour-specific attack condition |
| Injured state / escape probability | .35 / .05; separate XML escape probability also exists |
| Corpse delete time / loot table | 30 s / 11 |
| Initial / death buff | 30200053 / `30200126,0,3` |
| HPRecover | .01, retained as data; this audit did not establish a complete passive-health-recovery schedule |

`PeEntityCreator.CreateMonster` (`Assets/PeWorld/PeEntityCreator.cs:1467`) chooses scale `s`, scales the spawned entity transform, and calls `DbAttr.ToAliveAttrWithScale`. That method scales **only maximum HP, current HP, attack and defense**: `212*s`, `212*s`, `64*s`, `29*s`. Normal difficulty ranges are HP 169.6–275.6, attack 51.2–83.2, defense 23.2–37.7. Movement speeds are explicitly excluded from this attribute scaling.

Easy difficulty additionally mounts buff 30200168, whose expressions halve maximum/current HP, attack and defense. Do not silently fix the tutorial enemy to raw 212 HP or assume a particular live scale/difficulty. A deterministic C fixture may select `s=1` and normal difficulty, but must identify that choice. The initial buff 30200053 recovers rigid by `RigidRecoverSpeed*.2` every .2 seconds; it is not a tutorial HP nerf. Buff 30200126 is an experience-related death effect, not combat invulnerability.

Plot 352 changes patrol mode to enum 1, `BHPatrolMode.SpawnCenter`, with both minimum and maximum patrol radius 30. `SpecialHatred.cs` resolves 30000 as all NPCs, 20000 as the main player, and fixed-spawn references separately from prototype IDs. The two plot clauses install ignore relations between the fixed hare and NPCs. They do **not** imply that the hare ignores the player or cannot retaliate.

## Attacks and collision

The XML describes multiple states, not a stationary target with one periodic attack:

| Behaviour | Source parameters |
| --- | --- |
| Melee | Weight .5; cooldown 1 s; gap range 0–.5; facing −10°…10°; skills 30100003, 30100004, 30100008, 30100046, 30100047 |
| Pounce | Weight .5; cooldown 5 s; gap range 2–5; facing −15°…15°; animation `attack5`; start .267, end .6, stop .967 s; skill 30100053 |
| Move around | Probability .08; range 5–25; duration 3–6 s |
| Back-jump stress reaction | Animation `move_afterjumpback`; cooldown 10 s; movement .6–.9 s at speed 8; total 1.633 s |
| Leisure patrol XML default | Probability .6; duration 10–20 s; radius 15–30; `spawnCenter=false`, overridden by the plot's patrol mode/radii |
| Leisure idle | Probability .25; 6–8 s, relax probability .8 |
| Escape XML | Cooldown/max time 30 s, max distance 200, interval 10, probability .1 |

`BTMelee` in `Assets/Scripts/Behave/Action/BTAttack.cs:474` requires a runnable skill, cooldown, range, facing, target HP range and an unobstructed path. Its cooldown check is strict `elapsed > cdTime`; selection chooses randomly among currently runnable skills. The XML's range compares `Enemy.SqrDistanceLogic`, **not center-to-center distance**. `TargetCmpt.cs:2215` finds points using both entities' local bounds and transforms, then uses their horizontal gap when the land entities overlap in height; otherwise it uses the three-dimensional gap. Angle checks have a special inside-target case. A simple fixed reach sphere cannot claim these checks are identical.

The six skills have additional database timing: 30100003/004/046 have 1 s main duration/cooldown; 30100008 has two trigger windows (prepare .36/.24, main .15/.24, ending .51; cooldown 1.5); 30100047 has 1.0666 s main/cooldown; pounce 30100053 has .967 s main/cooldown. Behaviour cooldown and skill cooldown are separate gates. Hit occurrence also depends on animated attack collision; elapsed-time damage alone is a substitute.

Triggers 10100, 10400 and 10250 require `colinfo`, set target rigid to −1, and compute `A*A/(D+A)` times shield and critical factors. They use forces 100, 400 and 250 respectively. Prototype 13's critical probability is zero. `TryGetHurt` applies the collision hit part's damage scale; its second argument `.1` is an experience value, not a hit delay or random factor. Player knife damage is covered by the companion combat audit.

The binary prefab was decoded with UnityPy 1.25.4. It contains 20 capsule colliders, two sphere colliders, 18 rigidbodies, 16 configurable joints and two skinned renderers; these include ragdoll and animation geometry and must not all become simultaneous solid world obstacles.

| Object / local path ID | Relevant serialized geometry |
| --- | --- |
| `collider` / CapsuleCollider 13617262 | Enabled, non-trigger, layer 8, vertical capsule radius .6, height 1.5, center `(0,.75,0)`; identity model-root transform |
| `Defencetrigger` / CapsuleCollider 13605086 | Enabled trigger, layer 25; Z-axis capsule radius 1.2, length 3.07, center `(0,.5,0)`; broad phase, not locomotion shape |
| `Defencetrigger` / MonoBehaviour 11491584 | 15 bone-relative defence capsules; `Head1` damage scale 2; Body1 and 13 limb/tail capsules scale 1 |
| `Bip01` / CapsuleCollider 13692568 | Enabled trigger attached to animated skeleton; not equivalent to the root capsule |
| Forearm colliders 13626506 / 13633044 | Trigger colliders initially disabled; attack components carry additional animated capsule definitions |
| Main Rigidbody 5413290 | Mass 200, no built-in gravity, angular constraints 112; locomotion motor supplies its own gravity |

The movement capsule's dimensions scale with entity scale `s`: radius `.6*s`, height `1.5*s`, center height `.75*s`. The model/root ancestors are identity at authoring time. The serialized motor includes gravity 10, maximum velocity change 1 and jump height 1, but several serialized speed/sensor defaults differ from database attributes and are overwritten or mediated by runtime components. These values are evidence, not a validated complete controller.

## Proposed bounded C integration and tests

Native enemy spawning, actual posed contacts, HP/death and the719 transition are implemented. [Training combat](TRAINING_COMBAT.md) and [training melee](TRAINING_MELEE.md) own integration; [PLAYTEST](../PLAYTEST.md#tutorial-native-graphical-fixtures) owns graphical results. Do not repeat the historical plan to create another enemy owner or substitute a synthetic kill. Source stat/event contracts above remain the reference; this does not qualify general Adventure AI.

## Reproduction anchors

- Database: `Assets/Resources/DataBase/localData.bytes`; SQL tables/IDs above can be queried directly with Python's `sqlite3`.
- Prefab SHA-256: `bfebf9c0d9c846ec1fc9a0764910fa14bb652fdbbcedcae204d1075b2973b1e5`.
- Behaviour XML SHA-256: `587284178f07dcf9ee14ecbba53f11441524f6366aa7d5e4d20500fdb7747118`.
- Source entry points: `Mission.cs:1467`, `SceneEntityMissionPointMonsterKill.cs`, `StroyManager.cs:1279` and `:1953`, `SceneEntityCreatorArchiver.cs:196`, `MonsterEntityCreator.cs:143`, `PeEntityCreator.cs:16` and `:1467`, `BTAttack.cs:474`, `TargetCmpt.cs:2120` and `:2215`, `SpecialHatred.cs`.
- Earlier lesson context: [training interaction audit](TRAINING_INTERACTION.md), [tutorial audit](TUTORIAL.md), and [static room collision audit](SCENE_COLLISION.md).


## Implemented melee event helper

`rebuild/src/pe_hare_attack.c` and `.h` implement the five melee animation windows and a bounded selection/cooldown contract. This is headless event state, **not a complete hare AI or a source of damage without a verified spatial contact**. It does not duplicate `PeHare` locomotion. Definitions come from `rebuild/build/reference-assets/hare-manifest.json`, recovered by the reference-asset exporter from bundle SHA-256 `94409ad1bf9570048a52c42671986907e2a5ca131f55f73737e5b7d7cff76c35`, AnimatorController path ID `4131577587453613724`.

| Attack / skill | Authored clip duration (s) | Normalized contact window | Bone |
| --- | --- | --- | --- |
| 0 / 30100003 | 1 | [.372, .642], inclusive | Right forearm |
| 1 / 30100004 | 1.000000954 | [.372, .642], inclusive | Left forearm |
| 2 / 30100008 | 1.500000477 | [.24, .34), [.5, .66) | Right, then left forearm |
| 3 / 30100046 | 1 | [.241, .631], inclusive | Right forearm |
| 4 / 30100047 | 1.066667080 | [.419, .7], inclusive | Both forearms |

The C table retains the serialized float values exactly rather than the rounded display values above. `pe_hare_attack_bone` exposes the forearm names, transform path IDs, skeleton palette indices (right 35, left 13), and path hashes so a spatial-animation host can resolve the correct bones. This table does not itself pose or retarget the skeleton.

`BTMelee` (`BTAttack.cs:474–710`) assigns weight .5 to the **whole melee behavior**, then chooses uniformly among runnable skills. It does not assign five separate .5 probabilities. `pe_hare_attack_candidates` takes the host's geometry-eligibility result and runnable-skill mask; `pe_hare_attack_begin` takes a uniform rank among that candidate set. Source geometry includes bounds-gap distance, facing, target HP, movement compatibility and obstruction. The helper intentionally does not infer perception or these checks from a center-distance approximation. Branch arbitration against pounce or other behaviors remains the host's responsibility.

Cooldown time is absolute monotonic host wall time, using the source `Time.time` epoch. The behavior requires strictly more than one second since its previous start (initial source value zero). Own skill cooldowns are 1, 1, 1.5, 1 and 1.0666 seconds; the shared type-1 gate is one second. Additional runtime skill gates belong in the supplied runnable mask. Ending an animation never resets these cooldowns.

**Animation time is a separate clock.** `pe_hare_attack_update` consumes the Animator's already evaluated normalized time, not elapsed wall seconds. It returns newly opened/closed windows, active forearm bits, and `MonsterEndAttack` events. `PEAnimatorAttackTrigger.cs` includes both endpoints. `PEAnimatorAttackTriggerEX.cs` tests `>= end` before opening: each of attack 2's intervals closes at its upper endpoint and emits its end event once, including when an update skips the entire interval. A skipped interval never briefly opens a fictitious hit window. The caller must report actual state exit through `pe_hare_attack_end`; normalized time reaching one is not evidence of a state exit or skill completion.

`pe_hare_attack_record_contact` commits a nonzero target-entity ID only in an open window and deduplicates per actual skill guide, following `SkInst.cs:297–309` and its guide reset at line 377. The host supplies guide index 0 or 1 independently of animation-window index: attack 2's two normalized windows are not a substitute for its separate wall-time skill-guide schedule. Both forearms share the same target list when they belong to one guide. Call only after spatial contact, damage permission and successful skill-event execution have been established. The bounded list holds 64 target entities per guide and rejects overflow without damage; this is an explicit native capacity limit.

`tests/test_hare_attack.c` covers uniform candidate ranks, sparse masks, initial/strict/own/shared cooldown boundaries, every inclusive/exclusive hit endpoint, skipped EX windows, one-shot end events, repeated targets across both guides, target capacity, explicit state exit, and atomic rejection of invalid/nonfinite/backward time. The helper and player-combat suite passed a strict C11 `-Werror` build with AddressSanitizer, UndefinedBehaviorSanitizer and leak checking. No live combat or graphics parity is implied by these headless tests.
