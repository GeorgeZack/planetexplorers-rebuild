# Rebuild handoff — October 12, 2026

## Current checkpoint

This round resumed clean checkpoint `30a969b` (female hit-IK requests).
Completed this round: [Wentfly, Knocked and GetUp lifecycles](audit/RAGDOLL_ACTIONS.md)
in disconnected `pe_ragdoll_action`, with a source exporter and tests for
ordered requests, exact relation rows, partial source faults, class-keyed
physics freezes, readiness and deferred force scaling.

The source player's Knocked timer is 2 seconds, overriding the class's 3-second
initializer. On expiry Knocked clears its mask and requests immediate GetUp
but returns false and stays registered until GetUp's relation cancels it.
GetUp immediate dispatch bypasses its normal Knocked+Lie mask requirements.
Do not lose these distinctions when composing a future manager.

All state is transient and unsaved. No host connects these requests and no
save format changed. No game was launched, no desktop input was sent, and no
original saves or recovered runtime asset groups were changed. The preceding
[hit IK](audit/HIT_IK.md) and [Halt](audit/HALT_STOP_EFFECT.md) foundations remain
disconnected too.

Validation results are recorded in [PLAYTEST](PLAYTEST.md). Ignored local logs
are `rebuild/build/ragdoll-action-{check,sanitize,content-sources,runtime-build}.log`.
Full check and sanitizer commands were run outside sandbox supervision because
existing leak-scanned tests fail under its ptrace restriction, established in
the preceding round. An initial focused fixture failure was corrected before
full verification; it confused absent-view start and present-view update.

## Next useful offline work

Read the `reverse-engineer-anything` skill before inspecting installed plugin
assemblies. Recover the bounded lifecycle of AnimFollow `RagdollControl_AF`
and `AnimFollow_AF`: inherited `active`, falling/gettingUp transitions,
`settledSpeed`, get-up readiness and trigger/orientation selection. The
checked-in `PERagdollController` is only a wrapper. In particular the view
reads inherited `active`, not its custom `IsRagdoll()` method. Read the new
audit before assuming any callback removes an action: Motion_Beat's
get-up-finished action-ending branch is commented out.

Then qualify the missing ragdoll bodies/joints and solve, or inspect installed
FinalIK statically for the Halt/hit-IK offsets. Neither a lifecycle request nor
a threshold/helper is a physical ragdoll, an IK solution or a get-up pose.
Keep parser/solver experiments separate from runtime integration until their
actual contracts and evidence are ready.

## Remaining engine/visual work

The `reference_animator_bake.py --preset hit-reaction` engine batch remains
prepared and unrun: 25 clips / 1,052 samples including get-up and sprint attack.
Before running it read [reference runtime](REFERENCE_RUNTIME.md), identify
what source/captures cannot supply and follow the supervised capture workflow.
It evaluates isolated leaves, not reaction-layer/directional composition.
Preserve the unqualified shield-composition work as a separate gate.

Sprint attack admission/timing exist but no pose or root capture is available.
After the prepared bake, stage complete body/held/defence groups and qualify a
separate sprint root capture before opening that branch. Do not borrow the
running root curve or claim source physics from a native controller adapter.

## Standing gates

Live hit reactions/stun/knockback, shield readiness, pounce/aggression and
Town Defense remain disconnected. Applying the Halt Move/Jump lock requires
an explicit user choice, even though its rules are source-portable. No visible,
physical or difficulty parity is established by this round's headless tests.
Follow [AGENTS.md](../AGENTS.md) and [feature contracts](FEATURE_CONTRACTS.md)
for each increment; full `check` is required before every code commit.
