# Gather harvest-commit and weight-boundary capture plan

This is a source-only planning note, written October 15, 2026, after the controller-clock replay in [GATHER_LIVE_CAPTURE](GATHER_LIVE_CAPTURE.md). It proposes no launch yet, and no probe code is part of this checkpoint. The native Gather gate stays closed, and immediate herb harvesting remains the native adapter. Every statement below is **source-derived** unless it is marked otherwise.

## 1. Harvest commit: source call chain

1. The Herb state's exit sends `Gather` to `AnimatorCmpt.AnimEvtString`, then to `Action_Gather.OnAnimEvent`, then to `Gather()`.
2. `Gather()` calls `SkEntitySubTerrain.Instance.GetTreeHP(mOpTreeInfo.WorldPos)`. If HP > ε it calls `skillCmpt.StartSkill(SkEntitySubTerrain.Instance, 20110003)`. Otherwise it ends the action.
3. Skill 20110003 has every time at 0. Trigger234 sets the target's `sums[0]=10000` (damage), `sums[1]=caster.sums[23]` (resource bonus) and `sums[2]=1` (return resource), and adds 0.2 to the caster's `raws[74]`. `SkInst.Exec` has no yield before its event execution when every time is 0. Whether the whole chain therefore completes inside the `StartSkill` call is **provisional**: it rests on unverified coroutine start semantics and has not been observed.
4. `SkEntitySubTerrain.onAlterAttribs` reacts only to `idx==2`. It:
   - reads the caster's `ISkSubTerrain.treeInfo`, which is `m_Live.gather.treeInfo` while Gather is running;
   - stores `treeHPInfos[WorldPos] = DigTerrainManager.Fell(...)`, which reads `GetTreeHP` again;
   - when HP ≤ 0, calls `OnTreeCutDown` and `DigTerrainManager.RemoveTree`, and with return-resource set calls `GetTreeResouce`, which uses `UnityEngine.Random`;
   - then `PlantItemAdd`, then `caster._attribs.pack += items`.
5. The pack addition runs `SkPackage.PushIn`, then `OnPutInPakAttribs`, then `SkAliveEntity.OnPutInPak`, then `PlayerPackageCmpt.Add(protoId, count)` for each item.
6. `Add` checks `package.CanAdd` before `package.Add`, then `SendGetItemEvent(protoId, count)` and `CheckMainPlayerGetItem`. A failed `CanAdd` returns false with no event and no retry, after the herb has already been removed. That **silent capacity loss** is source behaviour; the native adapter's retry policy is a deliberate difference.

## 2. Unresolved question

During the `Gather` receipt, between the moments just before and just after the `Action_Gather` handler, does the harvest effect happen inside that single callback, with no later frame involved? The harvest effect means:

- the herb HP entry appears and falls to ≤ 0;
- the item grant fires with exact item IDs and counts.

Whether, and when, the skill instance starts stays **unqualified**. No planned field records it, and a zero-duration skill may finish before the post snapshot. The capture observes only the HP and bag effects.

**Why the current evidence cannot answer it.** `gather-live-01` recorded no tree HP, package or grant state. The user's report that the gathers "gave items" carries no frame, item ID or count. The source chain depends on unverified coroutine start timing. The resource quantities it produces depend on `UnityEngine.Random`. It therefore predicts the shape of the result, not observed values.

## 3. Read-only snapshot fields

