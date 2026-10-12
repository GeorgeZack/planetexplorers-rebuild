# Rebuild handoff — October 12, 2026

The user accepted the single-click standing/walking Knife ending (“feels
good!”). [The150ms natural recovery](audit/KNIFE_RECOVERY.md) remains a native
presentation adapter; mechanical contact/root/combo clocks are unchanged.

The next animation item is now implemented: grounded female third-person
Knife33 sprint, source skill20110013/clip401 and ordinary Attack7–9
continuations. [The sprint audit](audit/SANDBOX_SPRINT_KNIFE.md) records owners,
assets, exact clocks, installation backups and bounded native acceptance.
Installed scenes are22 body/Knife clips and31 defence clips; the separate
33/42 Wood Sword stage and full25-clip reaction group remain uninstalled.
The distinct root reader fails closed; only a complete matching supported
Knife group enables readiness. Missing groups still reject before debit.

Local stages/baselines/backups are preserved under `rebuild/build` and must
not be removed. `sprint-player-06` regenerates from `sprint-base-01` and the
qualified body/Knife/defence reaction stages; complete replaced installed
folders are in `sprint-install-backup-01`. No source Unity input was edited.
No original client or desktop input was needed for this increment.

Actual exported-asset C fixtures passed open/wall collision, four-stage combo,
blade/defence clock ordering and stamina. The normal production sandbox's
private-input session observed one sprint start and a visible lunge, with no
new contacts or kills and unchanged baseline. It returned to main menu before
explicit supervisor termination-15. Full checks and their skips are recorded
in [PLAYTEST](PLAYTEST.md); do not describe skipped assets as passed.

For a user retest, restart the rebuilt sandbox, draw Knife with LMB, hold
Shift+W until sprinting, then make a fresh LMB press and wait. Existing
camera-facing-attacks settings affect requested heading. Broader sprint
combat/difficulty/reload and full original transition parity remain untested.

Next offline animation work remains AnimFollow PD/body/joint and FinalIK:
reuse `rebuild/build/humanphy-AssemblyCSharp-members.json` and the committed
[AnimFollow lifecycle excerpt](reference/animfollow-lifecycle-il.json).
`DoAnimFollow`, `PDControl`, `SetJointTorque`, `EnableJointLimits` and
`SyncTransforms` are the next source targets. No owner yet supplies a physical
rig/pose. Isolated engine-evaluated reaction/get-up leaves and the
[private preview](audit/REACTION_PREVIEW.md) do not qualify reaction layers or
ragdoll blending. Live reactions/stun/knockback, get-up, shield readiness,
pounce/aggression and Town Defense remain closed. Halt Move/Jump blocking
still requires the user's explicit choice. PECSv22 remains unchanged.
