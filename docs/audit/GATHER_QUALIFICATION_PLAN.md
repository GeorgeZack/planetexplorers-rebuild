# General Gather: smallest qualification plan

October 15, 2026, at private commit `bbecc7b`. This is an offline planning checkpoint. It follows the [Gather action gate](GATHER_ACTION_GATE.md) and the [source-first workflow](SOURCE_FIRST_REBUILD.md). No original game was launched, no input was sent, no asset was installed, and no runtime or tool code was changed. The immediate transactional herb harvest in [SANDBOX_HARVESTING](SANDBOX_HARVESTING.md) stays as it is.

Labels used below: **source-derived** (C#, database or serialized controller), **observed** (existing original captures), **implemented in C**, and **provisional** (a hypothesis that still needs an engine check).

## 1. Revalidated evidence

| Check | Result | Label |
|---|---|---|
| Installed `PE_Client_Data/sharedassets1.assets` SHA-256 | `d9c65f820a2186116d9eb73d815fe07d9f46924ddfd103ee9510bf254f9a3d67`, same as the gate audit. Controller779 and clips267/273/280 are therefore unchanged. | source-derived |
| PEAC entries under `rebuild/build` | 1,616 regular actor files, all structurally valid v1/v2, 335 of them under `reference-assets`. The director's diagnostic scan of these regular files is `rebuild/build/compiler-tmp/gather-current-peac-scan.json`. Path enumeration that follows symlinks (pathlib `rglob`, `find`) reports 1,625 entries. The 9 extra entries are symlinked `hare.peac` fixtures in the assets-baked, reaction, recovery and sprint-review capture directories, each 835,096 bytes and pointing to existing hare data. They are not malformed files. Neither the director's clip-name scan nor a binary-safe byte search for `HerbDown`/`HerbUp`/`Herb\0`/`Gather` (and a case-insensitive `herb` search) found a match. | observed (name/structure scan) |
| The October 7 scan | Covered only `reference-assets` (335 files) and only matched names containing `gather`. It is superseded by the wider search above, which gives the same answer. | historical |
| JSONL captures in `rebuild/build` and every `~/.local/share/planet-explorers-*` profile | 81 files. Three contain the string `Gather` only as an unread trigger parameter (`"Gather":null`). None contains `"Gather":true`, the text `FullBody.Gather` or a Herb clip name. The shield replay plans name the clips only because they embed the controller. | observed (string search) |
| Sampler allowlists | Python `CLIPS` in `inventory_reference_bake.py` and C# `Allowed` in `ReferenceAnimatorBake.cs` both exclude HerbDown, Herb and HerbUp. | implemented (tools) |

No qualified Gather-labelled pose, clip or capture evidence was identified. These are name and string scans, not a semantic review of unnamed poses. Captures store state hashes and clip identities, not always readable names, so the scans cannot prove that no unnamed captured pose ever came from a Gather state. Any such pose would also be unlabelled and unqualified, so it is not usable evidence either way.

## 2. Source call chain (source-derived)

1. **Admission.** `Action_Gather.CanDoAction` requires a transform, a found type10 resource and `!motionMgr.isInAimState`. `ActionRelation` row32 applies before `DoAction`. EndImmediately runs on 1 Move, 2 Sprint, 3 Rotate, 14 SwordAttack, 30 Dig, 31 Fell, 42 Pump, 45 Draw, 49 TwoHandSwordAttack, 50 Halt and 60/61. EndAction (an orderly end) runs on 4 Jump, 12 Step, 13 EquipmentHold, 15 HoldShield, 28 AimEquipHold, 47 TwoHandSwordHold and 58 Leisure. PauseAction applies to 40 HoldFlashLight. The motion manager's `RunningAction` waits until the EndAction targets stop; the same waiting rule is recorded in [SHIELD_ACTION](SHIELD_ACTION.md).
   - **Admission order.** `MotionMgrCmpt.DoAction` (`MotionMgrCmpt.cs`, around lines 326–360) checks `CanDoAction` and `CheckDepend`, then invokes `onActionStart(type)`. Only after that does it call `RunningAction.Do` and store the running action. `onActionStart` therefore marks an admission request. It does not prove that the deferred `Action_Gather.DoAction` and its `SetTrigger("Gather")` have run.
   - **What Gather blocks.** `ActionRelationData` reads relation columns from index5 as `PEActionMask` values. The `Gather` column (index34) is mask29 `Gather`. A value of2 means the new action requires that mask to be false. Rows 1 Move, 2 Sprint, 3 Rotate, 4 Jump and 13 HoldEquip all have `Gather=2`, and Move's own EndImmediately list (2, 3, 12) does not include32. Movement input during Gather is therefore **refused** by `CheckDepend`; it does not cancel the action.
   - **What ends Gather.** Rows that list32 are 5 Whacked, 7 Wentfly, 8 Knocked, 9 Death, 17 Fall, 26 Sleep, 41 Stuned, 52 Lie and 59 AlienDeath (EndImmediately), and 27 Eat, 39 Build, 54 Cutscene, 55 Cure, 58 Leisure and 61 Abnormal (EndAction). The inherited `PEAction.EndAction` calls `motionMgr.EndImmediately`, and Action_Gather does not override it. Sleep, Build and Abnormal themselves require the Gather mask to be false, so they cannot start during Gather. Eat's `DoAction` callers are NPC behaviour trees only. For a player, the reachable cancellations are therefore reaction and death rows such as Whacked, Fall or Death.
2. **Stowing a held tool.** `Action_HandChangeEquipHold.EndAction` (`Action_Tool.cs`) sets the put-off trigger or bool, clears its mask, calls `SetWeapon(null)` and waits for the `PutOffAnimEnd` animation event. A 5-second `m_FixErrorTimer` is the fallback. A held knife, axe or sword therefore delays the Gather start by an animation-event-driven put-off. That put-off is a second event clock that needs qualification.
3. **Start.** `PreDoAction` sets the Gather mask and retains `mOpTreeInfo`. `DoAction` calls `ResetTrigger("ResetFullBody")` and `SetTrigger("Gather")`, and clears `m_EndAnim`.
4. **Controller779, layer1 `FullBody`.** Override blending, default weight 1, a full body mask, no skeleton mask, `m_IKPass=false`. The controller has these transitions:
   - DoNothing (no motion) to entry selector SEL8: condition `Gather` If, normalized duration0.1, no exit time. Entry goes on to HerbDown.
   - HerbDown(.5333347s) to Herb: exit time.5312512, duration.4687488.
   - Herb(.8000011s) to HerbUp: exit time.6874997, duration.3125003.
   - HerbUp(.6333332s) to exit selector SEL9, which leads to DoNothing: exit time.6052631, duration.3947369.
   - AnyState#0 to Empty: condition `ResetFullBody` If, duration0. Empty returns to DoNothing at exit time.02.

   No Gather transition has conditions after entry, all of them use non-fixed durations, and every exit time plus its duration equals exactly1.0. Higher layers 2 `UpbodyOnly` (override), 3 `LArmOnly` (override) and 4 `Arm` (additive) can still override or add over FullBody inside their masks.
5. **Events.** Behaviour1119 on Herb and behaviour1219 on HerbUp are `AnimatorEventSender` instances with exit events `Gather` and `GatherEnd`. They have no enter events and no timed events. The delivery path is `OnStateExit`, then `SendMsg`, then `AnimatorCtrl.AnimEvent`, then `AnimatorCmpt.AnimEvent`, then the public `event Action<string> AnimEvtString`. `PEAction.anim` lazily subscribes `OnAnimEvent` to that event. Delivery happens synchronously inside the Animator update. `PEAction.anim` subscribes on its first getter access (`PEAction.cs`, around line 230). Action_Gather first reads `anim` in `DoAction`, so a handler that subscribes earlier can run **before** `Action_Gather.OnAnimEvent`. Handler order is therefore not fixed by source and has to be recorded at runtime.
6. **Harvest commit.** On `Gather`, if the retained tree still has HP, the action calls `skillCmpt.StartSkill(SkEntitySubTerrain.Instance, 20110003)`. In `skMain`, skill20110003 has every prepare, main and ending time at0, cooling0, `_cond=False`, and trigger234 (`_cond=True`, target mods `sums[0]=10000; sums[1]=caster.sums[23]; sums[2]=1; caster.raws[74]+=0.2f`). `CoroutineStoppable` calls `StartCoroutine` from its constructor, and `SkInst.Exec` reaches `ExecEventsInGuide` with no yield when every time is zero. This source path suggests a synchronous commit before the first yield, but the installed engine's coroutine dispatch and the actual harvest boundary have not been observed. Treat that timing as provisional; an event receipt alone proves neither material delivery nor the end of the skill.
7. **End.** `GatherEnd` sets `m_EndAnim`. The next `Action_Gather.Update` clears the mask and returns done. This is one action update after the event, not in the same callback. Normal completion does not clear `mOpTreeInfo`. `EndImmediately` clears the mask, sets `ResetFullBody` (a zero-duration snap to Empty), resets `Gather` and clears both tree references.
8. **Root motion.** HerbDown and HerbUp have nonzero `m_AverageSpeed` (z≈.216m/s and x≈.172, z≈.093m/s), with `KeepOriginalPositionXZ=false`. Herb has none. Whether the live player applies this root travel has not been determined.

The tutorial is a different path. `HoloherbGather` calls `AnimatorCmpt.SetBool("Gather", true)` on a trigger parameter and runs its own 1.1s and 1.966s clocks. It cannot qualify the general action, and its lesson rules must not transfer.

## 3. Provisional nominal schedule (not an implementation value)

If `OnStateExit` fires on the update where each outgoing normalized transition finishes, the controller data predicts the following times from the Gather entry start (HerbDown at time0):

| Point | Time after HerbDown start |
|---|---|
| Herb begins blending in | .28333s |
| HerbUp begins blending in | .83333s |
| **`Gather` exit event (harvest)** | **1.08333s** |
| **`GatherEnd` exit event** | **1.46667s** |
| Mask released | next action update after GatherEnd |

Three things are still unknown before HerbDown's time0:

- The DoNothing entry's 0.1 *normalized* duration on a state with no motion.
- Which frame consumes the trigger.
- Any deferred EquipmentHold put-off.

This schedule is a **provisional hypothesis only**. It must not become a native delay until an engine capture confirms it. Frame quantization, normalized-time carryover across transition starts, and the frame on which the exit callback is delivered are all engine behavior.

## 4. Why current tools cannot answer it

- **`reference_animator_bake.py` / `ReferenceAnimatorBake.cs`** use `AnimationClipPlayable`, `runtimeAnimatorController=null`, `fireEvents=false` and no attached controller. Adding a Herb preset could recover leaf body poses, but it cannot show transitions, layer composition, IK or any `OnStateExit` delivery. A preset on its own also fails `read_plan`, because both allowlists exclude the clips.
- **The shield composition and qualification clones** strip every state-machine behaviour before loading. They also replay *static* `Play(state, layer, t)` plus `Update(0)` poses, not a time-advancing triggered sequence.
- **`ReferencePoseProbe`** already records, at end of frame, per-layer current and next state hashes, normalized time, the transition flag, clip weights, frame/time/delta values, the full player and model hierarchy (which includes stowed and held attachment transforms) and the Animator root. It does not record animation-event receipts or action start and end.

## 5. Smallest qualification

### Lane A: leaf body, attachment and defence poses (reuse the qualified clip-bake pipeline)

This lane follows the established isolated sampler route, the same one used for the Felling, Mine and reaction groups.

1. Add `HerbDown:267`, `Herb:273` and `HerbUp:280` to `inventory_reference_bake.py` `CLIPS`, with the controller leaf check for FullBody states 22–24. Add the same names to `ReferenceAnimatorBake.cs` `Allowed`. Add a `gather` preset (`NormalIdle` anchor plus the three clips) to `reference_animator_bake.py`. `read_plan` counts `ceil(length*rate)+1` samples per clip. At 30Hz that gives HerbDown .5333347321s → 18, Herb .8000011444s → 26 and HerbUp .6333332062s → 20. NormalIdle adds the 62 samples recorded in earlier runs, for an estimate of 126 samples over 4 clips. Take the exact count from the regenerated plan, not from this estimate. It is far below the 32-clip, 3,600-sample and retention bounds. Female only.
2. Regenerate the plan from the SHA-bound inventory. Run the existing tests and add new ones. Each edited tool needs focused tests plus `make -C rebuild check`; run `check-sources` if a shared source list changes.
3. In the same supervised session as Lane B, or one immediately before it, bake with the existing tutorial-entry procedure. Then use `analyze_reference_bake.py`, the held/stowed attachment exporters, and `export_player_defence.py --extra-capture` so that body, attachments and defence share one clip clock. Stage the result as a separate complete group; never append it to an installed group.

This lane qualifies **only** leaf poses on the existing rig. It is not event or transition proof.

### Lane B: event and composed-controller observation (new bounded observer)

This is the single engine question that remains: **on which Animator update, relative to the recorded state info, does Unity 5.2.4f1 deliver the Herb and HerbUp `OnStateExit` events? What is the full composed sequence from `Gather` trigger acceptance through mask release, and how does a held tool's put-off delay it?**

The preparation needed is a dedicated opt-in observer. It is the next director-assigned preparation task, and it falls outside this checkpoint's assigned file scope. It needs no further user approval: it is routine, reversible tools work. It extends or sits alongside the existing `reference_pose.py`/`ReferencePoseProbe.cs` probe:

- Add **only its own** handler: `AnimatorCmpt.AnimEvtString += Observe`. Remove it at stop and teardown. Never replace, reorder or clear existing handlers. Never touch `PEAction.anim` or other getters to force a subscription, and never call `AnimEvent`, set parameters or start actions.
- **Do not assume handler order.** `PEAction.anim` subscribes lazily (see §2.5), so the observer may run before or after `Action_Gather.OnAnimEvent`. At each receipt, record the actual order read-only from the delegate invocation list, using `GetInvocationList` through reflection on the event's backing field without modifying it. Also record whether the observer comes before or after the `Action_Gather` handler. Label each receipt's state as **before Gather's handler** or **after Gather's handler** from that order. Do not call it pre-harvest or post-harvest without separate evidence of the skill's commit boundary. Record the observer's own allocation and IO as side effects.
- For each receipt, record:
  - the event string and `Time.frameCount`;
  - `Time.time`, `realtimeSinceStartup` and `deltaTime`;
  - FullBody layer1 current and next state hashes, normalized times and `IsInTransition`;
  - `MotionMgrCmpt` running state for Gather (32), EquipmentHold (13) and EquipmentPutOff (16), plus the Gather mask.
- Record action edges under separate labels:
  - `onActionStart` is an **admission request**: it is raised before `RunningAction.Do` and before the running state is stored;
  - **executing** is the first sample where the action is running, the Gather mask is set and the FullBody state or trigger shows that `DoAction` ran;
  - **end** is the update where the action stops running.

  The admission label must never stand in for the deferred `DoAction`.
- Record the end-of-frame snapshot that the existing probe already makes as a **separate** record: body, held and stowed attachments, defence-bone transforms and player root. Never merge it with in-callback receipt state.
- Keep the probe bounds: at most 3,600 frames, 120s and 256MiB, with fresh paths inside an isolated profile.

Use **one capture in the Adventure sandbox reference profile**. Slot1 `audit` is preserved; save to a new slot or profile if anything changes. The operator walks to a reachable wild type10 herb, presses gather normally, and the tool injects nothing. Three batched trials:

1. Bare hands: the plain three-state lifecycle and both named exit receipts.
2. Knife33 drawn: EquipmentHold's orderly put-off, the `PutOffAnimEnd` receipt, the stowed-attachment pose and the deferred `DoAction`.
3. A movement press held through HerbDown and Herb: a **refusal** record, not a cancellation. Source predicts that Move's `CheckDepend` fails on the Gather mask (§2.1). The run should show no `onActionStart(Move)`, an unchanged Gather mask, an uninterrupted state sequence and both receipts. If movement does cancel Gather, that contradicts the source reading and keeps the gate closed until it is explained.

Cancellation is not a scripted trial. Reaction and death are candidate source routes (§2.1); whether either actually interrupts this player must be established from the recorded admission/running/mask state. Record a naturally occurring interruption if one happens, without arranging a combat encounter for this capture.

An interruption is not evidence that no animation-event receipt can follow. `AnimatorEventSender.OnStateExit` sends its exit string without checking whether the exit was normal or interrupted. Resetting FullBody while Herb is active can therefore still deliver `Gather`; `Action_Gather.OnAnimEvent` checks that the action is running, and `EndImmediately` clears its retained target. Record any late receipts separately from actual harvest effects. The invariant to qualify is no grant from an ended/canceled action, not absence of an exit receipt.

**Expected capture.** Three short trials, with an initial target of at most 30 seconds of active sequences inside the 120-second capture cap. The actual frame count depends on runtime performance; preserve recorded timestamps rather than assuming a frame rate. The receipts expected are `Gather` and `GatherEnd`, plus `PutOffAnimEnd` in trial2. The harvest itself is visible as an inventory or herb change in the game, not in the probe.

**Offline validation after capture**, all before any native lifecycle code:

1. Replay a pure normalized-time scheduler (controller data from §2, the *recorded* per-frame deltas) and require it to predict every event receipt frame and every current/next state change **exactly** for all trials. Any mismatch keeps the gate closed.
2. Compare stable segments of the observed pose with the Lane A leaf bake, the same way the Attack6 check was made: report every bone and accept no hidden outliers. Report transition-window errors separately as the cost of the native blend adapter. Differences from IK, FBBIK or Grounder stay labelled and unexplained.
3. Measure the observed player root drift during HerbDown and HerbUp, and decide from that evidence whether root travel applies.
4. Only then design `pe_sandbox` Gather ownership. Retain the selected plant and run one composed action clock. Commit the existing atomic harvest at the qualified harvest boundary (expected at Herb exit, subject to the skill timing check), keep a capacity failure retryable with no duplicate grant, and release the mask on the update after `GatherEnd` or on cancellation. Keep the actor-only mask, never `consume_world`. A missing Gather group keeps today's immediate adapter; a malformed present group fails closed.

## 6. What this checkpoint did not do

- No tool, test or runtime change. A narrow edit to `reference_animator_bake.py` alone is not useful: a Gather preset fails `read_plan` until `inventory_reference_bake.py` and `ReferenceAnimatorBake.cs` also change, and the event observer belongs to the pose-probe owner. Both preparation changes are listed above as the next director-assigned tools work. They are reversible and need no separate user approval round.
- No launch, desktop input, asset staging or installation.
- No claim that a raw clip end, a transition threshold or the §3 sum equals `OnStateExit` timing.

Local scratch evidence: the decoded FullBody transition dump was made from `rebuild/build/gather-action-audit/controller-779.json`. A read-only copy of the database used for the skill and trigger rows is at `rebuild/build/compiler-tmp/localData-gather-ro.sqlite`.
