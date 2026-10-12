# Rebuild handoff — October 12, 2026

## Current checkpoint

This round resumed clean checkpoint `eb11d94` (disconnected Halt stop effect).
No separate prior rebuild handoff file was found in the checkout or the usual
local work directories; the top of [sandbox alignment](audit/SANDBOX_ALIGNMENT.md)
provided the next work order.

Completed this round: [female hit IK](audit/HIT_IK.md), a disconnected C owner
and source exporter for the force profile, exact transform membership,
per-part restart and additive FinalIK effector offsets. The left-foot
membership overlap and duplicate links are preserved. All state is transient.
No host connects these requests and no save format changed. No game was
launched, no desktop input was sent, and no original saves or recovered
runtime asset groups were changed.

Validation results are recorded in [PLAYTEST](PLAYTEST.md). Logs are local,
ignored `rebuild/build/hit-ik-check-unsandboxed.log`,
`hit-ik-sanitize.log`, `hit-ik-content-sources.log` and
`hit-ik-runtime-build.log` (all under `rebuild/build`).
The first sandboxed sanitizer attempt failed because LeakSanitizer cannot run
under ptrace. The sandboxed full check likewise failed five existing suites
that launch leak-scanned subprocesses; its failed log is `hit-ik-check.log`.
Use the successful unsandboxed reruns, not those failed attempts.

## Next bounded increment

Continue source-first with `Action_Wentfly`, `Action_Knocked` and
`Action_GetUp` in `Assets/Scripts/PeEntity/Cmpt/Motion/PEAction/Action_Beat.cs`.
Trace the ragdoll/view APIs, callback completion, MotionMgr admission/relations,
multiplayer branches and teardown before choosing a disconnected lifecycle
owner. [Reaction presentation](audit/REACTION_PRESENTATION.md) already records
the four ground get-up clips and their crossed state/clip names. A source
request or callback is not a qualified physical ragdoll or get-up pose.

Alternatively, inspect the installed FinalIK assembly statically to recover
how the Halt and hit-IK offset requests affect the body. That requires the
reverse-engineering skill and a bounded evidence question; do not improvise
an IK solver or enable offsets in a host based on these tests.

The `reference_animator_bake.py --preset hit-reaction` engine batch remains
prepared and unrun: 25 clips / 1,052 samples including get-up and sprint attack.
Before running it read [reference runtime](REFERENCE_RUNTIME.md), identify
what existing source/captures cannot supply and follow the supervised capture
workflow. Keep the unqualified shield-composition work separate.

## Standing gates

Live hit reactions/stun/knockback, shield readiness, pounce/aggression and
Town Defense remain disconnected. Applying the Halt Move/Jump lock requires
an explicit user choice, even though its rules are source-portable. Sprint
attack admission/timing exist but the pose and root capture remain absent.
Do not infer visual, physical or difficulty parity from numerical tests.
Follow [AGENTS.md](../AGENTS.md) and [feature contracts](FEATURE_CONTRACTS.md)
for every increment; full `check` is required before each code commit.
