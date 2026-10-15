# General Gather: source contract and gate

**Gate: closed.** Immediate transactional herb harvesting ([SANDBOX_HARVESTING](SANDBOX_HARVESTING.md)) remains the native adapter.

| Topic | Owner |
|---|---|
| Live observation, leaf bake, clock replay | [GATHER_LIVE_CAPTURE](GATHER_LIVE_CAPTURE.md) |
| First-stable pose residual | [GATHER_ENTRY_POSE_DIAGNOSTIC](GATHER_ENTRY_POSE_DIAGNOSTIC.md) |
| Harvest-commit and weight-boundary questions | [GATHER_COMMIT_CAPTURE_PLAN](GATHER_COMMIT_CAPTURE_PLAN.md) |

## Status

| Item | State |
|---|---|
| Source call chain, controller graph, events | Source-derived (below) |
| Female HerbDown/Herb/HerbUp leaf poses with attachments and defence | Engine-sampled and staged, not installed (`rebuild/build/player-gather-01`) |
| FullBody controller clock | Qualified only within `gather-live-01`'s recorded delta domain (0.01352–0.11407 s) |
| Pose application at the entry boundary | Supported provisional hypothesis: a one-update layer-weight lag |
| Skill commit, admission and deferred start, mask release, cancellation | Unqualified |

## Source call chain

`Assets/Scripts/PeEntity/Cmpt/Motion/PEAction/Action_Gather.cs`:

1. **Admission.** `CanDoAction` requires a transform, a found type-10 resource and `!motionMgr.isInAimState`.
   - **Order.** `MotionMgrCmpt.DoAction` checks `CanDoAction` and `CheckDepend`, then raises `onActionStart`, and only after that calls `RunningAction.Do`. `onActionStart` is therefore an admission request, not proof that the deferred `DoAction` ran.
   - **ActionRelation row 32.**
     - EndImmediately: 1 Move, 2 Sprint, 3 Rotate, 14 SwordAttack, 30 Dig, 31 Fell, 42 Pump, 45 Draw, 49 TwoHandSwordAttack, 50 Halt, 60, 61.
     - EndAction (orderly end): 4 Jump, 12 Step, 13 EquipmentHold, 15 HoldShield, 28 AimEquipHold, 47 TwoHandSwordHold, 58 Leisure.
     - Pause: 40 HoldFlashLight.
     - `RunningAction` waits until the EndAction targets stop ([SHIELD_ACTION](SHIELD_ACTION.md)).
   - **What Gather blocks.** The relation columns start at index 5 as `PEActionMask` values, and the `Gather` column (index 34) is mask 29. Value 2 means "requires that mask false". Rows 1, 2, 3, 4 and 13 have it, so movement during Gather is **refused**; it does not cancel.
   - **What ends Gather.**
     - Rows listing 32 as EndImmediately: 5 Whacked, 7 Wentfly, 8 Knocked, 9 Death, 17 Fall, 26 Sleep, 41 Stuned, 52 Lie, 59 AlienDeath.
     - Rows listing 32 as EndAction: 27 Eat, 39 Build, 54 Cutscene, 55 Cure, 58 Leisure, 61 Abnormal. The inherited `EndAction` calls `EndImmediately`.
     - Sleep, Build and Abnormal need the Gather mask false, so they cannot start during Gather. Eat is called only by NPC behaviour trees.
     - Ordinary movement requests do not cancel Gather; the listed interruptions do, when they are admitted.
   - **No global pause.** The Gather mask is player-action ownership; never reuse modal `consume_world` for it.
2. **Held tool.** `Action_HandChangeEquipHold.EndAction` (`Action_Tool.cs`) sets the put-off trigger, clears its mask, calls `SetWeapon(null)` and waits for `PutOffAnimEnd`, with a 5 s `m_FixErrorTimer` fallback. A drawn tool delays the Gather start by an event-driven put-off.
3. **Start.** `PreDoAction` sets the Gather mask and keeps `mOpTreeInfo`. `DoAction` does `ResetTrigger("ResetFullBody")`, `SetTrigger("Gather")` and clears `m_EndAnim`.
4. **Events.** The Herb state's `AnimatorEventSender` (behaviour 1119) sends `Gather` on `OnStateExit`; HerbUp's (behaviour 1219) sends `GatherEnd`. Neither has enter or timed events, and both use MonoScript 1916. Their raw 52- and 56-byte records were decoded against the class field order.
   - **Delivery path:** `SendMsg` → `AnimatorCtrl.AnimEvent` → `AnimatorCmpt.AnimEvent` → the public `event Action<string> AnimEvtString`, synchronously inside the Animator update.
   - **Lazy subscription.** `PEAction.anim` subscribes on first getter access (`PEAction.cs` around line 230). Gather first reads it in `DoAction`, so an earlier subscriber runs before `Action_Gather.OnAnimEvent`.
   - **Interruption.** A `ResetFullBody` interrupt still exits Herb and can still send `Gather`. The `IsActionRunning` check prevents a grant from an action that has already ended.
