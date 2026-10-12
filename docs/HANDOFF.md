# Rebuild handoff — October 12, 2026

## Current checkpoint

Claude continued after the AnimFollow checkpoint with commits `f2c7b9a` and
`adb377d`: completed the original female reaction bake, staged its complete
25-clip group and added optional sandbox actor/defence loading validation.
Checkpoint `ca92709` completed [SprintSwordAttack source root extraction](audit/SPRINT_ROOT_CHANNELS.md)
without another original-game launch. Checkpoint `09f2d53` added the
[native reaction preview](audit/REACTION_PREVIEW.md). This continuation stages
matching Knife33 drawn/stowed25-clip attachments and fixes the user-reported
single-click abrupt ending with [native150ms recovery](audit/KNIFE_RECOVERY.md).
Sprint/reaction admission remains closed.

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
The base group holds wood sword attachments. Newly regenerated Knife33
attachments are separate in `rebuild/build/knife-reaction-01-drawn` and
`...-stowed`, from unchanged `export_baked_knife.py`, closed reaction01, source
knife metadata and matching body. They are uninstalled. Private fixture
`knife-recovery-captures-02` validates their exact25 clocks and blade geometry;
all staged hashes agree before/after the capture.

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

First collect the user retest: restart the rebuilt game, make one click and
wait while standing, then walking forward. The user reported the abrupt end
in that exact input pattern. The renderer had dropped the attack immediately
at mechanical EndAction; its new150ms inactive recovery is a labelled native
adapter, not the original Animator transition/layer solve. Damage, root, combo
and stamina clocks are unchanged. Tests cover shared pose weights, pause,
instance binding, expiration and cancellation. Host cancellation clears
presentation too; all tail state is transient. Live visual feel remains a
user retest, separate from the inspected private idle/walking sheets.

The private-display fixture is completed. Final evidence is
`rebuild/build/reaction-captures-04`: actual sandbox actor GPU load,25 clip
sheets,100 pose-clock records, input/output hashes and exit0. All sheets were
reviewed, sprint/get-up in detail. First diagnostic capture01 read the prior
frame; it is unqualified. The final renderer draws twice and checks each
readback token. Captures02/03 are intermediate. No full sandbox-host startup or
live dispatch was tested. Reproduce with `reaction-render-capture` and a fresh
`REACTION_CAPTURE_OUTPUT=build/reaction-review-01`. Read the linked audit for
prerequisites and limits; never adopt an unrelated display.

Next implement a distinct sprint root reader and retained outgoing-interval
routing, with the separately staged matching Knife33/body/defence presentation
and shared
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
Only the native graphics fixture was launched on its owned private display.
No original-game launch, desktop input, save or installed asset change
occurred. The C fixture owns and unloads its actor, scenes, capsules and GPU
render target; the actor now owns unsaved natural-end recovery, and the host clears it on
explicit cancellation. Core combat and save format are unchanged. Staged input hashes agree
before/after the final capture. Current full-check/sanitizer and26 CLI results are in PLAYTEST. The first
full-check build failed in the extracted consume-host fixture because it lacked
the new actor cancellation context; the tracked extractor/mock were corrected. The equipment-host mock also
needed to model the melee setter owning its action binding; focused checks
pass. Final full check passes258 jobs/905 Python tests with87 skips; full
sanitizer passes101 suites and CLI passes26 tests. Focused actor ASan/UBSan
and leak scanning also passes. No generated file was edited by hand. The optimized GPU fixture does not establish driver leak acceptance.
Live reactions/stun/knockback, get-up, shield readiness, pounce/aggression
and Town Defense remain closed. Halt Move/Jump blocking still needs an
explicit user choice. PECSv22 is unchanged. Follow [AGENTS.md](../AGENTS.md)
and [feature contracts](FEATURE_CONTRACTS.md); full check precedes each commit.
