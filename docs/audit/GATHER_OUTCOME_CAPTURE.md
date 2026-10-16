# Gather outcome observer qualification

October 16, 2026. **Observed in the original:** one successful bare-hands Adventure Gather changed herb HP and granted an exact bag instance between the observer immediately before `Action_Gather.OnAnimEvent` and the observer immediately after it. This qualifies the outcome observer for that callback. Native immediate herb collection remains an adapter; no gameplay or asset group was enabled.

## Session and provenance

- Steam-installed native Linux V1.1.4, Unity 5.2.4f1, normal Steam initialization, isolated `planet-explorers-sandbox-reference` profile, preserved Adventure Normal manual slot 1 (`audit`, female).
- `reference_gather.py --launch --outcomes --single-threaded-rendering`, 1,800-frame/60-real-second capture bounds and a 900-second arm wait. The probe remained observation-only.
- Process-only software rendering: Mesa 26.2.2-arch1.1, OpenGL 4.6 Compatibility Profile, llvmpipe LLVM 22.1.8. The window was resized to 800×450 before loading. CPU terrain and disabled MSAA used the established isolated launcher settings.
- Captured third person, hands stowed. Requested preparation inputs were D for 4 seconds, W+D for 1.2 seconds, W for 2 seconds, then W+D for 0.3 seconds. After stopping and arming, one E press lasted 0.25 seconds. No movement command followed arming. These are issued commands, not physical-key telemetry.
- Raw capture: the profile's `pose-captures/gather-outcome-live04.jsonl`, SHA-256 `02b53f817c117d0f7144f025304663c3caeb6a695f6093d3b27a47cdacdb517a`. Its sibling launch and exit manifests remain preserved. Exact source, assembly, shim and compiler inputs are retained under ignored `rebuild/build/gather-probe-provenance-outcomes-02`.
- [The numerical report](../reference/gather-20261016/outcome-observation.json) is the unmodified strict analyzer output. [Session provenance](../reference/gather-20261016/outcome-provenance.json) records launch/input/renderer details, hashes, handler order, clocks, exit and all 14 unchanged manual-save hashes. Raw poses and recovered assets remain local.

## Observed callback boundary

Strict analysis passed **106 hierarchy frames, nine event records, four outcome records and one Gather admission**, with successful `stop-file` closure. The outcome count is included in the nine event records.

| Record | Frame | Sequence | Observation |
|---|---:|---:|---|
| Gather admission | 1209 | 1 | First executing end-of-frame sample is also frame 1209; post observer attaches there as sequence 2. |
| Gather receipt | 1214 | 3 | Original handler order is Rotate, Move, pre observer, Gather, post observer. |
| Pre snapshot | 1214 | 4 | Raw subterrain instance exists; selected tree's HP dictionary entry is absent. |
| Grant | 1214 | 5 | Original item notification records prototype 238, count 2. |
| Post snapshot | 1214 | 6 | HP entry exists at −1389359.38; new exact instance 109 contains prototype 238 ×2 in tab 2, slot 4. |
| OnEndFullAnim | 1215 | 7 | Recorded separately from GatherEnd. |
| GatherEnd | 1216 | 8 | Gather end event arrives. |
| Action end | 1217 | 9 | Original action 32 end notification arrives. |

Both snapshots identify the same prototype 15 tree at world position (−563.6862, 21.4523811, 349.0192), terrain index −1. The absent pre-entry is not an observed HP of 255. The post-entry is the raw negative value, not a clamped native value. The grant lies strictly between pre and post in the shared sequence, and the only bag difference is the new instance above. The analyzer labels the receipt `paired` without supplying an expected item or quantity.

This observation resolves the planned question for this successful gather: HP and bag effects occurred within the original Gather handler's callback boundary, without a later frame between the snapshots. It also exercises the previously unobserved dictionary, package-container and grant-event reads against the installed assembly. It does not observe a skill instance or establish its start/finish semantics.

## Qualification limits

Recorded frame deltas range from 0.316823959 to 0.333333343 seconds. The first-to-last sample span is 35.754425 real seconds and 34.402779 scaled seconds. This software-rendered domain is outside the earlier controller-clock replay's qualified domain; it does not extend that replay qualification or establish ordinary rendering performance.

Only one successful gather with available bag capacity was captured. Full-bag loss, cancellation, deferred admission, general mask release, pose composition and native scheduling remain unqualified. The player's reported root transform distance is not controller/capsule motion acceptance. No native gameplay replacement follows from this capture.

After closure the original's System menu, Quit Game and YES confirmation were operated normally. The exit sidecar records return code −9/signal9 at `2026-10-16T16:41:11.759417Z`; no exit-zero claim is made. No original client remained, and all 14 Adventure/Build manual-save files matched their pre-session hashes. Autosaves and preferences are excluded from that claim.

## Startup diagnosis and observer correction

[Startup evidence](../reference/gather-20261016/startup-diagnosis.json) corrects the earlier handoff: it had read the stale October 13 shared log. The actual isolated log reported `SteamAPI_IsSteamRunning()` and `SteamAPI_Init()` failures. **Source-derived:** `SteamMgr.Awake` requests Quit on initialization failure, and `PEMutex.OnDestroy` kills the non-editor process. Starting Steam normally allowed the subsequent Adventure load. A separate native-GL attempt then aborted: its inspected core shows generated `.gl` code reading address `0xc` through NVIDIA 580.178.04, matching the documented graphics fault pattern. This does not establish observer causation or the exact driver-versus-game defect. The extracted diagnostic core copy was removed.

The first software capture (`live02`) validated 361 frames but admitted no Gather. Long E/F5 holds were unsuitable: **source-derived** `ClickPressInfo` accepts release only within its default strict 0.5-scaled-second interval. A later successful collection happened after capture closure and is not callback evidence.

The next capture (`live03`) contained the outcome records but failed strict validation. At frame 1462, `outcome-attach` used real time 311.014526, followed by a frame row using the earlier entry sample 311.0135. [Failure evidence](../reference/gather-20261016/clock-order-failure.json) retains the single reversal and raw hash. The capture is not qualified. Its matching pre-fix inputs remain in `rebuild/build/gather-probe-provenance-outcomes-01`.

**Implemented in tooling:** the frame's real-time sample is now refreshed after attachment writes its receipt. The strict validator remains unchanged. The analyzer's generic limitation no longer embeds the obsolete pending-observation status; it still infers no expected item, quantity or commit rule. Offline compilation, both focused suites (28 tests, no skips), the 26-test CLI gate, unchanged legacy-capture validation and the complete `live04` validation pass.

Before commit/push, full `make -C rebuild check` passed 285 suites with zero failures; its 1,021 Python tests included 98 optional skips. `make -C rebuild check-sanitize` passed 113 suites with zero failures or skips. Skips are not passes.
