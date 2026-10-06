# Next bounded sandbox combat increment

Audit date: October6,2026. This is a source/installed-asset audit, not a new original or native playtest. This audit initially changed no runtime behavior or recovered assets. The subsequent isolated targeting helper below remains disabled in gameplay. Actual native four-kill completion and partial-combat-progress reload still need physical verification; staged return-dialogue checks do not establish either.

The next substantial player-combat milestone should be **grounded running Knife33 attacks**, beginning with the female `RunSwordAttack` first stage and the existing Attack7–9 continuation. The matching poses are now recovered and staged; root-motion qualification and gameplay integration remain required. Simply removing the stationary gate and continuing ordinary WASD movement would give the wrong source skill, trajectory and collision geometry.

For a smaller increment using the already recovered poses, source attack target acquisition/heading and attack-capsule history are better candidates than fabricated stagger. Either may improve reliable contacts, but neither guarantees four kills. The new ordinary Med Pack use should also be physically exercised before attributing survival difficulty entirely to attack restrictions. Do not adjust hare HP, weapon damage or enemy counts to obtain the milestone.

## What is available now

The installed native asset manifests were inspected directly. This availability table was refreshed after the October6 [directional asset installation](DIRECTIONAL_PLAYER.md); the original audit and proposed combat boundaries below retain their dated scope:

| Asset group | Available poses | Relevant missing data |
|---|---|---|
| `player/recording.peas` |20 clips: twelve locomotion leaves including side/back walk/run, four turn alternatives, female TP Attack6–9 | RunSwordAttack, SprintSwordAttack, FP attacks; authoritative root displacement |
| `player-knife-drawn/recording.peas` | Matching twenty body poses on the real mountMain attachment | Running/sprint/FP attack poses and root trajectory |
| `player-defence` | All29 current locomotion/turn, TP knife and tool poses on the compatible23-joint actor | Running/sprint/FP attack defence poses |
| Hare actor | idle/run/walk and attack0–4 | Exported pounce pose, pounce host; no hurt/death clip exists in its inspected controller |

Native directional A/D/S run/walk input now exercises all six new clips (mask4037), with a drawn knife and exit0. Full15,000-frame tutorial progression reaches756 and full ASan/UBSan with leak detection passes. These are separate from running attacks: the directional test performed zero attack stages/contacts, and the tutorial mask63 covers only the original six locomotion states. See [PLAYTEST](../PLAYTEST.md). A separate captured-camera tutorial run also exercises the six side/back selections (mask4033) with exit0. The exact source RunSwordAttack408 now has a [validated staged body/knife/defence group](RUNNING_PLAYER_POSES.md), which is not installed. Authoritative root displacement and gameplay integration remain prerequisites, so running attacks remain unavailable.

The body manifest explicitly limits root data to diagnostics. [ReferenceAnimatorBake.cs](../../rebuild/tools/ReferenceAnimatorBake.cs) sets `applyRootMotion=false`, detaches the Animator controller, seeks a paused playable and calls `Animator.Update(0)`. Its `rootPosition`/`deltaPosition` fields are not validated accumulated gameplay movement. Adding a clip name to this sampler is sufficient only to begin pose recovery, not to establish a motion trajectory. Existing PEAC palettes remove the model root; translating only the visible mesh would leave player collision, defence and weapon contact inconsistent.

## Grounded moving attacks: exact source boundary

[Action_Sword](../../Assets/Scripts/PeEntity/Cmpt/Motion/PEAction/Action_Sword.cs) selects air first, water second, then compares the magnitude of actual movement velocity against serialized move/sprint thresholds. It does not inspect the sprint key or reject all nonzero velocity. [Motion_Equip.SwordAttack](../../Assets/Scripts/PeEntity/Cmpt/Motion/Motion_Equip.cs) requires the active sword to match the held hand-change equipment and no two-hand weapon. Its main-player initialization enables stamina spending even though the prefab's serialized flag is initially false.

Read-only UnityPy decoding of [EntityPlayer.prefab](../../Assets/Scripts/PeEntity/Resources/EntityPlayer.prefab), component path11400022, found these overrides:

| Property | Serialized value | C# field default, not authoritative here |
|---|---:|---:|
| Move attack threshold |4.5m/s, strict greater-than|4|
| Sprint attack threshold |7m/s, strict greater-than|6|
| Rotation interpolation factor |60|3|
| Attack target half-angle |115°|20°|
| Target query radius / height |3m /1.5m|3 /1.5|
| Target layer mask |33554432, layer25|unset|
| Hit-response animation scale time |0.649999976s|.15s|

