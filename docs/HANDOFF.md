# Current handoff

Updated October 16, 2026. Branch `halt-stop-effect`; the current Gather outcome checkpoint and latest control change are below.

## Current goal

Gather composed-pose qualification. The [outcome observer is now qualified](audit/GATHER_OUTCOME_CAPTURE.md) for one successful original callback: strict validation passed 106 frames, nine event records and one Gather admission. On frame 1214 the HP entry appeared and item 238 ×2 reached new exact bag instance 109 between the pre/post callbacks. This is bounded observation under llvmpipe, not native gameplay integration.

The earlier startup diagnosis read a stale shared log. The actual isolated log reports missing Steam initialization; source explains the resulting Quit/self-kill. Starting Steam normally allowed Adventure to load. A subsequent NVIDIA graphics fault was diagnosed separately, then the bounded outcome capture used software rendering. The probe's frame timestamp was refreshed after post-observer attachment to fix a strict clock-order failure. See the outcome audit for all attempts and limits. The original is closed; all 14 manual-save files matched their baseline hashes.

## Latest control change

Halt now yields to movement/sprint/jump input in both hosts, by user request. The stop effect cancels before the same step applies offsets/admission; other action locks stay authoritative. See [Halt contract](audit/HALT_STOP_EFFECT.md#live-in-both-hosts). Native acceptance after rebuild remains pending. Full check:280 passed/5 sandbox LeakSanitizer failures,1021 Python tests/98 optional skips. Earlier functional reruns of those five suites passed with leak scanning disabled. Full core ASan/UBSan/leak check:113 passed,0 failures/skips. The user explicitly requested commit/push with the known full-check limitation.

## Gather checkpoint

- **Observed in the original:** four Adventure gathers in `gather-live-01`; 1,711 frames and 76 event/action records. User held W during trials 3 and 4; both gave items and movement did not cancel them. Physical keys and inventory were not recorded by that probe.
- **Captured engine poses:** female Tutorial `female-gather-01`, 126 samples across NormalIdle/HerbDown/Herb/HerbUp. Complete group staged at `rebuild/build/player-gather-01`, uninstalled. Both profiles' manual saves stayed unchanged; both games are closed.
- **Qualified bounded replay:** `replay_gather_clock.py` matches 4 cycles/99 frames with zero hash/transition/event-frame mismatches; maximum normalized error 2.0068e-6. This qualifies only the recorded delta domain.
- **Provisional:** entry-pose weight lag fits the diagnostic; pose composition, admission/deferred execution, mask release and general skill timing remain unqualified. The successful outcome capture bounds HP and bag effects within one original Gather callback only.
- **Tooling qualified:** opt-in `reference_gather.py --outcomes` records raw pre/post tree HP, exact bag instances and item notifications. Default OFF. The `live04` capture validates after the timestamp fix; pure WorldPos access remains SHA-bound to inspected installed CIL. Both focused suites pass (28 tests, no skips), CLI passes (26 tests), and the legacy capture still validates unchanged. Exact pre/post-fix inputs are preserved separately.
- **Current commit/push gates:** full `check` passed 285 suites with zero failures; 1,021 Python tests included 98 optional skips. `check-sanitize` passed 113 suites with zero failures or skips. Skips are not passes.
- **Verification:** `check`, `cli-test`, `check-content`, `check-sources` and `check-sanitize` passed for `0163874`. Full check: 285 suites, 1,021 Python tests, 98 optional skips. Sanitizers: 113 suites, no failures/skips. Skips are not passes.

See [capture evidence](audit/GATHER_LIVE_CAPTURE.md), [pose diagnostic](audit/GATHER_ENTRY_POSE_DIAGNOSTIC.md), [next capture plan](audit/GATHER_COMMIT_CAPTURE_PLAN.md) and [numerical evidence](reference/gather-20261015/controller-clock-replay.json).

## Next work

Prepare isolated composed-pose evaluation: static leaf Play/Update(0) cannot establish ALWC timing because it reads Time.deltaTime. Follow section 4 of the [capture plan](audit/GATHER_COMMIT_CAPTURE_PLAN.md#4-layer-weight-boundary-in-an-isolated-clone): retain only audited behaviours, verify behaviour-instance isolation and record both the engine and manual-step deltas. Read source and existing captures first; identify the unresolved question and smallest useful capture before launching. Native immediate herb collection stays unchanged until qualification supports replacement.

Then resume supported equipment draw/stow qualification; oxygen/water-state recovery remains a later survival audit. Keep shield, reactions, pounce and Town Defense gameplay gates closed.

## Outstanding manual checks

- User's Grounder acceptance, especially standing still on a slope.
- Camp repair full-bag return, paid restart and death mid-job: user reserved these for their own session.
- Construction ramp contact/climbing and successful replay while moving remain unqualified.
- Food/starvation original comparison and starvation in ordinary play remain unobserved.

Ordinary native hunt completion/partial reload, food purchase/eating, trade, repair, construction replay, inventory editing and durable-tool discard have bounded acceptance records in [PLAYTEST](PLAYTEST.md). Those records do not establish original difficulty or visual parity.

## Resume safely

Claude worker pane is `w15:p1`; inspect its current Herdr session before dispatch. Only one operator controls desktop input. Check current jobs and PIDs before starting another process; never reuse historical PIDs. No original game or private-display acceptance session was left running at the observer checkpoint. Preserve all ignored build evidence and earned saves.
