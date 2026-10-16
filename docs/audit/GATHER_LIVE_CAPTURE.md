# Gather live observation `gather-live-01`

This note records the first Lane B capture defined in the [Gather qualification plan](GATHER_QUALIFICATION_PLAN.md). The plan stays as the historical planning checkpoint (commit `c4ab008`), and this note does not rewrite it. It records how the capture was made and the counts it produced. The director validated the recorded frame hierarchy and action/event edges offline. These observations do not establish a harvest-commit frame or a native schedule.

## Run identity

- **Date and client:** October 15, 2026. The Steam-installed native Linux client, Unity 5.2.4f1, started through `rebuild/tools/reference_gather.py --launch` with normal Steam initialization and the launch-only shim preload. No original files were patched and no assets were installed.
- **Profile:** `~/.local/share/planet-explorers-sandbox-reference`, Adventure manual slot1 (`audit`).
- **Bounds:** 3,600 frames, 120s capture and a 900s arm wait.
- **Hashes recorded by the launch manifest `gather-live-01.launch.json`:**

  | Input | SHA-256 |
  |---|---|
  | Probe source `ReferenceGatherProbe.cs` (also in the capture header) | `a362c2bfbfe9d8dae24c4c930bcdadde8c9f88b464e8823c7e902cf3d773e949` |
  | Probe assembly | `74c29a1025658f304d09e73f1f4000642fa480fc7b4a0027415fafe561d5215e` |
  | Shim | `b0c7f921a55240eac476427ddbf09b4eaa6defb0d19056703626813f16e81cb8` |
  | `UnityEngine.dll` | `f728161940fbb8f27659c979bfcabe3f544002858779abfaa1143065926c051b` |
  | `Assembly-CSharp.dll` | `56d90949e732331be7644fc9185dc088c78ad4888a100f60d96b7aa093f4d7d3` |
  | `sharedassets1.assets` | `d9c65f820a2186116d9eb73d815fe07d9f46924ddfd103ee9510bf254f9a3d67` |

- **Operator:** the user was the only input operator. The assistant created only the fresh `.arm` and `.stop` files and sent no game input.

## Save preservation

Before launch, all 14 files in the profile's manual slots `save0` and `save1` were hashed into the ignored file `rebuild/build/gather-probe/manual-checkpoints-pre-launch-20261015T150726Z.sha256` (file SHA-256 `97201ff2f9368e4cfe524a401d882469f83a747b8abd334443b3abe660b8d9b2`). Autosave slots are excluded, as for other capture baselines. After the client exited, `sha256sum -c` matched all 14 files and the file count was unchanged. The user reports quitting without saving.

## Capture

- **Output:** `pose-captures/gather-live-01.jsonl`, 112,494,072 bytes, SHA-256 `2160e34155fef0a4b13a95111cc88c74d0e75b7edaf19017209b15cee8ede2a3`. Every record parses as JSON.
- **End:** the capture closed itself with `{"type":"end","reason":"capture-limit","frames":1711,"eventRecords":76}`, the 120s limit, shortly before the requested stop file was created. Player.log shows observer attachment, `Capturing player poses` and `Finished: capture-limit; frames=1711`. It has no observer-error or unsubscribe-error line.
- **Frames:** 1,711 end-of-frame samples, Unity frames 9047–10757, about 14.3 samples per second.
- **Subscription record:** one record, at frame 9047. At that point the animation-event handlers were `Action_Rotate.AnimEvent`, `Action_Move` (`PEAction.OnAnimEvent`) and the observer. `Action_Gather` had not subscribed yet.
- **Event records:** 76 in total, covering animation events, action-admission requests and action-end notifications. Four complete Gather cycles were recorded. Each has an admission request for action32, a `Gather` animation event, a `GatherEnd` animation event and an action32 end notification. Two of the cycles also contain an EquipmentHold put-off sequence ending in `PutOffAnimEnd` and an action13 end notification.
- **Handler order:** at every recorded Gather-cycle event the order was Rotate, Move, observer, `Action_Gather` (with `Action_HandChangeEquipHold` after it once that action subscribed). Event-time state in these records was therefore read before the Gather handler ran.
- **Process exit:** the launcher's exit record `gather-live-01.exit.json` reports return code −9 (signal 9) at `2026-10-15T15:15:47Z`. No client process remained. This note does not attribute the signal to a particular cause.

## User-reported trials

The trial descriptions below are the user's own account. **Input key states were not captured**, so they are not raw key telemetry. The trials were:

1. Knife stowed, gather with E.
2. Knife drawn, gather with E.
3. and 4. Gather while holding W.