Source prefab SHA256: `7119a434d3e1527cca656614ab347f87593913510a5302bef117a2e7834e4d3f`. These are serialized source facts, not a measurement of the current installed player's component. Capture or independently decode its installed equivalent before declaring live parity. At those thresholds, speed4.5 remains stationary skill, speed7 remains moving skill, and only speed>7 selects sprint. Air/water remain separate unsupported branches.

Knife33's source prefab selects stationary20110011, running20110014 and sprint20110013. The database gives running effect42 and sprint effect41, each followed by attack7, attack8, attack9; both use trigger237 for their guides. Their own cooldown is.2s, shared group1 time0. The repeated six-second waits are interrupted by animation callbacks and are not swing durations. Running is interruptible (`_interruptable=1`); sprint is not (`0`). Preserve that distinction when widening cancellation behavior.

The existing installed [animation inventory](PLAYER_ANIMATION.md) pins controller779 and these first-stage timings. Times are authored clip seconds, not wall time:

| Female stage / clip ID | Clip duration | Hit opens | Combo opens | Hit ends | Combo ends | EndAction |
|---|---:|---:|---:|---:|---:|---:|
| RunSwordAttack /408 |1.833339691|.296858042|.420243800|.709120452|.787268519|1.168072581|
| SprintSwordAttack /401 |1.800006866|.168534517|.350311279|.567953229|.716360569|1.192403674|

Male overrides are315 and310 respectively; they have different event times and must not reuse this female table. These moving states have a single leaf, state speed1 and leaf duration1, unlike the stationary FirstPerson blend. Runtime clip-rate verification is still required. Current stamina rules remain five per start/combo, base speed1.37, ×.9 at each reached50/20 threshold, clamped.5–1.5; the recovered hit-response curve additionally changes speed after real contact. EndAttack waits at least two engine frames after opening, and EndAction uses the existing.3s stale-event guard.

During an active skill, `UpdateSkillState` requests horizontal `anim.m_LastMove / Time.deltaTime`. `AnimatorCtrl.OnAnimatorMove` supplies that delta. If the difference from current horizontal physics velocity has magnitude≥12, the requested velocity is limited to current velocity plus a length12 difference. This is a per-update velocity correction, not acceleration12m/s². Normal walking velocity must not be added on top. Root translation must pass through the same terrain/tree/building collision used by normal movement, with the resulting position used by every contact and renderer.

Minimal implementation sequence:

1. Confirm installed4.5/7 thresholds; capture a normal running attack with actual skill ID, Animator state/time and model/physics displacement, without altering the reference save or combat stats.
2. Extend the bounded sampler allowlist for female408; recover matching body, mountMain knife and23-joint defence poses. Recover and independently validate its root displacement under sequential evaluation; do not reinterpret zero-time seek diagnostics as velocity.
3. Add a running first-stage variant to the single melee clock, followed by existing Attack7–9. Export exact events rather than hand-edit generated assets. Keep stationary/air/water/sprint admission explicit; unsupported sprint must fail before stamina debit.
4. Route root displacement through shared geometry once per simulation step. Use the same authoritative stage/time, position and yaw for body, blade and incoming defence. Reset transient history on cancellation, equipment change, load and guide changes.
5. Test threshold equality, actual velocity versus input requests, unsupported branches, wall/tree stopping, combo continuation, stamina debit, multi-target dedupe, death and modal ownership. Physically verify a running contact and ordinary four-kill/partial-reload progression separately.

## Smaller asset-ready candidate: source targeting and contact history

`Action_Sword.ChangeAttackTarget` runs at attack and combo start. It queries the attack sphere, excludes self/ragdolls/nonconflicting factions, and considers posed defence geometry within the source horizontal cone. `PEDefenceTrigger.GetClosest` has special centre-bone priority and inside-capsule behavior; it is not simply nearest entity centre. The defence-trigger branch does not update the outer `minDis` value, so later qualifying colliders can replace earlier ones. A native stable actor-order choice would be an explicit ordering adapter, not recovered Unity overlap ordering.

While the skill runs, the third-person branch sets the character heading toward the selected direction with vertical look removed on land. The current native camera yaw also supplies character yaw; implementing this faithfully enough requires a separate transient attack-facing value. Otherwise target assistance would unexpectedly turn the camera. Body, knife and compatible defence must share that facing. Preserve camera/input yaw independently. Full source pitch aiming uses procedural IK, which the baked poses do not contain; heading-only assistance must state that limit. There is no new line-of-sight ray in this particular source acquisition routine; do not quietly substitute the hare perception mask.

