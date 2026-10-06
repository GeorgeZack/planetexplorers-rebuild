# Melee capsule history: source lifecycle

Source audit: 2026-10-06. The findings below come from local C# source and existing exported hare-controller provenance. No new original-game launch, desktop input or runtime modification was performed. The source findings and the subsequent native implementation/verification are recorded separately below.

## Three distinct kinds of state

The source separates capsule endpoint history, attack-trigger candidate/deduplication lists, and skill-guide hit lists. Resetting one does not reset the others.

[PECapsuleTrigger.cs:53](../../Assets/Scripts/Attack/PECapsuleTrigger.cs#L53) `ResetInfo` computes local endpoint offsets from radius, height, axis and offset. It **does not clear or seed** current endpoints, previous endpoints or movement vectors. These private vectors begin at their default zero values on a new capsule object. `Update(centerPos)` copies current endpoints into previous endpoints, transforms the local offsets to current world points, calculates the two displacements and stores the supplied center ([line82](../../Assets/Scripts/Attack/PECapsuleTrigger.cs#L82)). Radius scales by the absolute X component of the transform's lossy scale.

[PEAttackTrigger.cs:78](../../Assets/Scripts/Attack/PEAttackTrigger.cs#L78) `ClearHitInfo` clears candidate defence triggers, already effected defence triggers and direct-hit entities. `ResetHitInfo` moves effected triggers back into candidates and clears effected/direct-hit lists. Neither touches capsule history.

[SkInst.cs:377](../../Assets/Scripts/SkillSystem/SkInst.cs#L377) clears `_hitInGuide` when each skill guide enters its collision-enabled main phase. The following collision callback requires `_bExecExtEnable` and an as-yet-unhit eligible target ([line297](../../Assets/Scripts/SkillSystem/SkInst.cs#L297)). That guide boundary likewise does not reset a physical capsule's endpoint history.

## Trigger activation and frame order

`PEAttackTrigger.Start` initializes local offsets, caches the parent `SkEntity` and its `PeEntity`, then sets `active=false` ([PEAttackTrigger.cs:99](../../Assets/Scripts/Attack/PEAttackTrigger.cs#L99)). Its active flag initially defaults true, so this first assignment disables the collider. The accompanying `UpdatePartsInfo` returns immediately because the new state is inactive; it does not seed endpoints.

The active setter ([line58](../../Assets/Scripts/Attack/PEAttackTrigger.cs#L58)) returns if the requested value is unchanged. Otherwise it changes the flag, toggles the broadphase collider and calls `UpdatePartsInfo`:

- Inactive→active immediately advances capsule history using the pose at activation.
- Active→inactive does not advance history, because the update sees inactive state.
- Reassigning active=true while already active performs no additional pose update.

Every `LateUpdate` calls `UpdatePartsInfo`, then `UpdateHitState` ([line110](../../Assets/Scripts/Attack/PEAttackTrigger.cs#L110)). Inactive attack parts **do not update** ([line185](../../Assets/Scripts/Attack/PEAttackTrigger.cs#L185)); their endpoints remain at the last active sample. There is no `OnEnable`/`OnDisable` endpoint reset in this class or in `PECapsuleTrigger`.

Normal activation followed by LateUpdate therefore produces two updates. The first can have zero previous endpoints on a newly created trigger, or old endpoints from its preceding active window. The second has activation-pose endpoints as its previous values. The late collision check consequently uses activation→late-pose history, not necessarily the zero-origin or old-window interval. However `OnTriggerEnter` can also call `CheckHit` directly ([line137](../../Assets/Scripts/Attack/PEAttackTrigger.cs#L137)). This audit does not establish a universal PhysX callback ordering relative to animation activation and LateUpdate. Do not claim that the first interval can never be queried, or that every window should explicitly seed both endpoints equal.

## The supplied center is an entity bounds center

The parameter called parent center in `PECapsuleTrigger` is **not** `capsule.trans.parent.position` or an attack-bone origin. `PEAttackTrigger.UpdatePartsInfo` supplies `m_PEEntity.centerPos`, or `Vector3.zero` if there is no cached `PeEntity` owner.

[PeEntityPartial.cs:278](../../Assets/Scripts/PeEntity/PeEntityPartial.cs#L278) resolves `centerPos` through `PeTrans.center`. [PeTrans.cs:178](../../Assets/Scripts/PeEntity/Cmpt/PeTrans.cs#L178) transforms `mLocalBounds.center` by the entity transform. Model binding initializes those bounds through `PEUtil.GetLocalColliderBoundsInChildren` ([PeTrans.cs:36](../../Assets/Scripts/PeEntity/Cmpt/PeTrans.cs#L36), [line408](../../Assets/Scripts/PeEntity/Cmpt/PeTrans.cs#L408)).

[PEUtil.cs:1300](../../Assets/Scripts/Assist/PEUtil.cs#L1300) temporarily removes model rotation, collects child colliders, excludes triggers, transforms each collider-bounds center into model-local space and combines the bounds before restoring rotation. It does not simply choose an arbitrary root or forearm bone. A faithful supported actor binding must derive this center from the relevant recovered nontrigger geometry and transformations. An uninitialized/default bounds center resolves to the entity origin; it is not automatically the null-owner zero branch.

During collision, the stored **current** parent center is used to measure both previous and current endpoint radii. There is no separately retained previous parent center. Unity's Vector3 comparison to zero controls whether radial correction runs. The null-owner path and a genuine center comparing equal to world zero take the linear branch. Exact Unity comparison/normalization and segment-closest arithmetic require their own numeric source/assembly audit; do not replace those predicates silently.

## Qualified player center metadata

[export_player_collider_center.py](../../rebuild/tools/export_player_collider_center.py) now qualifies both source player prefabs and generates [pe_player_collider_center_source.h](../../rebuild/generated/pe_player_collider_center_source.h) plus its [provenance manifest](../../rebuild/generated/pe_player_collider_center_source.json). The generated array is female0/male1; both local centers are **(0,.875,0)**.

| Source prefab | PEModelController | Model CapsuleCollider | SHA-256 |
| --- | --- | --- | --- |
|FemaleModel.prefab|11429522|13600000|`1e518e5af9afd09d89e16a3a2dd776abd42846143ff6b40e409fdf4bb08ae441`|
|MaleModel.prefab|11420780|13654112|`fe19a5d5019c8f2273fe562aa536015ddecb2eae44ba2169d864c4517150ae58`|

The exporter resolves actual script GUIDs and the BiologyViewRoot→PEModelController reference, validates identity model/root transforms and active objects, and requires exactly one nontrigger collider in the **Model subtree**. That collider is enabled, Y-aligned, height1.75, radius approximately.37 and centered at(0,.875,0). All eleven other nontrigger colliders per prefab belong to the sibling Ragdoll subtree. This avoids incorrectly combining ragdoll geometry into the living model center. The helper's generated OutCollider objects are parented under the outer BiologyViewRoot, also outside Model ([BiologyViewCmpt.cs:365](../../Assets/Scripts/PeEntity/Cmpt/BiologyViewCmpt.cs#L365)). The source model-build message passes the PEModelController's GameObject to PeTrans, establishing the query root.

This qualifies the source living-model center for the supported unit-scale player setup; it is not a new capture of installed runtime component values or arbitrary modified hierarchies. The host must still transform the local center using the actual entity position/yaw/scale. Changed bindings, extra nontrigger Model colliders, disabled roots, nonidentity model transforms and malformed geometry fail closed.

Offline verification: ordinary Python ran10 structural tests successfully and explicitly skipped one UnityPy-dependent test. The existing UnityPy interpreter ran all11 tests successfully, including real female/male prefab regeneration; `export_player_collider_center.py --check` also passed. These are source/export tests, not physical contact evidence. Test entry: [test_player_collider_center.py](../../rebuild/tests/test_player_collider_center.py).

## Collision sampling semantics

[PECapsuleTrigger.cs:159](../../Assets/Scripts/Attack/PECapsuleTrigger.cs#L159) computes each endpoint's sample count as `floor(.5 * displacementMagnitude / scaledRadius) + 1`, takes their maximum, and interpolates previous→current endpoints. Where the center is nonzero, each intermediate vector is normalized and scaled to the linearly interpolated previous/current radial distance. This is not a generic straight capsule sweep or a general angular interpolation.

The single-sample case uses interpolation parameter0, therefore tests the **previous** attack endpoints. With more samples, both ends are included. Each sample compares against the defence capsule's current endpoints using WhiteCat's segment closest-point helper, with strict penetration `< (r1+r2)^2`; exact tangency is not a hit. Contact direction includes both attack-endpoint displacements plus the closest-point separation. The first successful sample returns.

`PEAttackTrigger.CheckHit` updates the queried defence once through its lazy cache and visits attack parts, then defence parts in authored array order ([PEAttackTrigger.cs:213](../../Assets/Scripts/Attack/PEAttackTrigger.cs#L213)). All successful capsule contacts report distance0, so the strict replacement rule preserves the first overlapping pair. General target/right-versus-left callback order is not established merely by that per-trigger loop.

[PEDefenceTrigger.cs:243](../../Assets/Scripts/Attack/PEDefenceTrigger.cs#L243) lazily updates defence parts; its LateUpdate clears the cache and aligns the trigger with `modelRoot`. Defence parts call `capsule.Update(Vector3.zero)`. Their history is maintained incidentally, but this attack-versus-defence method tests their **current** geometry, not a sweep of both moving objects. Relative ordering of different LateUpdate components remains a runtime scheduling boundary.

## Sword/Knife event chain

The animation `StartAttack` event sets `SkipWaitPre`, activates the named collider/trigger and records `Time.frameCount` ([Action_Sword.cs:550](../../Assets/Scripts/PeEntity/Cmpt/Motion/PEAction/Action_Sword.cs#L550)). The active trigger is resolved below the actual Animator, not selected from an unrelated generic weapon model.

`ActiveCol` ([line300](../../Assets/Scripts/PeEntity/Cmpt/Motion/PEAction/Action_Sword.cs#L300)) handles reuse separately from switching triggers. Reusing the same trigger resets hit eligibility and activates it; switching clears/unsubscribes/disables the old trigger, then binds/enables the new one. None of these branches resets endpoints. `InactiveCol` resets eligibility and disables the current trigger ([line344](../../Assets/Scripts/PeEntity/Cmpt/Motion/PEAction/Action_Sword.cs#L344)). Full action cleanup clears lists, unsubscribes and nulls the action's trigger reference ([line400](../../Assets/Scripts/PeEntity/Cmpt/Motion/PEAction/Action_Sword.cs#L400)); the trigger object's history remains if the object survives.

`EndAttack` starts a coroutine which waits until at least `StartAttackFrame + 2`, then sets `SkipWaitMain` and disables the collider ([line589](../../Assets/Scripts/PeEntity/Cmpt/Motion/PEAction/Action_Sword.cs#L589)). This is a rendered-frame minimum, not two fixed simulation ticks. The skill-guide and animation-trigger gates remain separate. Outgoing history activity follows the actual named trigger activation window, not the entire Sword action, combo-input window, skill collision permission or target deduplication state. The native `contact_open` flag represents this mechanical interval within its already documented simulation-clock adapter.

## Hare Animator trigger chain

Existing `hare-manifest.json` provenance was produced by [export_reference_actor.py:145](../../rebuild/tools/export_reference_actor.py#L145) from installed `Monster/m_lepus_hare.unity3d`, SHA-256 `94409ad1bf9570048a52c42671986907e2a5ca131f55f73737e5b7d7cff76c35`. Its recovered windows are:

| State | Behaviour | Trigger bones | Normalized window |
| --- | --- | --- | --- |
|attack0|PEAnimatorAttackTrigger|Right forearm|[.372,.642]|
|attack1|PEAnimatorAttackTrigger|Left forearm|[.372,.642]|
|attack2|PEAnimatorAttackTriggerEX|Right, then left forearm|[.24,.34), [.5,.66)|
|attack3|PEAnimatorAttackTrigger|Right forearm|[.241,.631]|
|attack4|PEAnimatorAttackTrigger|Left and right forearms|[.419,.7]|

Decimal values summarize serialized floats; use exported values in runtime calculations. [PEAnimatorAttackTrigger.cs:70](../../Assets/Scripts/Assist/AnimationStateInfo/PEAnimatorAttackTrigger.cs#L70) clears eligibility/disables on entry and exit, and activates within the inclusive window. Its helper named `ResetHitInfo` actually calls the trigger's **ClearHitInfo**, not its ResetHitInfo. Outside the window, it clears and disables again.

[PEAnimatorAttackTriggerEX.cs:81](../../Assets/Scripts/Assist/AnimationStateInfo/PEAnimatorAttackTriggerEX.cs#L81) handles each authored window independently: end is exclusive, completion disables and resets trigger eligibility, and sends `MonsterEndAttack`; entry/exit clear the configured triggers. If a frame skips directly past the end, the end branch wins without opening that window. Neither behaviour resets endpoint history. Attack2's forearm windows must not be confused with the independent skill guide indices.

## Teleport, load, model and equipment boundaries

The inspected attack/capsule classes contain no teleport, style-change, OnEnable or serialization history hook. `PeTrans.position` updates transforms without notifying capsule history ([PeTrans.cs:73](../../Assets/Scripts/PeEntity/Cmpt/PeTrans.cs#L73)). A surviving active trigger can therefore observe a large endpoint displacement; a surviving inactive trigger retains old endpoints until its next activation. Do not invent a source distance threshold which discards such history.

Removing actual equipment destroys its GameObject ([PEEquipment.cs:343](../../Assets/Scripts/Items/PEEquipment/PEEquipment.cs#L343)); a newly instantiated trigger then starts with zero vectors. Setting Sword action equipment to null ends the action ([Action_Sword.cs:19](../../Assets/Scripts/PeEntity/Cmpt/Motion/PEAction/Action_Sword.cs#L19)); changing style/weapon does not itself prove the old trigger object was recreated. Model destruction ends actions through [PEAction.cs:256](../../Assets/Scripts/PeEntity/Cmpt/Motion/PEAction/PEAction.cs#L256). Loading recreates model/equipment components rather than persisting these private trigger vectors.

## Native integration and its limits

Bind endpoint history to the exact supported attack-trigger/weapon instance, separately from current skill-guide deduplication. Retain it across inactive windows and same-object combo stages. Supply the correctly transformed entity collider-bounds center and current defence pose. Advance history according to explicit activation and active sampling calls; resolve all targets from the same snapshot without advancing once per target. Missing/malformed input must leave committed history and damage unchanged.

A native fixed-tick host cannot silently claim Unity LateUpdate/PhysX scheduling parity. If it seeds equal endpoints on initial activation, discards history on a load/teleport boundary, limits sample count, or samples only once per fixed tick, label those choices as bounded adapters. Do not manufacture historical poses from a different clip, infer a previous center from a bone, or reset on each guide solely to make contacts convenient. New-instance history and same-instance reactivation are different lifecycle cases.

[pe_capsule_history](../../rebuild/src/pe_capsule_history.c) now owns the numerical query. It reproduces source sample count/order, current-radius use, current-target testing, current-center radial correction, strict tangency, WhiteCat closest-point branches and contact direction. `PeHitboxContact.normal` remains the diagnostic axis-separation normal; `PeCapsuleHistoryContact.hit_direction` preserves the distinct source direction including endpoint displacement. Callers retain the first overlapping authored attack/defence pair. The existing instantaneous helper is retained explicitly for legacy callers, including the separate tutorial; this increment enables history in Adventure sandbox combat.

The installed UnityEngine managed assembly supplies the vector equality/normalization thresholds. [The exporter](../../rebuild/tools/export_capsule_history.py) guards the five original numerical method bodies, assembly hash and relevant method IL. Native finite-coordinate/radius bounds and a4096-sample maximum are safety/work limits, not original gameplay values. Excess work returns INVALID without truncating or spreading the sample sequence. This fails the enclosing staged transaction rather than silently accepting a partial contact set.

[PeSandboxMelee](../../rebuild/src/pe_sandbox_melee.h) owns one transient blade snapshot. The frontend opts in using the exported player center; setup remains explicit so legacy low-level users do not silently change behavior. After authoritative action advancement and any running displacement, a continuously open trigger moves current endpoints into previous and samples the new visible pose exactly once. No targets, permission rejection, misses and guide duplicates still advance that snapshot. Target resolution borrows it. Closing a window freezes endpoint values and retires active ownership. Cancel, changed equipped identity and reinitialization prevent an old active interval from carrying into another action. A new activation pins both endpoints to the resolved current pose: this deliberately coalesces activation and LateUpdate within the existing native60Hz clock. It does not reproduce every possible PhysX callback order or the original zero-initialized first callback interval.

[PeHuntCombat](../../rebuild/src/pe_hunt_combat.h) owns four histories per hare, preserving the actual attack record indices15–18 and joints22,35,6,13. `pe_hare_damage_history_step` samples each Animator-active forearm part after encounter movement, independently of skill contact eligibility and successful-guide dedupe. Both right-hand parts and both left-hand parts remain distinct. Current action identity/time and posed capsule agreement are validated before using a supplied history. Death and mission-owned cleanup clear the transient owner; load prepares a new encounter. None of this state is serialized; the save format remainsv16.

Player HP, all target HP, stamina, action clocks, histories, exact sword/outfit/shield wear and kill progress remain part of the enclosing value-staged transaction. A later invalid input cannot publish earlier contacts or wear. Current blended player defence and authored pose clocks remain authoritative. The incoming/outgoing changes do not alter damage formulas, creature counts or stats.

The source candidate/effected-trigger broadphase and global callback ordering remain unrecovered; the native encounter still supplies its bounded eligibility/target order. The source minimum two rendered frames for Sword contact remains the existing two-native-update adapter. There is no claim of exact Unity frame/physics scheduling, continuous collision, general teleport relocation or combat difficulty parity. Active large displacements are sampled until the explicit work bound; no invented distance threshold discards them.

## Offline numerical qualification

A standalone process loaded the installed game's `PE_Client_Data/Mono/x86_64/libmono.so`, its managed compiler and pinned `UnityEngine.dll`, then evaluated the original source math without creating a game scene. This is installed-managed numerical evidence, not an original-gameplay observation. Across4,103 bounded cases (2,714 hits), native hit/miss results agree; maximum recorded contact/direction component error is2.3841e-7. The comparison tolerance is2e-5. [The numerical evidence manifest](../reference/melee-capsule-history-numerics.json) records input hashes and exit statuses. The full local harness/corpus is retained ignored at `rebuild/build/capsule-history-audit`; rerun it from the repository root with `python3 rebuild/build/capsule-history-audit/compare.py`. Only selected golden cases and metadata are committed; the installed binaries and generated oracle executable are not distributed. Near-parallel and degenerate cases matter: the installed Mono retains wider scalar expression intermediates before assigned float rounding, so a superficially equivalent float-only determinant can pick a materially different closest point. The C implementation preserves the qualified evaluation structure; this finite corpus is not a universal bit-exact proof.

[The committed numerical tests](../../rebuild/tests/test_capsule_history.c) retain original-managed golden cases and cover intermediate-only hits, previous-only count1, radial versus linear behavior, tangency, first sample order, degenerate segments, Unity normalization, coincident axes, sample-limit boundaries and unchanged outputs on invalid/miss. Outgoing tests cover shared six-target snapshots, zero-target/deduped updates, guide-list resets without history reset, same/new-instance reactivation and failed-frame rollback. Existing recovered running-attack and hunt lifecycle tests now opt into outgoing history. Incoming tests use the four actual authored parts and exercise skill-disabled history advancement, separate windows, invalid later parts and death cleanup. These arranged tests remain distinct from four ordinary gameplay kills.

## Verification checkpoint

Final build, integration, sanitizer and source-regeneration outcomes are recorded in [PLAYTEST](../PLAYTEST.md). No game was launched, no desktop input was sent, and no normal save was changed by this increment. Ordinary four-hare completion, partial-kill physical reload, visual contact acceptance and retreat/recovery remain open. The source-first next workstream is sword/shield action integration while masked block-pose qualification remains separate.