5. **Harvest.** On `Gather`, if `GetTreeHP` is above ε, the action calls `StartSkill(SkEntitySubTerrain.Instance, 20110003)`. Skill 20110003 has every time at 0, cooling 0, `_cond=False`, and trigger 234 (`_cond=True`; target `sums[0]=10000; sums[1]=caster.sums[23]; sums[2]=1`; `caster.raws[74]+=0.2f`). Whether the commit is synchronous is provisional ([plan](GATHER_COMMIT_CAPTURE_PLAN.md)).
6. **End.** `GatherEnd` sets `m_EndAnim`, and the next action update clears the mask. Normal completion keeps `mOpTreeInfo`. `EndImmediately` clears the mask, sets `ResetFullBody` (a zero-duration snap to Empty), resets `Gather` and clears both tree references.
7. **Root motion.** HerbDown and HerbUp have `m_AverageSpeed` z ≈ 0.216 m/s, and x ≈ 0.172 with z ≈ 0.093 m/s, with `KeepOriginalPositionXZ=false`. In `gather-live-01` the sampled player root did not move, with `applyRootMotion=false`.

The tutorial is a separate path: `HoloherbGather` calls `SetBool("Gather", true)` on a trigger parameter and runs its own 1.1 s and 1.966 s clocks. None of it transfers to the general action.

## Installed controller graph

Installed `sharedassets1.assets` SHA256 `d9c65f820a2186116d9eb73d815fe07d9f46924ddfd103ee9510bf254f9a3d67`, `FemaleAnimator2` 779. Layer 1 `FullBody` is override blending at weight 1, with a full body mask, no skeleton mask and `m_IKPass=false`. Hashes and clocks are bound in the inventory's `gather_controller` record.

| State | Clip (array index / path ID) | Length | Outgoing |
|---|---|---|---|
| 22 `FullBody.Gather.HerbDown` | 405 / 267 | 0.5333347321 s | → 23, exit 0.5312511921, duration 0.4687487781 |
| 23 `FullBody.Gather.Herb` | 406 / 273 | 0.8000011444 s | → 24, exit 0.6874997020, duration 0.3125002980 |
| 24 `FullBody.Gather.HerbUp` | 407 / 280 | 0.6333332062 s | → exit selector → DoNothing, exit 0.6052631140, duration 0.3947369158 |

- **Entry.** Motionless `FullBody.DoNothing` → entry selector, on condition `Gather` If, duration 0.1 normalized, no exit time.
- **Reset.** AnyState → `Empty` on `ResetFullBody`, duration 0. Empty returns to DoNothing at exit 0.02.
- **Transitions.** All durations are non-fixed, and each exit time plus duration equals 1.0. Every blend lasts 0.25 s.
- **Clips.** Humanoid bindings, empty `m_Events`; the events come from the state behaviours.
- **Idle behaviours.** DoNothing carries `AnimatorEventSender` (enter `OnEndFullAnim`) and `AnimatorLayerWeightCtr` (fadeout 5), as bound in [reaction presentation](REACTION_PRESENTATION.md).
- **Higher layers.** Layers 2 `UpbodyOnly` and 3 `LArmOnly` (override) and 4 `Arm` (additive) can act over FullBody within their masks.

Neither event time may be taken from the tutorial's 1.1 s, from a clip's first frame, or from summed clip lengths. An exit threshold starts a blend; it is not the frame on which `OnStateExit` is delivered.

## Integration rule, once qualified

The transient owner keeps the selected plant and advances the qualified composed clock. It commits the existing atomic harvest only on the qualified Herb-exit update, and releases the mask on the update after `GatherEnd` or on cancellation. A capacity failure stays retryable, grants never duplicate, and NPC clocks stay independent. A delayed invisible lock in place of this would be another unqualified adapter.