For gathers 3 and 4, the user reports that W was held during each gather, that both gathers produced items, and that holding W did not cancel either gather.

The action records relate to that account at the action level only:

- **Gather 3:** the Move (action1) admission request and its end notification fall on frame 10258, the same frame as the Gather admission request. The Gather end notification is at frame 10277, and the next Move admission request is at frame 10278.
- **Gather 4:** the next Move admission request is at frame 10439, after the Gather end notification at 10438.

The initial admission frame and the window in which Gather is executing with its mask set must be kept apart. At the end-of-frame sample on 10258, Move is already stopped and Gather is executing with its mask set. This is consistent with movement being refused while Gather remains active; the probe does not record rejected input attempts. Inventory contents were not recorded by the probe. This note makes no claim about which items were produced, their quantities, or the frame on which the harvest committed.

## Offline frame and event validation

`analyze_reference_gather.py` is an offline reader; it neither launches the game nor predicts Animator behavior. It checks bounded JSONL, duplicate JSON keys, clocks and frame/event ordering, subscription identity, handler ordering, action-state shapes, successful closure and exact terminal record counts. The sibling launch manifest must match the probe source digest, capture path and frame bound; the report hashes both inputs. It reuses `analyze_reference_pose.validate_frame` for every recorded pose hierarchy. Its synthetic tests distinguish admission from execution, absent and reordered Gather handlers, incomplete action cycles, malformed hierarchy and failed or truncated closure.

The raw capture above passes: 1,711 hierarchy frames, 76 event records and four Gather admissions. The maximum TRS/hierarchy matrix discrepancy is approximately 0.000444599, within the existing validator's world-offset tolerance. The ignored report is `rebuild/build/gather-probe/gather-live-01-analysis-bound.json`; regenerate it to a fresh path with:

```sh
python3 rebuild/tools/analyze_reference_gather.py \
  /home/admin/.local/share/planet-explorers-sandbox-reference/pose-captures/gather-live-01.jsonl \
  --report rebuild/build/gather-probe/gather-live-01-analysis-new.json
```

| Trial | Admission frame | First executing end-of-frame sample | PutOffAnimEnd frame | Gather receipt frame | GatherEnd receipt frame | End notification / first cleared-mask sample |
|---|---:|---:|---:|---:|---:|---:|
| 1 | 9726 | 9726 | — | 9743 | 9748 | 9749 |
| 2 | 10091 | 10100 | 10099 | 10120 | 10126 | 10127 |
| 3 | 10258 | 10258 | — | 10272 | 10276 | 10277 |
| 4 | 10403 | 10412 | 10411 | 10431 | 10437 | 10438 |

“Executing” here means the first sampled running action32 with Gather mask set and both `waitingRelation` and `doActionPending` false. It is an end-of-frame observation, not the exact callback timestamp of `DoAction`. Trials 2 and 4 retain their pending Gather through the put-off event and first execute on the following update. In all four trials, the mask remains set at the `GatherEnd` callback and at that frame's end-of-frame sample, then clears at the next recorded frame. The observer precedes `Action_Gather.OnAnimEvent` in each named receipt; these are **before-Gather-handler** states, not post-harvest states.

The sampled player-root world position is identical from admission through the final active sample in each trial (maximum recorded displacement 0m). The captured player Animator has `applyRootMotion=false`. This establishes no sampled player-root translation for these four sequences; it does not remove body motion within the model or qualify other root-motion application paths.

## Limits

This capture provides observed evidence only. It does not qualify the native Gather schedule, the Unity state-exit rule, transition blending, pose composition or the harvest-commit frame. The separate Lane A leaf-pose bake and a source-derived scheduler replay still need their own qualification; event receipts and leaf poses do not establish the skill commit boundary.

## Lane A leaf-pose bake `female-gather-01`

This section records the separate isolated clip bake on the qualified female rig. It is a leaf-pose result only. Its sampler drives one clip at a time and has no controller attached (`runtimeAnimatorController=null`, `fireEvents=false`, `applyRootMotion=false`). It therefore carries no transitions, layer composition, IK, root motion or state-exit events.

**Plan.**

- `inventory_reference_bake.py` adds HerbDown267, Herb273 and HerbUp280 to its catalog. It requires each to be a direct, speed-1 leaf of `FullBody.Gather.<name>` that references exactly its clip.
- `reference_animator_bake.py` adds a `gather` preset (NormalIdle plus the three clips). It rejects the Herb clips for a male player and for a mismatched controller state.
- `ReferenceAnimatorBake.cs` `Allowed` adds the same three names.
- The regenerated plan `rebuild/build/animator-bake/clip-plan-gather.json` (SHA-256 `2a973b864efa323c04edda706c5a6d92508161ddea9b97120ed04cddc7e3a9e3`) is bound to `sharedassets1.assets` `d9c65f82…`.
- The male controller does not override these clips, but the preset is still female-only.
- The installed lengths give 62+18+26+20 = 126 planned samples at 30Hz.

