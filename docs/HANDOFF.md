# Rebuild handoff — October 12, 2026

## Current checkpoint

This round resumed clean checkpoint `c47fcac` (Wentfly/Knocked/GetUp requests).
Completed this round: [installed AnimFollow lifecycle](audit/ANIMFOLLOW_LIFECYCLE.md)
in disconnected `pe_animfollow`, with compiled evidence, exact female prefab
settings, a regeneration exporter, core tests and artifact/layout guards.

Important recovered behavior: view IsRagdoll reads AnimFollow.mActive, not the
wrapper callback flag. The controller runs before the active check and reads
the preceding PD error. Get-up waits for the entry transition to end before
orientation, then for sufficient drive strength and actual limb matching/idle.
The compiled signal uses SetBool(hash,true). OnFallFinished can repeat on every
settled tick before readiness; do not substitute a one-shot event or a timer.
Installed settings override several constructor defaults; see the audit.
The active setter uses RagdollLock, distinct from the IgnoreMe collision-ignore
setting; the exported tag getter is bound to the actual setter literal.

No host connects this owner. All state is transient, no save format changed,
and no game or target assembly was executed. No desktop input was sent, and no
player saves or recovered runtime asset groups were changed. Prior Halt,
hit-IK and ragdoll-action foundations remain disconnected.

Validation results are in [PLAYTEST](PLAYTEST.md); ignored logs are
`rebuild/build/animfollow-check-final.log`, `animfollow-sanitize-final.log`,
`animfollow-content-sources.log` and `animfollow-runtime-build.log`.
The earlier check/sanitize logs precede the final exported tag/API correction.
The first core fixture expected the third-call capture on its fourth call;
its assertion was corrected before full verification. The initial installed
MonoBehaviour typetree read exposed only the common header and rejected full
consumption. The exporter now binds explicit metadata layout to the complete
2,496-byte and 564-byte payloads, without inventing absent tree fields.

## Next useful offline work

Use the reverse-engineer-anything skill for shipped-artifact inspection.
The complete saved REA member inventory is
`rebuild/build/humanphy-AssemblyCSharp-members.json`; its assembly SHA/MVID
match the committed [bounded excerpt](reference/animfollow-lifecycle-il.json).
Reuse it and verify identity instead of making another whole-assembly request.
`cil_disasm.py` can decode its selected methods without loading an assembly.

Next recover the remaining drive in `AnimFollow_AF.DoAnimFollow`, `PDControl`,
`SetJointTorque`, `EnableJointLimits` and `SyncTransforms`: the force/torque
profiles, actual Rigidbody/joint ordering, gain inputs and pose/error feedback.
Those methods are already in the committed excerpt. Bind the installed
female bodies, colliders and ConfigurableJoints separately; the lifecycle
export retains component pointer arrays but does not qualify a physical rig.

The new exporter reads the installed female settings from `resources.assets`
components 324352/324629. Do not use constructor settings or hand-edit outputs.
Its bounded layout uses pinned field metadata/SerializeField attributes,
excludes static/readonly/nonserialized fields, and checks all component bytes.
All tools run from the root; the one-off inventory/decoding scripts under build
are investigation artifacts, not dependencies of regeneration or tests.

An independent alternative is the installed FinalIK solve for Halt/hit-IK
requests. Keep each numerical proof bounded and compare with available captures
before proposing another session. No request owner alone supplies physics,
quaternion alignment, an Animator, an IK solve or a pose.

## Remaining engine/visual work

The `reference_animator_bake.py --preset hit-reaction` batch remains prepared
and unrun: 25 clips / 1,052 samples including get-up and sprint attack. Read
[reference runtime](REFERENCE_RUNTIME.md), state the unresolved pose question,
and follow the supervised capture workflow before a launch. It evaluates
isolated leaves, not live ragdoll blending, reaction layers or get-up admission.
Shield composition stays a separate gate. Sprint root motion needs its own
qualified capture after the pose group exists.

## Standing gates

Live reactions/stun/knockback, shield readiness, pounce/aggression and Town
Defense remain closed. The Halt Move/Jump lock still requires an explicit user
choice. No visible, physical or difficulty parity is established by this
headless controller. Follow [AGENTS.md](../AGENTS.md), the
[feature contracts](FEATURE_CONTRACTS.md) and full `check` before each commit.
