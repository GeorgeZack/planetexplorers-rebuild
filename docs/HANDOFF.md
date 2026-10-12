# Rebuild handoff — October 12, 2026

## Current checkpoint

Claude continued after the AnimFollow checkpoint with commits `f2c7b9a` and
`adb377d`: completed the original female reaction bake, staged its complete
25-clip group and added optional sandbox actor/defence loading validation.
This continuation completes [SprintSwordAttack source root extraction](audit/SPRINT_ROOT_CHANNELS.md)
without another original-game launch. No live animation admission is enabled.

The earlier claim that the reaction bake was unrun is superseded. The capture
`female-reaction-01.jsonl` has25 clips/1,052 poses, a complete ending and a120-frame
post-cleanup window. The reference process ended with signal9; its closure
method was not observed. Do not claim clean teardown. NormalIdle agrees with
capture06; mirroring was evaluated by the engine. These are isolated leaf
poses, not reaction layers or ragdoll-to-get-up blending.

## Staged animation assets

`rebuild/build/starter-pair-reaction-01` contains matching body, drawn/stowed
wood sword, wood shield and defence for25 clips. It is not installed. Existing
33/42-clip groups cannot simply absorb it under the64-clip actor limit.
`pe_sandbox_actor` validates it as an optional second group, all-or-nothing,
with matching clocks and NormalIdle; no code selects or draws these clips.
Host roles and installation mappings are in
[the reaction audit](audit/REACTION_PRESENTATION.md#host-reaction-group).
The group currently holds sword attachments, not a qualified Knife33 sprint
attachment. Preserve that distinction when planning live Knife integration.

`rebuild/build/sprint-source-motion-01` now holds `sprint-root.pesr/json`.
The exporter checks the pinned asset/native ELF and unchanged running408
oracle, applies the exact existing importer/channel predicate to401 and
replays401's starting quaternion through the pinned arithmetic slices.
The 56 dense30Hz knots are source-derived; source EndAction displacement is
about2.70m forward. No observed sprint root playback is claimed.
The one-record PESR1 is separate from the installed four-record ordinary
profile and captured PERM1. The ordinary loader rejects it intentionally.
Regenerate with `export_sprint_root_motion.py --output ...`; use `--check`
for an existing stage. Never hand-edit or overwrite another recovered group.

## Next animation items

First create a private-display presentation fixture for the optional reaction
body/attachments/defence and sprint leaf. It must select the separate group
without altering gameplay admission, use matching clocks and inspect real
rendered poses. Claude's load-only increment has no window/graphics acceptance.
Read [native private input](audit/NATIVE_PRIVATE_INPUT.md) before using its
supervised explicit high display; never adopt an unrelated display.

Then implement a distinct sprint root reader and retained outgoing-interval
routing, qualify matching Knife33/body/defence presentation and shared
collision/contact ordering. Only complete supported assets and integration
checks may justify setting sprint_motion_ready. Do not substitute408 or zero
motion, borrow the ordinary four-record loader or debit stamina on an
unsupported start. A new original sprint root capture is optional
corroboration; available source now answers bounded unblended extraction.

For live reactions/get-up, the remaining numerical work is AnimFollow PD,
body/joint contracts and FinalIK, plus reaction layer/blend qualification.
Reuse the saved REA inventory `rebuild/build/humanphy-AssemblyCSharp-members.json`
and the committed [AnimFollow CIL excerpt](reference/animfollow-lifecycle-il.json).
`DoAnimFollow`, `PDControl`, `SetJointTorque`, `EnableJointLimits` and
`SyncTransforms` remain next offline targets; no request owner supplies a
physical rig or pose. Installed female settings are components324352/324629
in resources.assets, not constructor defaults.

## Validation and gates

Current results and ignored log names are in [PLAYTEST](PLAYTEST.md).
No game launch, desktop input, save or installed asset change occurred in
this continuation. No C code or allocation/ownership changed.
Live reactions/stun/knockback, get-up, shield readiness, pounce/aggression
and Town Defense remain closed. Halt Move/Jump blocking still needs an
explicit user choice. PECSv22 is unchanged. Follow [AGENTS.md](../AGENTS.md)
and [feature contracts](FEATURE_CONTRACTS.md); full check precedes each commit.