**Run.**

- The run used the dedicated profile `~/.local/share/planet-explorers-bake-reference`, Steam initialization and the launch-only shim.
- The user loaded the female Tutorial and reported ready. The assistant created only the fresh `.arm` file and sent no input.
- The output `pose-captures/female-gather-01.jsonl` is 3,024,404 bytes, SHA-256 `f1e1045a31ea6ce387dcc7af97168931900f89e54ab88245e0b8fbfa6b90d827`.
- It ended `{"type":"end","reason":"complete","samples":126}` with 4 clip records.

  | Clip | Source ID | Samples |
  |---|---|---|
  | NormalIdle | 684 | 62 |
  | HerbDown | 267 | 18 |
  | Herb | 273 | 26 |
  | HerbUp | 280 | 20 |

- Player.log shows four retained graphs, `Finished: complete; samples=126` and `Post-cleanup alive120frames`.
- The exit record reports return code −9 (signal 9) at `2026-10-15T15:19:40Z`, after the user quit. No cause is attributed.
- All 7 manual-slot files of the bake profile matched their pre-launch hashes afterwards (`rebuild/build/animator-bake/bake-profile-manual-checkpoints-pre-gather-20261015T151333Z.sha256`).

**Strict analysis.**

```sh
python3 rebuild/tools/analyze_reference_bake.py \
  ~/.local/share/planet-explorers-bake-reference/pose-captures/female-gather-01.jsonl \
  --rig rebuild/build/reference-player-starter/player-female-starter-rig.json \
  --report rebuild/build/animator-bake/female-gather-01-report.json \
  --export-dir rebuild/build/animator-bake/player-female-gather
```

- The analyzer reports `Validated 126 samples / 4 clips; motion detected=True; missing bones=0`, with a maximum hierarchy matrix error of 1.0068e-6.
- Every clip changes all 42 mapped bones, and the two sample-count calculations agree.
- The maximum bone-position change from each clip's first pose is 0.0101m for NormalIdle, 0.9012m for HerbDown, 0.2834m for Herb and 0.9007m for HerbUp. These are body trajectories in model space, not player-root travel.
- No exact repeatability comparison against earlier NormalIdle bakes was run.

**Staged group (not installed).** `rebuild/build/player-gather-01` was produced by the same per-tool exporters, with the same arguments, that `export_directional_player.export` uses:

- `analyze_reference_bake.py` for the body;
- `export_baked_knife.py` for the knife, stowed and drawn;
- `export_baked_shovel.py` and `export_baked_pickaxe.py`, each drawn and stowed;
- `export_baked_axe.py` for the safety axe and the wood axe;
- `export_baked_torch.py` for the torch, held and stowed;
- `export_player_defence.py` for defence, using only this capture.

It contains the body plus all ten attachment scenes of the directional group and a 4-clip defence actor (16 capsules, 23 joints, maximum point error 1.45e-7m).

- Every attachment passes `check_body` against the body's exact clocks, and the defence clocks equal the body's.
  - NormalIdle: 62 frames, 2.0000153s, looping.
  - HerbDown: 18 frames, 0.5333347s.
  - Herb: 26 frames, 0.8000011s.
  - HerbUp: 20 frames, 0.6333332s.
- The defence `.pehb` is byte-identical to the installed `reference-assets/player-defence` file.
- `gather-manifest.json` (schema `pe-gather-player-stage-1`, `installed: false`) records 66 file hashes, the capture, launch, exit, rig and plan hashes, the exporters used and the limitations. The directional exporter only accepts its own fixed clip names, so the manifest was written by a short inline script; no exporter was changed.

**Status.** The female HerbDown, Herb and HerbUp leaf poses are now qualified as isolated engine samples, together with complete staged attachment and defence groups on the same clocks.

Still open:

- Scheduler and frame replay against the recorded receipts.
- Transition blending and layer composition against the live poses.
- The skill-commit boundary.
- Host loading and integration.

The native immediate herb harvest remains the declared adapter, unchanged. No asset was installed and no host loads this group.

## Stable live-pose comparison

