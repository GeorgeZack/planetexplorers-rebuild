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

October 13, step 3 of the agreed order: [the grounder now adjusts the drawn player in both hosts](audit/FINALIK_GROUNDER.md#live-in-both-hosts) through the shared `pe_grounder_host` and `pe_ik_drive_solve_grounded`. Presentation only. Rays are each host's own geometry and the ground angle is a single-ray adapter. Headless tests and short rendered smokes pass; **nobody has looked at it in play**, so the next thing it needs is a user look on slopes, steps and built surfaces in the sandbox and the tutorial. The user will check standing still on a slope manually (requested October 14); do not mark it accepted until they report. Hit offsets stay disconnected.

October 14, user-chosen follow-up (option 1): [the grounder's two stand-ins are narrowed](audit/FINALIK_GROUNDER.md#trees-and-the-ground-contact). Trees now catch the foot rays in both hosts (sandbox perception ray; tutorial HoloTree while its collider is enabled, layer 0 read from the prefab), and the ground angle comes from each host capsule's own support contact instead of a ray under the root. Actors stay out. Still owed: the user's look in play, including standing still on a slope.

October 14, next goal after the grounder: [player hunger](audit/HUNGER.md) is recovered from the database and source into the disconnected `pe_hunger` (drain, starvation condition and its stamina/comfort pre-adds, gains for the ten catalog foods the shops already sell). Nothing in play changed. Connecting it needs PECSv23, the starvation pre-adds in the stamina owners, eating through item use and a HUD bar; that is a user decision.

October 14, by user request: [hunger is connected in the Adventure sandbox](audit/HUNGER.md#connected-in-the-sandbox). PECSv23 appends hunger and the starving flag. Starvation doubles stamina costs and the recovery percent through `pe_game_stamina_reduce_percent`; eight catalog foods are eaten through item use; the character panel shows it. The food loop is now observed in ordinary native play (below); starvation in play is not.

October 14, director-chosen goal: [Adventure hunger is visible in the shared HUD](audit/HUNGER.md#hud-bar-and-starvation-notice). `pe_ui_hud` draws the prefab's `5_Hunger` bar (rotated 180°, fills from the right) from an Adventure-only snapshot field; the tutorial and Legacy keep the empty track. Starvation's start shows the original notice text 8000654. Director review and full verification passed; evidence remains a staged render fixture, not ordinary gameplay acceptance. The repeat-on-start notice is a labelled native adapter.

October 14, director-chosen next goal, reviewed for checkpoint: [ordinary native acceptance of the food loop](audit/HUNGER.md#ordinary-native-acceptance-of-the-food-loop). In private session `rebuild/build/food-private-20261014-02`, from the earned completed hunt save (`9a28960b…e0d2`, unchanged): bought three Biscuit I and one Biscuit II from Xing Zhang (money 325→237, stock 35→32 and 20→19); ate from the bag, with a refused retry; dragged to quickslot 4 and ate with key 4, with a refused retry; the live bar followed hunger. Pause save, play, Pause load and a paused re-save gave a file byte-identical to the pre-load save (`40e47365…e114`): hunger 325.773621, 391 ×1, 392 ×1, quickslot 4 = 391. Session 01 was aborted by the watchdog (supervisor predated the new `wheel` action) before any purchase; nothing is claimed from it. Teardown was an explicit SIGTERM of the owned game. No runtime code changed. Tooling: the save inspector reports hunger and food stock (schema 3); the private-input helper gains bounded `drag` and `wheel`; restart the supervisor before using a new action. Observed, not changed: a repeated identical message does not re-show (so the quickslot eat showed no text), and the rebuild ignores quickbar digits with any panel open where the source does not. Checks: `make -C rebuild check` 280 passed, 0 failed; 971 Python tests, 95 skipped (skips are not passes) (`rebuild/build/compiler-tmp/food-final-check.log`); `test_native_private_input.py` 30 and `test_hunt_save_inspector.py` 3 pass. Not run, as no runtime ownership, exporter, generated or presentation code changed: sanitize, content, sources, UI render. Starvation in play and an original comparison remain unchecked.

October 14, director-chosen goal: [the Adventure HUD comfort presentation](audit/COMFORT_HUD.md) replaces the always-empty comfort track and permanent sad face. The source and serialized values come from `UIMainMidCtrl.UpdatePlayerAtrbute` and `GameMain.prefab` through the new `export_comfort_hud.py`. The new `pe_comfort_hud` is the presentation owner (transient, in `PeSandboxUI`, never saved), with one update call per frame outside drawing. The sandbox pauses it while building (source-derived) and while paused or unfocused (native adapter), and resets it at start and load (native lifecycle adapter). `pe_ui_hud` draws the fill, flash alpha and face from new layout rectangles. Tutorial and Legacy stay absent. No gameplay, save-format or closed-gate change. Results:
- Full check: 282 passed, 0 failed; 977 Python tests, 98 skipped (skips are not passes) (`rebuild/build/compiler-tmp/comfort-check.log`).
- `check-content` (`comfort-content.log`) and `check-sources`, including the 6 comfort-export tests under UnityPy (`comfort-sources.log`), pass.
- `ui-shared-render-capture` passes with exact tutorial/Legacy parity (`comfort-render.log`).
- The exporter's `--help` and `--check` were checked directly.
- Pre-push sanitizer: 113 passed, 0 failed, 0 skipped, with leak checks enabled (`rebuild/build/compiler-tmp/comfort-prepush-sanitize.log`; per-suite logs in `rebuild/build/test-logs/sanitize_*.log`).
- `ui-shared-test`, `sandbox-ui-test` and `cli-test` pass (`rebuild/build/compiler-tmp/comfort-ui-cli.log`).

Not checked: ordinary play (a revival flash), the original's load/pause behaviour, and engine-qualified curve values. At 320×240 face2 is indistinguishable from face3.

October 14, director-chosen goal, left uncommitted for review: [ordinary native selling and buyback](audit/TOWN_SELL_BUYBACK.md#ordinary-native-acceptance) at Xing Zhang, in session `rebuild/build/trade-private-20261014-01` from the earned food save (`40e47365…e114`, unchanged). Observed:
- The equipped refusal.
- A worn exact-instance sale and buyback: shovel instance 10 at 3543 durability, 41 each way, vendor credited 42.
- A partial Wood sale and buyback (10 out, 4 back) with both purses checked.
- Buyback retained across close/reopen.
- Load clearing buyback while money and inventory persist: the paused re-save is byte-identical to `traded` (`21a80c12…1ac0`).
- Live comfort fill matching the saved comfort on two in-world frames.

The final earned save for the next goal (camp repair) is `rebuild/build/trade-private-20261014-01/reloaded.pesave`: shovel instance 10 at 3543, Wood 194, money 261. No runtime code changed; the read-only save inspector gains vendor purses (schema 4) with a formatter test. No defect found. Not checked:
- digits versus quickslots, the small window, panel drag and a held close button;
- other vendors;
- a revival flash.

Checks: full check 282 passed, 0 failed; 978 Python tests, 98 skipped (skips are not passes) (`rebuild/build/compiler-tmp/trade-check.log`). The sanitizer is the director's pre-push run.

October 14 pre-push verification of the food-acceptance checkpoint: the required full sanitizer run passes 112 suites with zero failures/skips (`rebuild/build/compiler-tmp/food-prepush-sanitize.log`). Its first sandboxed attempt could not initialize LeakSanitizer under ptrace; the unrestricted rerun passed without disabling leak checks. The prior full check remains 280 jobs / 971 Python tests with 95 explicit optional skips; runtime sources are unchanged.

October 14 director pre-push verification of the ordinary commerce checkpoint: 113 sanitizer suites pass, zero failures/skips, with leak checks enabled (`rebuild/build/compiler-tmp/trade-prepush-sanitize.log`); the full regression is 282 jobs / 978 Python tests with 98 explicit optional skips. Director review independently verified the exact worn instance, purses/counts, byte-identical reload and all twelve lossless evidence crops.
