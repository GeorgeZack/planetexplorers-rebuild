# General Gather qualification plan

This is the October 15, 2026 planning record. Both lanes have been executed: an isolated leaf-pose bake and an observed live controller with its event receipts. The source contract now lives in [GATHER_ACTION_GATE](GATHER_ACTION_GATE.md), the captures, bake and clock replay in [GATHER_LIVE_CAPTURE](GATHER_LIVE_CAPTURE.md), and the remaining capture questions in [GATHER_COMMIT_CAPTURE_PLAN](GATHER_COMMIT_CAPTURE_PLAN.md).

Decisions from this plan that still bind:

- **Clip sampling is not event proof.** The isolated clip sampler sets `runtimeAnimatorController=null` and `fireEvents=false`, so its leaf poses say nothing about transitions, IK or `OnStateExit` timing.
- **The composed clone strips behaviours.** The shield composition clone removes all behaviours and evaluates static `Play`+`Update(0)` poses. It cannot answer event or time-advancing questions without a separate change.
- **Event observation adds its own handler only.** It records the actual invocation list and never primes `PEAction.anim` or reorders delegates. Receipts are labelled before or after the Gather handler. An admission request (`onActionStart`) is kept distinct from deferred execution.
- **Movement during Gather is a refusal record.** It is not a cancellation trial, because source refuses movement while the Gather mask is set.