The director reused `compare_reference_bake.py` against every stable matching frame in the Adventure capture, comparing all 42 mapped body bones. The ordinary pose reader does not accept synchronous event rows, so an ignored derived view keeps the original `header`, `humanTraits`, `frame` and `end` lines unchanged and omits only the subscription and 76 event records. The raw capture remains untouched and passed its separate event validator. The projection manifest is `rebuild/build/gather-probe/gather-live-01-eof-view-manifest.json`.

[The committed numerical report](../reference/gather-20261015/stable-pose-comparison.json) preserves every compared frame and every bone's maximum and RMS positional error, matrix error, source hashes and limitations. These are comparison measurements, not recovered mesh or pose assets. The full ignored reports are `rebuild/build/gather-probe/compare-{HerbDown,Herb,HerbUp}-live01.json`.

| Clip | Stable frames | Mapped bones | Maximum positional error | Worst bone |
|---|---:|---:|---:|---|
| HerbDown | 12 | 42 | 0.204876699m | Bone L Forearm / Bip01 L Forearm |
| Herb | 17 | 42 | 0.039838444m | Bip01 L Calf |
| HerbUp | 6 | 42 | 0.024255067m | Bip01 R Foot |

The largest HerbDown discrepancies occur at the first end-of-frame sample after each entry blend reports complete: frames 9728, 10102, 10260 and 10414 (approximately 0.1856–0.2049m). Later stable HerbDown samples in the same trials have smaller maxima, approximately 0.0088–0.0321m. No boundary frame or bone is excluded from the report. The cause is still provisional: the live poses include controller composition and procedural IK, while the isolated bake does not. These measurements do not prove a one-frame delay or identify a specific solver defect.

The next offline goal must explain the boundary discrepancies as well as replaying current/next states and event receipt frames. Leaf recovery succeeded; live composition remains unqualified.

## Offline FullBody controller-clock replay

`rebuild/tools/replay_gather_clock.py` replays the source FullBody Gather state clock against this capture. It runs offline: no launch, no input, no native code.

### Inputs and provenance

- **Controller values** come from the clip-plan inventory that owns them. `inventory_reference_bake.gather_controller` now records the entry, the leaf chain and the exit:
  - the FullBody layer;
  - the motionless `FullBody.DoNothing` idle;
  - the `Gather`-conditioned entry: duration 0.1, not fixed, no exit time;
  - the shared `FullBody.Gather` entry and exit selectors;
  - the HerbDown, Herb and HerbUp transitions;
  - every state's name and full-path hash.

  It requires speed 1, cycle offset 0, no speed, cycle-offset or mirror parameter, and no mirroring. The record was regenerated into the fresh ignored plan `rebuild/build/animator-bake/clip-plan-gather-02.json` (SHA-256 `d934935d6bd66c7a083219c1412d6ad2494e67932f0fdfbb1ffbb0db9d957173`). The bake's original `clip-plan-gather.json` (`2a973b86…`) is unchanged.
- **Plan checks.** The replay requires the plan's asset hash to equal the launch manifest's `asset_sha256`. Each Herb clip's ID must equal the inventory's `GATHER_CLIPS`, and its recorded transition evidence must equal the controller record. All inputs are size-bounded and strictly decoded, so duplicate keys and nonfinite values fail. The validated clock fields and scalar flags (speeds, cycle offsets, offsets, motion, mirror, exit-time and fixed-duration flags) are type-checked, so a Boolean cannot stand for a number or the reverse. Other JSON fields, such as `schema`, are compared by value only.
- **Idle contract.** The DoNothing contract is reused from `rebuild/generated/pe_reaction_presentation_source.json`: `AnimatorEventSender` with enter event `OnEndFullAnim`, and `AnimatorLayerWeightCtr` with fadeout 5. Its `sharedassets1.assets` and `Assembly-CSharp.dll` hashes must equal the capture's launch manifest.
- **Capture.** The capture is validated by `analyze_reference_gather.analyze`. The replay reads only the unique FemaleAnimator2/FemaleAvatar player animator.

### Rules

**Source-derived.** Clip lengths. Exit times and durations multiplied by the source state's length. The resulting timeline in seconds from time zero is below. The one exception is the entry end: 0.1000000015 s combines the *serialized* normalized entry duration (0.10000000149) with the *captured* rule that the motionless DoNothing state lasts 1 s. Every other point uses only source clip lengths and controller values.

| Point | Seconds |
|---|---|
| Entry end (serialized 0.1 × captured 1 s idle length) | 0.1000000015 |
| Herb starts | 0.2833347122 |
| HerbDown exit completes | 0.5333347162 |
| HerbUp starts | 0.8333352606 |
| Herb exit completes | 1.0833358566 |
| HerbUp-to-DoNothing starts | 1.2166684891 |
| HerbUp exit completes | 1.4666684856 |