| Value | Read through | Never call |
|---|---|---|
| Subterrain entity | static field `SkEntitySubTerrain.m_Instance` (raw). Null before the first original handler is recorded as **null**, not as "HP 255". | `SkEntitySubTerrain.Instance`: it lazily creates a GameObject and runs `InitSkEntity` |
| Herb HP | private `treeHPInfos` dictionary on that raw instance: entry present/absent and its value. An absent entry is not HP 255; the original handler's own `GetTreeHP` creates it, and that is the original's mutation. | `GetTreeHP`: it inserts 255 when the entry is missing |
| Selected herb | private `Action_Gather.mOpTreeInfo` and `mFindTreeInfo` (raw); `GlobalTreeInfo` world position and prototype index | `UpdateOPTreeInfo` (subterrain picking) |
| Bag | private `PlayerPackageCmpt.mPackage` (raw). Null is recorded as null. Item counts come from raw enumeration of the existing containers. | the `package` getter (lazily constructs `PlayerPackage`); `GetCount` or `ItemProto` lookups, unless they are first shown to be pure |
| Grant events | subscribe the observer's own handler to `PlayerPackageCmpt.mEventor` (`getItemEventor` returns that field directly) and record protoId, count and frame | any `Add`/`Remove` path |
| RNG | none | `UnityEngine.Random`: `GetTreeResouce` consumes it, so the observer must not |

**Handler order.** Keep the existing pre-Gather observer. Add a second observer handler on `AnimEvtString` only after `Action_Gather` has subscribed, at the first end-of-frame where action32 is executing, and record the actual invocation list on every receipt. A later subscriber (`Action_HandChangeEquipHold` subscribed during trial 2) may sit after the post handler. Then label each snapshot by its recorded position, never by an assumed order.

**Smallest capture.** One bare-hands Adventure gather, using an extended `ReferenceGatherProbe`. It needs the pre/post `Gather` snapshots of the fields above, the grant-event subscription and the existing end-of-frame records. A second gather on a full bag would exercise the silent-loss branch, but it needs a prepared checkpoint and is optional.

## 4. Layer-weight boundary in an isolated clone

**Question.** Does `SetLayerWeight`, called from `AnimatorLayerWeightCtr.OnStateExit` inside an Animator update, affect that same update's pose? `gather-live-01` supports a one-update lag, but only as a provisional hypothesis: see [GATHER_ENTRY_POSE_DIAGNOSTIC](GATHER_ENTRY_POSE_DIAGNOSTIC.md). Live IK cannot be excluded there.

**Reuse.** `export_shield_controller.py` already stages an isolated copy of controller779, with an optional variant that disables IK. But `sanitize_controller` strips **all** state-machine behaviours, and the composition and qualification clones evaluate static `Play(state, layer, t)` followed by `Update(0)`. As they stand, they can answer neither the weight callback question nor a time-advancing transition.

**Smallest change, for a future goal only.**

1. **Keep only the needed behaviours.** Retain `AnimatorLayerWeightCtr`, which calls only `Get`/`SetLayerWeight` and reads `Time.deltaTime`. Retaining `AnimatorEventSender` is optional and needs separate verification. Its cached `m_AnimCtrl` must be shown to be null, or to target the clone itself, for the behaviour instances actually bound to the clone. Isolation must not be assumed from the new GameObject alone. Behaviour instancing and caching per Animator are engine details that need checking.
2. **Keep stripping the rest.** `AnimatorParamCtrl` and `PEAnimatorAudio_Human` are stripped, with a re-audit of the stripping logic's safety and provenance.
3. **Step the clone in time, with the right clock.** `AnimatorLayerWeightCtr` fades with **`Time.deltaTime`**, not the `dt` argument passed to `Animator.Update(dt)`. Manually stepping with recorded deltas would therefore apply the fader with the wrong delta. A clone run must record both values and either keep them equal by setup, or use ordinary updates. Its result must be labelled with the delta domain it actually used.

Even then, a clone result isolates the mechanism; it does not by itself establish live parity. The existing probe loader takes one assembly per launch, so this would be a second bounded launch with no gameplay input, separate from the commit capture. No clone or input code is written yet.

## 5. Not covered

Neither capture qualifies:

- the native action owner, the deferred `DoAction` start or mask release;
- cancellation;
- NPC gathering;
- graphics.

The fitted weights in the entry diagnostic are not accepted native values.
