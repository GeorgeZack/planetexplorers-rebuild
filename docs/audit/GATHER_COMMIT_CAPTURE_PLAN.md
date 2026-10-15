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

## 6. Prepared opt-in outcome observer (not yet run)

Section 3 is now implemented as offline tooling only. No launch, arm or input has been used. The first capture's exact probe source, DLL, shim, compiler and launch manifest are preserved, read-only and hashed, in the ignored `rebuild/build/gather-probe-provenance-01`. New builds go to `rebuild/build/gather-probe-v2`.

- **Opt-in, qualified installation only.** `reference_gather.py --outcomes` (off by default) sets `PE_GATHER_OUTCOMES=1` and records `outcomes` in the launch manifest; an inherited value is cleared. The flag is refused unless the installed `Assembly-CSharp.dll` has SHA-256 `56d90949e732331be7644fc9185dc088c78ad4888a100f60d96b7aa093f4d7d3`. That is the assembly in which `WorldPos` was verified pure, and the check runs before any build, profile write or launch. The launcher is the supported entry point; setting the environment variable directly bypasses that check and is unsupported. Without the flag the probe's output format is unchanged.
- **Snapshots.** `ReferenceGatherProbe.cs` writes a `pre` snapshot from its existing handler on each `Gather` receipt. It adds a `post` handler only at an end-of-frame sample where `Action_Gather.OnAnimEvent` is already in the recorded invocation list, and records that as an `outcome-attach` row. Each snapshot records:
  - the selected and found tree's raw terrain index, local position, prototype and `WorldPos`;
  - the exact `treeHPInfos` entry for that key, read through the raw `m_Instance` (absent or null stays explicit);
  - every bag item instance by tab and slot, read through the raw `mPackage`.

  `WorldPos` is the one approved pure exception to raw-field reads; its read-only installed IL is recorded in [world-position-readonly-il.json](../reference/gather-20261015/world-position-readonly-il.json).
- **Grants.** The probe's own subscriber on the raw `PlayerPackageCmpt.mEventor` records each successful grant's prototype and count.
- **Safety.** The new rows share the sequence counter, the event and byte bounds, and the observer-error closure. Snapshots are capped at 1,024 bag items. Teardown removes only the probe's own handlers.
- **Analyzer.** `analyze_reference_gather.py` accepts outcome rows only when both the header and the manifest declare outcomes. It requires exactly one pre and at most one post observer role, and a post handler that follows the Gather handler. It reports every `Gather` receipt in a top-level `outcomePairs` list, including receipts that arrive after an action end such as a cancellation, and gives each cycle its share of that list. Each receipt gets one of these statuses: `paired`, `partial-pair`, `invalid-pair`, `nested-gather-unsupported`, `post-not-attached-before-Gather` or `ambiguous-order`. `paired` requires all of: pre and post snapshots carrying that receipt's ID, receipt < pre < post in sequence, the receipt's own frame and time, pre before and post after the Gather handler, and no other `Gather` receipt in between. Each pair also carries the raw HP entries, the grants between the snapshots and the bag differences by unique instance ID. It infers no expected item or quantity. Legacy captures validate unchanged.

Outcome fields remain pending runtime observation. Neither the commit boundary nor any native behaviour is qualified by this tooling.

### Observer checkpoint status

- **What is established.** The probe compiles offline against the pinned installation. All 10 raw snapshot field names are confirmed in that assembly by the root's metadata inventory, recorded in [world-position-readonly-il.json](../reference/gather-20261015/world-position-readonly-il.json). The `WorldPos` helper IL is fully decoded and its bytes are hash-verified.
- **What is not established.** The generic signatures of `mEventor` and `treeHPInfos` were only partly parsed, so the runtime casts are unobserved. This is a compiled and field-name-verified observer, not a runtime-qualified harvest capture.
- **First capture's binaries.** They remain preserved in `rebuild/build/gather-probe-provenance-01`: DLL `74c29a10…`, shim `b0c7f921…`, source `a362c2bf…`.
- **Validation.** The offline `--outcomes` build-only run compiled into `rebuild/build/gather-probe-v2` without any profile write. The legacy `gather-live-01` capture still validates (1,711 frames, 76 events, 4 admissions), and its clock replay still has 0 mismatches. Focused suites: `test_reference_gather` 7, `test_reference_gather_analysis` 21, `test_replay_gather_clock` 10. Gate results are in the table below.

The gates ran with `TMPDIR=rebuild/build/compiler-tmp` on a code snapshot hashed beforehand (`rebuild/build/compiler-tmp/outcome-gates-1/tree.sha256`):

| Gate | Result |
|---|---|
| `make -C rebuild check` | exit0. 285 suites passed, 0 failed. 1,021 Python tests, of which 98 were skipped; skips are not passes. |
| `make -C rebuild cli-test` | exit0 |
| `make -C rebuild check-content` | exit0 |
| `make -C rebuild check-sources` | exit0 |
| `make -C rebuild check-sanitize` | exit0. 113 suites passed, 0 failed, 0 skipped. |

Runtime outcome observation is still pending.