**Captured-engine rules, read from the capture and not derived from source:**

1. Time zero is the first executing sample, where HerbDown is the **next** state at normalized time 0.
2. The initial DoNothing normalized phase is copied from that sample as an initial condition. The replay compares the later phase; it does not predict the phase itself, nor the deferred start.
3. A motionless state advances one normalized unit per second.
4. A transition starts exactly at its source threshold, and the destination clock starts there.
5. A threshold counts as crossed on the first update whose accumulated recorded delta reaches it.
6. Exit events arrive on the update that completes a transition; the idle enter event arrives on the update that starts one.

### Result

```sh
python3 rebuild/tools/replay_gather_clock.py \
  ~/.local/share/planet-explorers-sandbox-reference/pose-captures/gather-live-01.jsonl \
  --report rebuild/build/gather-probe/gather-live-01-clock-replay-NEW.json
```

The tool prints `4 cycles, 0 mismatches; qualified=True (observed delta domain only)`. The canonical committed numerical report is [controller-clock-replay.json](../reference/gather-20261015/controller-clock-replay.json), SHA-256 `04a2dde4faaaafdfd8c93a593e1182211ea5a1628687d6792af40b8fdeb6ee5a`. It holds numbers and hashes only, with no poses or assets. It is byte-identical to the ignored original run output `rebuild/build/gather-probe/gather-live-01-clock-replay-110427.json`. An independent root run reproduced the same result: 4 cycles, 0 mismatches, 99 frames, maximum error 2.0068e-6, plan `d934935d…`.

- **Frames compared:** all 99 sampled frames from each first executing sample through its end notification. On each frame the replay compares the current and next short and full-path hashes, the transition flag, and both normalized times, against a fixed tolerance of 1e-5. The maximum normalized-time error is 2.01e-6.
- **Receipts:** every `Gather`, `GatherEnd` and `OnEndFullAnim` receipt frame is predicted exactly.

  | Trial | First executing frame | `Gather` | `OnEndFullAnim` | `GatherEnd` | End notification |
  |---|---|---|---|---|---|
  | 1 | 9726 | 9743 | 9744 | 9748 | 9749 |
  | 2 | 10100 | 10120 | 10122 | 10126 | 10127 |
  | 3 | 10258 | 10272 | 10274 | 10276 | 10277 |
  | 4 | 10412 | 10431 | 10432 | 10437 | 10438 |

- **Margins:** no update crossed more than one threshold. The smallest margin to a threshold is 3.85e-5 s: trial 1's DoNothing enter at frame 9744. A float32 accumulation difference of that size would move that one decision by a frame. This is a margin, not a fitted value.
- **Delta domain:** recorded deltas range from 0.01352 to 0.11407 s.

### Limits

- **Clock only.** This qualifies the FullBody controller **state clock** only, and only within that recorded delta domain. It does not qualify action admission, the deferred `DoAction` start, Gather mask release, poses, layer weight or the harvest commit.
- **Outside the recorded domain.** Larger steps, updates crossing several thresholds, interruption, cancellation and other speeds remain unqualified, although synthetic tests exercise them.
- **Clip info.** Clip-info arrays can lag a completed blend, so state hashes and the transition flag are compared instead.
- **Pose application.** The first-stable pose discrepancy and its supported, provisional weight-application-lag hypothesis are recorded separately in [GATHER_ENTRY_POSE_DIAGNOSTIC](GATHER_ENTRY_POSE_DIAGNOSTIC.md). The replay does not use it.

### Tests

- `rebuild/tests/test_replay_gather_clock.py` (10 tests) covers:
  - source timeline arithmetic and inclusive threshold crossings;
  - a model-consistent trace, the tolerance boundary and state mismatches;
  - missing, shifted and extra receipts;
  - gapped and incomplete cycles, and overlapping transitions;
  - malformed or clock-changing plan metadata, duplicate and nonfinite JSON and size bounds;
  - asset, assembly and idle provenance tampering;
  - ambiguous or changing player animators.
- `test_reference_animator_bake.py` adds the controller-record guards for cycle offset, speed parameter, idle speed, fixed duration, entry exit time, selectors and chain destinations.


The subsequent [October 16 outcome capture](GATHER_OUTCOME_CAPTURE.md) qualifies one successful callback's HP and exact bag effects. The isolated layer-weight boundary remains planned in [GATHER_COMMIT_CAPTURE_PLAN](GATHER_COMMIT_CAPTURE_PLAN.md).
