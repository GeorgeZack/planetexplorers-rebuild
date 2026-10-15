# Current handoff

Updated October 15, 2026. Branch `halt-stop-effect`; Gather tooling checkpoint `0163874` is pushed; the latest control change is below.

## Current goal

Gather outcome and composed-pose qualification. Documentation is consolidated: main guides have one owner, obsolete review snapshots/work orders are removed, and source algorithms/unique observations stay in their feature audits. Link/anchor, claim and unchanged-source/evidence checks pass; no game launch or runtime change occurred.

## Latest control change

Halt now yields to movement/sprint/jump input in both hosts, by user request. The stop effect cancels before the same step applies offsets/admission; other action locks stay authoritative. See [Halt contract](audit/HALT_STOP_EFFECT.md#live-in-both-hosts). Native acceptance after rebuild remains pending. Full check:280 passed/5 sandbox LeakSanitizer failures,1021 Python tests/98 optional skips. Earlier functional reruns of those five suites passed with leak scanning disabled. Full core ASan/UBSan/leak check:113 passed,0 failures/skips. The user explicitly requested commit/push with the known full-check limitation.

## Gather checkpoint

- **Observed in the original:** four Adventure gathers in `gather-live-01`; 1,711 frames and 76 event/action records. User held W during trials 3 and 4; both gave items and movement did not cancel them. Physical keys and inventory were not recorded by that probe.
- **Captured engine poses:** female Tutorial `female-gather-01`, 126 samples across NormalIdle/HerbDown/Herb/HerbUp. Complete group staged at `rebuild/build/player-gather-01`, uninstalled. Both profiles' manual saves stayed unchanged; both games are closed.
- **Qualified bounded replay:** `replay_gather_clock.py` matches 4 cycles/99 frames with zero hash/transition/event-frame mismatches; maximum normalized error 2.0068e-6. This qualifies only the recorded delta domain.
- **Provisional:** entry-pose weight lag fits the diagnostic; pose composition, admission/deferred execution, mask release and the skill commit boundary remain unqualified.
- **Tooling implemented:** opt-in `reference_gather.py --outcomes` records raw pre/post tree HP, exact bag instances and item notifications. Default OFF, offline compilation checked; no outcome capture has run. Pure WorldPos getter exception is SHA-bound to inspected installed CIL. Legacy captures still validate.
- **Verification:** `check`, `cli-test`, `check-content`, `check-sources` and `check-sanitize` passed for `0163874`. Full check: 285 suites, 1,021 Python tests, 98 optional skips. Sanitizers: 113 suites, no failures/skips. Skips are not passes.

See [capture evidence](audit/GATHER_LIVE_CAPTURE.md), [pose diagnostic](audit/GATHER_ENTRY_POSE_DIAGNOSTIC.md), [next capture plan](audit/GATHER_COMMIT_CAPTURE_PLAN.md) and [numerical evidence](reference/gather-20261015/controller-clock-replay.json).

## Next work

Qualify the Gather outcome observer in a minimal preserved-profile capture. Prepare isolated composed-pose evaluation separately: static leaf Play/Update(0) cannot establish ALWC timing because it reads Time.deltaTime. Read source and existing captures first; identify the unresolved question and smallest useful capture before launching. Native immediate herb collection stays unchanged until qualification supports replacement.

Then resume supported equipment draw/stow qualification; oxygen/water-state recovery remains a later survival audit. Keep shield, reactions, pounce and Town Defense gameplay gates closed.

## Outstanding manual checks

- User's Grounder acceptance, especially standing still on a slope.
- Camp repair full-bag return, paid restart and death mid-job: user reserved these for their own session.
- Construction ramp contact/climbing and successful replay while moving remain unqualified.
- Food/starvation original comparison and starvation in ordinary play remain unobserved.

Ordinary native hunt completion/partial reload, food purchase/eating, trade, repair, construction replay, inventory editing and durable-tool discard have bounded acceptance records in [PLAYTEST](PLAYTEST.md). Those records do not establish original difficulty or visual parity.

## Resume safely

Claude worker pane is `w15:p1`; inspect its current Herdr session before dispatch. Only one operator controls desktop input. Check current jobs and PIDs before starting another process; never reuse historical PIDs. No original game or private-display acceptance session was left running at the observer checkpoint. Preserve all ignored build evidence and earned saves.