[PECapsuleTrigger.CheckCollision](../../Assets/Scripts/Attack/PECapsuleTrigger.cs) tests intermediate attack endpoints from previous to current pose, with parent-centre radial correction. Sampling count is `max(floor(.5*endpointTravel/radius)+1)`; its count1 case uses the previous endpoint sample, so a generic continuous sweep is not identical. [PEAttackTrigger](../../Assets/Scripts/Attack/PEAttackTrigger.cs) supplies entity centre and preserves successful hit bookkeeping. Native [pe_hitbox](../../rebuild/src/pe_hitbox.h) currently tests only instantaneous capsules. A bounded history port can use existing real poses, preserve source sample rules and guide dedupe, and test a target crossed between samples without increasing damage or reach. Activation/reset ordering and source centre placement must be recovered before enabling it; stale samples after load/teleport must never produce a hit.

## Why the alternatives are not immediate substitutes

**Hit stagger:** knife trigger493/502/503 carries force140;237 carries150. Damage also writes rigid attribute29 to−1. Neither means “pause every hare for a fixed duration.” [Motion_Beat](../../Assets/Scripts/PeEntity/Cmpt/Motion/Motion_Beat.cs) selects null/Whacked/Repulsed/Wentfly/Knocked from angle-scaled force and target thresholds, with separate response curves and action masks. `Action_Whacked` expects BeHit and HitAnimEnd, but the inspected hare controller contains no hurt/death clips. Recover its actual BeatParam, thresholds, rigid/force lifecycle and motor response first. A fabricated flinch clip or unconditional stun would change combat balance without source support. Player hit-stop is separately recoverable from the serialized.65s curve; it slows player animation and is not evidence for a hare stun.

**First person:** the stationary event tables already exist in `pe_combat`, and observed female stationary FP rate is.65×Animator speed. Female FP clips499/504/509/515 are present in the installed animation inventory, but not in the installed native body/knife/defence groups. All three matching pose groups and first-person presentation/aiming must be recovered; applying FP callbacks to TP poses is invalid. This is a smaller pose-recovery task than moving attacks, but it leaves the movement restriction and therefore is not the first choice for combat mobility.

**Hare pounce:** its installed clip `attack5_pouncefront`, path1506232535475238862, duration.966667175s, is present and reports zero average speed. It is not exported in the current hare actor. [M_lepus_hare.xml](../../Assets/Resources/AiPrefab/EntityData/Monster/M_lepus_hare.xml) pins skill30100053, bounds gap2–5m, angle±15°, probability.5, cooldown5, start.267/end.6/stop.967. [BTPounce](../../Assets/Scripts/Behave/Action/BTAttack.cs) disables/restores gravity and drives explicit movement speed `(horizontalDistance+1)/(.6-.267)` during the travel interval. Zero clip average speed therefore does not mean stationary pounce. Export pose/windows, skill contacts and the complete movement/reset lifecycle before integration. This would improve enemy parity and could make the fight harder; it is not a player-survival workaround.

The concrete near-term choice is source-qualified heading/contact reliability with present assets, or begin the larger running-attack recovery. Preserve current evidence boundaries until an ordinary input-driven four-kill run and partial-progress reload are actually observed.


## Subsequent bounded helper — implemented, host disabled

[pe_sword_targeting](../../rebuild/src/pe_sword_targeting.h) now implements the ordered third-person source selection with the serialized3m/1.5m/115°/layer25 query settings. Callers supply actual query overlap, source collider order, view identity, faction/ragdoll status and already posed defence capsules. It preserves first-view deduplication, centre-bone and inside-capsule priority, inclusive3m capsule distance, strict horizontal angle admission, strict non-defence centre ranking, and the defence branch’s lack of an outer distance update. An inside-capsule selection can produce zero direction and inactive aim, as in the source. Invalid input leaves output unchanged. Query construction and direction selection are pure; no pose, rotation, damage, permission or IK is manufactured.

[test_sword_targeting.c](../../rebuild/tests/test_sword_targeting.c) verifies order reversal, mixed defence/non-defence ranking, duplicate views, part ties/centre priority, a target inside a capsule, capsule range boundaries, both sides of the115° cone, finite-segment projection, permission/layer/overlap filtering and atomic malformed-input rejection. Strict optimized and ASan/UBSan runs with leak detection pass. The helper is registered in the core test suite, but **no gameplay host calls it**. Unity overlap enumeration remains unobserved, and separate attack facing has not been integrated. The stationary Knife gate, existing camera/body yaw, current damage and rendering remain unchanged. These tests do not establish automatic aiming or a physical combat improvement.
