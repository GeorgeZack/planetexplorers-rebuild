# Running Knife root motion

**Qualified:** the isolated clip-motion curve for female `RunSwordAttack` 408, from trial 06, plus its native loader. It is installed with the matching running pose group.

**Not established:** live running-attack displacement, the source motor and collision, or gameplay parity. Runtime use is owned by [SANDBOX_RUNNING_KNIFE](SANDBOX_RUNNING_KNIFE.md) and the [melee contract](../FEATURE_CONTRACTS.md). Evidence record: [running-knife-root06.json](../reference/running-knife-root06.json).

## Source contract

- **Clip.** `FemaleAnimator2` 779 has a single leaf for `FullBody.Attack.RunSwordAttack`: clip 408, state speed 1, leaf duration 1, 1.8333396911621094 s at 30 Hz. Knife33 selects skill 20110014 for this stage. The plan pins `sharedassets1.assets` SHA256 `d9c65f820a2186116d9eb73d815fe07d9f46924ddfd103ee9510bf254f9a3d67`.
- **Delta reading.** [AnimatorCtrl.OnAnimatorMove](../../Assets/Scripts/PeEntity/Cmpt/AnimatorCtrl.cs) reads `Animator.deltaPosition` and `deltaRotation`. The action and motor then apply velocity correction and collision.
- **Velocity correction.** `Action_Sword.UpdateSkillState` projects `m_LastMove / Time.deltaTime` horizontally and caps its difference from the horizontal physics velocity at length 12 **per update**. That is not an acceleration of 12 m/s². A fixed-rate native application is an adapter.
- **No automatic yaw.** The line `trans.rotation = anim.m_LastRot * trans.rotation` is commented out, so captured rotation must not become player yaw. Root Y is kept as evidence only and is not grounded vertical motion.
- **EndAction.** Fires at 1.168072581 s, so a normal finished action must not move through the clip's unused tail. Running 20110014 is interruptible; sprint 20110013 is not equivalent.

## Sampler

[ReferenceRootMotionBake.cs](../../rebuild/tools/ReferenceRootMotionBake.cs) reads only the loaded female player's model, sex, Animator, Avatar and clip references.

- **Clones.** It copies the local skeleton into two fresh Transform+Animator hierarchies with identity roots: no controller, `fireEvents=false`, `applyRootMotion=true`, AlwaysAnimate. It never mutates the source, sends input or saves.
- **Lifetime.** Graphs are retained until process exit, never disposed, and a 120-update watchdog runs after output closes.
- **Bounds.** Two graphs, 512 transforms, 3,600 samples, 4 MiB.
- **Schema 2.** The engine player loop advances each clip-only graph, with each clone set to `DirectorUpdateMode.GameTime` at a fixed clone speed. No `Update`, seek or time assignment happens while sampling. The guard requires the playable time to match scaled frame dt within 0.0001 s. Each trial stops at the first observed time at or past the clip length; that overshoot is kept, and no terminal sample is invented.
- **Analyzer** ([analyze_reference_root_motion.py](../../rebuild/tools/analyze_reference_root_motion.py), `--require-motion`). Requires nonzero displacement, applied root versus accumulated delta within 1 mm, and cross-speed agreement within 5 mm. The comparison is over the union of every observed timestamp in the common range, the 60 Hz grid and the authored end, using linear position and shortest-sign quaternion interpolation inside measured brackets. Every authored gap must be at most 1/60 s for export. These are validation tolerances, not gameplay thresholds.

## Trials

| Capture | Method and speeds | Result |
|---|---|---|
| 01 | Manual `Animator.Update(dt)` | Clock guard failed on the first step, with nothing logged |
| 02 | Manual, after one primed frame | Expected 0.03333333507 s, actual 0; manual updates do not advance the Director clock |
| 03 | Frame-driven, 1 / 0.5 | 70 samples; clock error < 3.7e−9 s, delta error < 1.05e−6 m. Cross-speed error 0.189273 m at 0.040219545 s, because gaps of up to 0.0948 s at about 12 fps are too coarse. Rejected ([report](../reference/running-knife-root03.json)) |
| 04 | Dense, 0.1 / 0.05 | 654 samples, error 0.010034785 m. Rejected |
| 05 | Denser, 0.05 / 0.025 | 1,294 samples, error 0.007640744 m. Rejected |
| 06 | Ultra-dense, 0.025 / 0.0125 | 2,597 samples, error 0.002425775 m. **Passed** |

**Trial 06 details.**

