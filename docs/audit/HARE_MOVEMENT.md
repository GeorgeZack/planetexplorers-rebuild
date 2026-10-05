# Native training hare movement

`rebuild/src/pe_hare.c` and `pe_hare.h` provide a headless, deterministic movement subset for the tutorial's prototype-13 lepus hare. They use the recovered static room collision mesh and preserve verified source constants. They do not implement attacks, damage, death, kill counts or mission completion. The initial training integration is **leisure patrol only**, with explicit scale 1 and no accepted hostile target. Perception is not implemented; a nearby player does not automatically become an enemy in this integration.

The enemy's original mission, prefab, stats and combat data are recorded in [TRAINING_ENEMY.md](TRAINING_ENEMY.md). This document describes the native implementation and its practical limits. No new graphics run or original-game input was performed for this audit; graphics evidence belongs in [PLAYTEST.md](../PLAYTEST.md).

## API and state

| Interface | Contract |
| --- | --- |
| `pe_hare_init(hare, scene, seed, scale, easy)` | Creates a temporary state, verifies the authored spawn and a floor beneath the scaled capsule, and publishes only on success. A failed call leaves the destination unchanged. |
| `pe_hare_step(hare, scene, target)` | Advances one fixed 1/60-second step without allocation. A null or unaccepted target selects leisure behaviour. Invalid arguments/state return false without publishing a change. |
| `pe_hare_engagement(hare, scene, target)` | Reports a target's bounds gap, facing angle, static line of sight, and melee/pounce geometric eligibility. This is not an attack or damage authorization. |
| `PeHareTarget` | Caller-supplied accepted-target flag, position, velocity, radius, height, yaw, and desired melee/pounce approach mode. Hostility, reputation and perception selection belong to the caller. |

`PeHare` contains the source spawn, current position/velocity/yaw, scale/capsule dimensions, maximum HP/attack/defense values, patrol goal, leisure timers, mode and deterministic RNG state. Its modes are idle, patrol, approach, retreat and hold. It has **no current-health field or death state**. A future combat system must own actual health and publish validated target/death events separately.

Source coordinates use +Y up and yaw 0 facing +Z. The authored spawn remains `(15,2,12)` in `hare.spawn`; initialization settles `hare.position` onto the mesh with collision skin. Fixed-spawn creation does not add the monster prototype's .5 m general spawn-height offset. Tiny horizontal adjustment during floor contact is possible because contact skin follows the actual triangle normal.

A scale argument of zero samples `.8 + .5*u` using a seeded native xorshift64* stream; an explicit scale must lie in [.8,1.3]. Seed zero maps to a fixed nonzero state. This supplies deterministic replay of native decisions, **not Unity RNG equivalence**. The source samples the same scale range, but its global random stream and sequence are different. Explicit scale 1 is a reproducible integration fixture, not proof that the observed original hare spawned with that scale.

## Verified source behaviour retained

| Rule | Native handling and source |
| --- | --- |
| Model/collision scale | Radius `.6*s`, height `1.5*s`; original root CapsuleCollider 13617262, center height `.75*s` |
| Combat attributes | Maximum HP `212*s`, attack `64*s`, defense `29*s`; Easy halves these, following `PeEntityCreator.CreateMonster` and `DbAttr.ToAliveAttrWithScale` / buff 30200168 |
| Walk/run/retreat | 2.5 / 6.5 / 2 m/s; prototype 13. Attribute scale does not multiply these speeds. |
| Turning | Maximum 150 degrees/s, prototype 13; native yaw follows the shortest angular difference |
| Patrol center/radius | Authored spawn, radius 30. Plot 352 overrides the XML radii through `BHPatrolMode.SpawnCenter`, `MinPatrolRadius`, `MaxPatrolRadius`. |
| Patrol attempt and duration | Acceptance test at probability .6; accepted patrol lasts 10–20 s, from `M_lepus_hare.xml` / `BTPatrol.Init` |
| Idle duration | 6–8 s from hare XML; the native scheduler using it is provisional |
| Retargeting | Arrival at less than 2 m or obstruction for more than 2 s selects a new goal; more than 5 s without movement abandons the patrol attempt |
| Approach | Run toward target plus one meter in normalized target-velocity direction when outside selected attack range; `BTMoveToTarget.Tick` |
| Retreat/hold | Retreat at 2 m/s when below the selected range; otherwise stop translation and face the target |
| Melee geometry | Bounds gap 0–.5 m and facing within ±10 degrees |
| Pounce geometry | Bounds gap 2–5 m and facing within ±15 degrees; only a query/approach mode, no simulated pounce |

Source references: `Assets/Scripts/Behave/Action/BTIdle.cs:410` (`BTPatrol`), `BTTarget.cs:599` (`BTMoveToTarget`), `BTAttack.cs:474` (`BTMelee`), `Assets/Scripts/PeEntity/Cmpt/TargetCmpt.cs:2120` and `:2215` (gap computation), and the hare behaviour XML referenced in the enemy audit.

