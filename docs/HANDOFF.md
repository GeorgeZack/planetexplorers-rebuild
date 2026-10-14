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

The next offline increment now supplies [AnimFollow drive and rig requests](audit/ANIMFOLLOW_DRIVE.md):11 source bodies/10 joints,66 nonrigid pairs, exact per-body profiles, collider/material metadata and a disconnected numerical C owner. It preserves PD operator order, prior unclamped error,360-degree history skip, shared damper, joint limits/sync admission and original call cadence. A bounded decoded-CIL float32 replay agrees with259 C force samples; this is adapter math comparison, not an engine oracle. No live reaction or new physical pose is enabled. Full verification is in PLAYTEST.

Next animation work is actual quaternion/target evaluation and physical rig consumption, plus FinalIK. The recovered joint recipe and source identities are in the linked audit/generated metadata. Reuse the saved complete Assembly-CSharp inventory and existing lifecycle excerpt; another broad original launch is unnecessary for source questions. Physical collision/constraint response, friction/default materials, renderer handoff and reaction layer blending remain unqualified. Live reactions/stun/knockback, get-up, shield readiness, pounce/aggression and Town Defense remain closed. Halt Move/Jump blocking still requires the user's explicit choice. PECSv22 remains unchanged.

The FinalIK part of that is now done offline: [the installed full-body biped solver](audit/FINALIK_FBBIK.md) is read from the same saved inventory into the disconnected `pe_fbbik`, with the female rig generated from `FemaleModel.prefab`. It turns Halt/hit effector offsets into a local pose for 25 transforms. A second transcription agrees with C across 84 random cases. One supervised tutorial session then observed the live player's solver for 1,399 frames: replaying its recorded inputs through `pe_fbbik` reproduces the original output to 0.29 mm, with solver weight 1 and every effector and bend weight 0 throughout. It also showed the solver root is the `IK` object (exporter corrected), and the offset writer order. The same capture holds thirteen stops: `pe_halt` reproduces their Halt-only offsets to 2 mm with fitted speed and heading, including shoulder offsets up to 11.6 m that the solver absorbs. October 13: [the grounder](audit/FINALIK_GROUNDER.md) is read from the same inventory into the disconnected `pe_grounder` (root ray, legs, pelvis, spine lean; rays are a caller callback) and agrees with a second transcription over 2,240 updates; the flat tutorial capture cannot qualify it. [Solver poses](audit/PLAYER_IK_POSE.md) for the 31 staged player clips are exported offline from the existing bake captures (`player-ik-pose` group, PEIK1) and loaded by the disconnected `pe_ik_pose`; `pe_fbbik` solves all 991 samples, and without offsets still bends nearly straight limbs by up to 1.9 cm, as the original was observed to do. Still open: applying a solve to the baked skin palettes, an uneven-ground grounder capture, the unobserved Halt mask, the unexercised pin/PullBody/bend-goal paths, and host skeleton mapping. AnimFollow quaternion/target evaluation and physical rig consumption are untouched. All gates remain closed and the Halt Move/Jump choice still waits on the user.

October 13, user-approved step 1: [the zero-offset solve now draws the player in both hosts](audit/PLAYER_IK_DRIVE.md). `pe_ik_drive` blends the staged bone poses of the drawn clips, solves, and `pe_actor_pose_blend_driven` moves each skin joint by its driver bone from the new staged `player-ik-drivers` table (64 parts, 23 groups). It is a 1–19 mm correction, presentation only: capsules and contact queries stay on the baked pose, the arm mapping is off while equipment is drawn, and a dead player or the inventory preview stays baked. The agreed order is this plumbing, then the Halt stop effect (needs the Move/Jump lock decision), then the grounder (needs a supervised uneven-ground capture). While checking, the tutorial host turned out to reject the installed 22-clip sprint body; its clock check now accepts exactly RunSwordAttack then SprintSwordAttack.

October 13, user decision "apply the lock as the original does": [the Halt stop is live in both hosts](audit/HALT_STOP_EFFECT.md#live-in-both-hosts). `pe_halt_host` runs once per fixed movement step: releasing movement on the ground with empty hands starts a Halt, and for 0.22 s (walk), 0.5 s (run) or 0.9 s (sprint) movement and jump requests are dropped. No Halt with equipment drawn, in the air or dead. Its offsets go to the solve, so run and sprint stops show a lunge. The lock is source-derived, not observed; earlier notes in this file that the choice "still waits on the user" are superseded. Remaining from the agreed order: the grounder, which needs a supervised uneven-ground capture.

October 13, unsupervised follow-up: the Halt mask also refuses a herb Gather request in both hosts, silently, as the source's refused `DoAction(Gather)` does; the source never requests PickUpItem for the player, so hare-drop pickup stays independent. [The uneven-ground grounder capture is prepared](audit/FINALIK_GROUNDER.md#prepared-uneven-ground-capture): an observation-only probe on the grounder's own pre/post delegates, its launcher and a replay analyzer, compiled against the installed assemblies and tested on synthetic captures, not yet run in the game. The grounder's ray layers are now named from source (terrain, buildings, static scenery, trees; not Player or Ragdoll). Next: the supervised session, then host composition of the grounder.

October 13, supervised: [the grounder was observed on Adventure terrain](audit/FINALIK_GROUNDER.md#observed-on-uneven-ground) and `pe_grounder` agrees to 0.2 mm and 0.23° over 1,453 frames. The game rewrites the foot rotation limit (curve of ground angle) and spine bend (spine mask) while playing; that capture recorded them only at attach, so they are fitted. The probe now records both per frame. A host needs both, and the same mask zeroes the solver's arm mapping weights. Rays hit only voxel terrain.

October 13, follow-up without a launch: [the foot rotation curve and both setting writers are recovered](audit/FINALIK_GROUNDER.md#settings-written-while-playing) (`pe_grounder_foot_rotation_limit`, `pe_grounder_set_live`). Still host inputs: contact normals for `pe_human_phy`'s ground angle, and the spine mask from `Action_Tool`/`HeavyEquipmentCtrl`. The grounder stays disconnected.
