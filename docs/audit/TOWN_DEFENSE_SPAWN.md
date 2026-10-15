# Disconnected Town Defense spawn planner and member owner

October7,2026, US/Central. Source-derived and implemented in C. No game, graphics, desktop input or new capture was used. [The progression audit](SOURCE_FIRST_REBUILD.md) identifies this offline slice; [Town Defense evidence](ADVENTURE_TOWN_DEFENSE.md) separately records the original observations, archived center/protected NPC and unqualified dependencies.

[pe_town_defense_spawn](../../rebuild/src/pe_town_defense_spawn.h) is disconnected from `PeGame`, quest acceptance, saved state and graphics. The existing clock still emits two wave requests only. The new planner can consume each request and retain a plan; neither a finished clock nor an admitted plan establishes that an actor exists or that9077 is complete. The generated cohort-availability flag remains0.

## Source spawning and draw ownership

`EntityMonsterBeacon.CreateMonsterBeaconByTDID` uses the target's town position and identity rotation, so active9077 starts with forward+Z. `RefreshTowerMission` processes groups in database order: an integer count draw, then an angle draw and radius draw for each member, then the next group's count. Wave0 creates8–9 hare13 requests followed by2 rhino4 requests; wave1 creates8–9 wolf7 requests followed by2 rhino4 requests. `Random.Range`'s integer maximum is exclusive. The planner's injected integer callback receives those exact bounds and invalid returned values fail closed.

The active six-argument `AiUtil.GetRandomPosition` flattens forward Y, rotates around positive Y, normalizes, and multiplies by a uniformly drawn radius20–80. Positive90° sends+Z toward+X. Float endpoints are included. It does not use the adjacent `insideUnitCircle` overload or an area-uniform square-root radius. The source subsequently replaces Y with `SceneEntityPosAgent.PosYTBD=0`; native plans retain that unresolved Y and forbid riding. General zero/near-zero flattened forward input is rejected as a bounded native precondition; the qualified9077 orientation is nonzero. Native trigonometric intermediates use double precision, so this is a numerical port rather than a claim of bit-identical Unity coordinates.

Draw callbacks belong to the caller. Invalid input or a failed draw leaves the output plan unchanged, but does not rewind arbitrary callback state. A future host must stage its RNG if it needs that rollback. Once successfully planned, support retries must reuse the plan without drawing again. The planner does not draw prototype scale, instantiate actors or claim a Unity RNG sequence. Source asynchronous creation can make additional choices later; those are outside this module.

No route bounds are silently imposed. A request at source(-204,0,134),80m east of the archived center, remains there even though the installed terrain ends at X−224. The full annulus exceeds the current capture; clipping or rerolling it would alter the source distribution.

## Pending agents, created members and death credit

A transient owner has an explicit beacon identity, source center/forward, sequentially admitted waves and at most22 records. Native agent IDs1–22 remain stable within that beacon and are distinct from actual created entity IDs. Admitting a validated wave stages all its pending records atomically. The source adds scene agents sequentially and can instantiate only some later; atomic native admission is a documented allocation adapter, not synchronous actor creation.

`SceneEntityPosAgent.TryCreateEntityUnstatic` attempts support before creating the monster. The new owner models that separation without porting its100-attempt/.05s coroutine or multi-hit height selection. Failed support leaves the same pending request unchanged. The future caller supplies qualified support and the actual living actor/death capability; X/Z must equal the retained request, while supported Y may differ. Wrong species, duplicate entity IDs, beacon-ID aliases and rebinding are rejected. The source `OnMonsterCreated` death subscription also assigns DefaultPlayerID8 and CampID26; those permissions appear only after successful native binding. This module never overwrites DamageID.

The second wave may be admitted while every first-wave request is still pending or alive. Only an actually created entity registered to the supplied beacon can receive a death credit. Killing actor identity is irrelevant. Duplicate notifications, unrelated entities and pending requests cannot increment the counter. At exactly15 credits the owner latches completion and retains surviving created and pending records. Later notifications are rejected; stable-ID deduplication and this latched terminal state are native event-safety adapters.

That latch does not complete a gameplay quest, grant a reward or remove any actor. Source survivor escape/delayed removal, protected-Naoko failure, allied NPC combat and persistence remain future host responsibilities. The created records' positions are supported spawn positions, not a second movement owner.

## Guards and verification

[The exporter](../../rebuild/tools/export_adventure_town_defense.py) still emits the same quest/schedule/species metadata and adds radius, deferred-Y and permission constants. Six whitespace-normalized method-body guards pin active beacon initialization, wave construction, the correct AiUtil overload, creation/death binding, target death credit and deferred creation. Negative tests change orientation, radius, permissions, death subscription, flattening, death arithmetic, deferred Y and support admission and require rejection. Regenerate outputs through the exporter; do not hand-edit the generated files.

One initial fixture assertion incorrectly expected a50m south sample to leave the captured Z range; its computed Z84 was correctly inside. The fixture now explicitly injects90°/80m and verifies unchanged X−204. That development assertion failure is not a game crash or a passing test; the corrected optimized and sanitizer runs passed. Subsequent assert-based test launches disable core dumps only for that shell process to avoid desktop crash notifications.

```sh
python3 rebuild/tools/export_adventure_town_defense.py --check
python3 rebuild/tests/test_adventure_town_defense_export.py
make -C rebuild town-defense-spawn-test
```

Playable9077 stays gated on qualified rhino/wolf poses, horizontal collision bodies and skills, source-backed town/NPC combat and protected-target policy, full annulus geometry, cleanup and an explicit reload contract. A recorded random cohort is not needed to recover this planner; it could qualify one bounded session later without authorizing a changed general distribution.