The native destination sampler mirrors the source's spawn-centered directional attempts: forward −75°…75° candidates, a second angular candidate range corresponding to the source's `Random.Range(135,-135)` arguments, and a final unprojected forward candidate. The source nests five calls with up to five ground attempts each before fallback. Native ground projection checks for a roof above the candidate and then a floor below, in the spirit of `PEUtil.GetCorrectHeight`; it checks the resulting capsule for clearance. All candidate X/Z radii remain 30, including a fallback outside the room. A fallback is a movement goal, never a teleport.

The native engagement query follows the source's two closest-bound-point calculation. It projects the gap horizontally when the source-style vertical overlap condition holds; otherwise it uses the 3D gap. It uses yaw-rotated **capsule bounding boxes as a proxy** for the entities' original bounds. Consequently the melee .5 m threshold is not a .5 m center distance. At scale 1, a same-height player proxy of radius .3 directly in front gives .5 m gap at 1.4 m center separation.

## Selection evidence and intentional exclusions

The modern target system must not be confused with the older AI tables. `TargetCmpt.ThreatData.LoadData` reads **`initthreat`**, where hare camp 45 toward player camp 1 has value **5**. `DamageData.LoadData` reads **`isaddhatred`**, whose corresponding value is **1**. Looking up row 45 in the older `ai_campnew` gives an unrelated unused-slot row and would incorrectly suggest no player hostility. `TargetCmpt.CanSelectEntity` also checks reputation, force conflict, existing enemies and target validity. Plot 352's special relations exclude NPC involvement, not the player.

Target collection comes from instantiated `PEVision` and `PEHearing` components. The source prefab's animated head sensors have hearing radius 15 and vision radius 20/angle 60; these differ from the database's 10.8/18/75 values. Their bone direction, occlusion rules and surrounding target-selection state are not reproduced by inventing a simple proximity radius. Therefore the module accepts an already-selected target as input, and the initial training integration supplies none.

`pe_hare_engagement` does not evaluate cooldowns, skill availability, injury/escape, shields, faction permissions, animation contact or damage. Its flags must not be used to subtract HP by themselves. The pounce flag does not produce a leap. The player/hare systems still need authoritative animated capsule contact and the source combat/skill lifecycle before allowing mission 746 to complete.

## Controller approximations

The collision backend is the native BVH described in [SCENE_COLLISION.md](SCENE_COLLISION.md). It sweeps the scaled hare capsule against the recovered room, removes motion into contact normals, adds .003 m contact skin, and snaps short distances onto slopes with normal Y at least .65. Gravity 10 follows the serialized hare motor value, but the resulting motion is a new controller. Instant directional velocity, 60 Hz stepping, five slide iterations, slope threshold, probe lengths, contact skin and lack of a step-up solver are native implementation decisions. Rotation is rate-limited; translation can change direction independently of facing.

A grounded animal refuses motion when its proposed center has no nearby supporting floor. This conservative ledge guard is necessary because no navigation mesh is available and the authored 30 m patrol ring often extends beyond the finite training deck. It is not a claim about original Unity navigation. The current module cannot plan around obstacles; repeated invalid or blocked destinations can leave the hare idle or moving against a wall until a retry. It does not implement A*, polar-shield avoidance, water/layer masks, flock following or dynamic character avoidance.

The compiled `MLepusHare` behaviour-tree topology is not ported. The native leisure scheduler starts with a 6–8 s idle interval, then chooses a patrol attempt with probability .6 or another idle interval. Source action-local constants and guards do not establish that the original compiled tree follows this exact scheduling order. Headless equality between runs therefore establishes repeatability of the native approximation, not original AI parity.

Line of sight is a static ray between capsule midpoints, not an animated head/sensor or weapon ray. Actor skinning and clip playback are separate presentation systems. A walk animation should accompany 2.5 m/s patrol; playing a run cycle instead is not equivalent merely because the model moves at the correct speed. Root translation comes from `PeHare`, while the actor palette is evaluated in model-local space.

## Validation

The independent `tests/test_hare.c` suite passed an optimized C11 build with `-Wall -Wextra -Wpedantic -Werror`, and AddressSanitizer/UndefinedBehaviorSanitizer with leak detection. It covers:

- Authored spawn, seeded scale equality, scaled/Easy stats, and transactional rejection of invalid scale or input.
- One-second 2.5 m patrol displacement, 6.5 m/s approach, 2 m/s retreat, and a 2.5-degree turn per 60 Hz step.
- Bounds-gap versus center-distance distinction, facing rejection, vertical separation, pounce hold/retreat and static obstruction.
- Identical complete native state over 1,800 leisure steps for identical seeds, radius-30 destinations and collision-free output.
- Repeated wall approach without penetration, and patrol toward a goal beyond a finite floor without falling off its edge.
- Optional recovered-room loading, source hare spawn settling, and 900 leisure steps without capsule penetration.

These tests do not open a graphics context. They do not establish source attack timing, successful enemy combat, player damage, mission 746 completion or visually correct animation. Current training integration is limited to leisure motion with a fixed-scale source actor; its rendering and interactive evidence should be checked separately.