- **Samples.** Trial 0 has 823 samples and trial 1 has 1,774, with maximum gaps of 0.007265151 s and 0.002825975 s.
- **Errors.** Root versus delta 3.374 µm and 10.390 µm; quaternion error 0.
- **Method.** No time shift, fitting, rescaling or tolerance change was used to qualify it.
- **End displacement.** At 1.8333396911621094 s it is about (−0.076067522, 0, 3.984784740) m in source XYZ. The final measured knot, at 1.833589792251587 s, is kept in the asset.
- **Capture.** SHA256 `b6ccd0199470a14b10182943d18db566409a03d27029a65bea657a06e63a43b4`. Its source, DLL, launcher, analyzer, exporter, plan and Player.log are preserved in `rebuild/build/root-motion-bake/root-06-provenance`.
- **Session.** It passed the 120-update watchdog and the dialogue stayed responsive ([frame](../reference/running-knife-root06-responsive.png)). Quit returned −9 (signal 9), the original's self-kill.

A [user-operated visual reference](../reference/running-knife-user-result.json) shows the poses and transitions. It cannot identify the skill branch or metric displacement.

## Export and native curve

[export_running_knife_motion.py](../../rebuild/tools/export_running_knife_motion.py) accepts only a complete schema-2 capture that passes `--require-motion` with no gap over 1/60 s.

- **Profiles.** Standard 1/0.5, dense 0.1/0.05, denser 0.05/0.025, ultra 0.025/0.0125. The launch manifest must agree with the capture.
- **Pinned identity.** The asset hash, Unity 5.2.4f1, controller 779, FemaleAvatar, clip 408 and skill 20110014.
- **Provenance checks.** The launch's embedded plan, capture path, profile, command, probe source, assembly and UnityEngine hashes. Older captures need their preserved `--probe-source` and `--probe-assembly`.
- **Output.** A fresh directory with `running-knife.perm` and `provenance.json`. Trial 1 is selected explicitly, with only its time-0 position subtracted, keeping source XYZ and +Z forward.

**PERM1 format.** Little-endian: `PERM`, then five int32 (version, clip, skill, count, reserved 0), a double duration, and samples of four doubles `(t, x, y, z)`. At most 3,600 samples, each component within ±10,000 m.

[pe_root_motion](../../rebuild/src/pe_root_motion.h) rejects:

- nonfinite, unordered, oversized, truncated or trailing data;
- wrong IDs or duration, or a nonzero origin;
- gaps over 1/60 s, or a missing or extra terminal crossing.

```c
PeRootMotion *pe_root_motion_load(const char *path, char *error, size_t capacity);
void pe_root_motion_free(PeRootMotion *motion);
bool pe_root_motion_sample(const PeRootMotion *motion, double seconds, PeVec3 *out);
bool pe_root_motion_delta(const PeRootMotion *motion, double from, double to, PeVec3 *out);
```

Times must be finite and within [0, duration], with `from <= to`. Invalid calls leave the output unchanged. The module owns no clock, yaw, collision or save data.

**Installed curve.**

- **Export.** 1,774 knots in a 56,800-byte file, SHA256 `1643255b87413534856f9c7635cb0f22d3c47a9802e1a64a826eef8bd3093860`, installed at `rebuild/build/reference-assets/player-running-motion` with the matching pose groups. Earlier folders are in `rebuild/build/player-before-running-20261006` ([install record](../reference/running-player-install.json)).
- **Native check.** 3,547 queries over every knot, the adjacent midpoints and the exact end. Maximum component error 2.384e−7 m, interval error 2.299e−10 m, accumulated composition 1.261e−9 m.

Recheck without changing installed assets:

```sh
python3 rebuild/tools/export_running_knife_motion.py \
  "$HOME/.local/share/planet-explorers-strafe-reference/pose-captures/female-running-root-06.jsonl" \
  --plan rebuild/build/root-motion-bake/root-06-provenance/running-knife-plan.json \
  --probe-source rebuild/build/root-motion-bake/root-06-provenance/ReferenceRootMotionBake.cs \
  --probe-assembly rebuild/build/root-motion-bake/root-06-provenance/ReferenceRootMotionBake.dll \
  --output rebuild/build/reference-assets/player-running-motion --check
```

## Pose capture

`RunSwordAttack:408` is in `inventory_reference_bake.CLIPS`, with its `FullBody.Attack.RunSwordAttack` state metadata, and in `ReferenceAnimatorBake.Allowed`. Female 408 is 57 poses at 30 Hz; the male override is clip 315. Body, drawn `mountMain` Knife and the 23-joint defence come from the existing exporters on the same clip clock ([running poses](RUNNING_PLAYER_POSES.md)).
