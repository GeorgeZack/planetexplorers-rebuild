# Isolated running Knife root-motion recovery

October6,2026: **trial06 qualifies the isolated clip-motion curve and its staged C export**. Trials01–02 rejected manual clock advancement; frame-driven trials03–05 failed the unchanged5mm cross-speed comparison. Ultra-dense06 passes at2.426mm with all authored gaps≤1/60s. Its1774-sample slower trial is exported and checked through the C sampler, then installed locally with the matching running pose group and preserved backups. This establishes sequential clone motion, not live running-attack displacement, source motor/collision or gameplay parity. The operator separately verified cleanup survival/responsive dialogue and ordinary Quit confirmation; the process returned−9/signal9. [Bounded evidence](../reference/running-knife-root06.json) and the latest checkpoint below preserve hashes and limits. This work extends [SANDBOX_COMBAT_NEXT](SANDBOX_COMBAT_NEXT.md) and is separate from the [paused pose sampler](ANIMATOR_BAKE.md).

## Source and installed identity

Female controller779 (`FemaleAnimator2`) has a single leaf for `FullBody.Attack.RunSwordAttack`, clip408, state speed1 and leaf duration1. The installed clip lasts1.8333396911621094s at30Hz. Knife33 selects skill20110014 for this running stage. The newly generated plan pins installed `sharedassets1.assets` SHA256 `d9c65f820a2186116d9eb73d815fe07d9f46924ddfd103ee9510bf254f9a3d67`, the exact source clip ID, humanoid bindings, events and controller transition metadata. The inventory is read-only and refuses an existing output plan rather than replacing capture provenance.

The source [AnimatorCtrl.OnAnimatorMove](../../Assets/Scripts/PeEntity/Cmpt/AnimatorCtrl.cs) reads `Animator.deltaPosition` and `deltaRotation` into the entity's animation component. The action/motor subsequently applies its velocity correction and collision. This clone sampler has no such component, motor, scene collider or gameplay callback. It measures the clip's sequential Unity evaluation, not the source entity's final physical path.

## Initial manual-advance trials01–02: isolation and clock

[ReferenceRootMotionBake.cs](../../rebuild/tools/ReferenceRootMotionBake.cs) reads only the matching loaded female player's model, sex, enabled Animator, Avatar and controller clip references. It copies one local skeleton TRS snapshot into two **fresh pure Transform+Animator hierarchies**, each with an identity root before evaluation. It never instantiates the original player GameObject or copies gameplay, renderer, physics or input components. It does not mutate the source Animator, send game inputs, invoke mission code or save.

Each new Animator has no controller, `fireEvents=false`, `applyRootMotion=true`, AlwaysAnimate culling and speed1. A clip-only playable starts at time0, playing at speed1. Following the first rejected trial, both clones are initialized paused at zero speed for one complete player-loop frame before either trial begins. That setup and the subsequent `Update(0)` baseline are explicitly excluded from accumulated motion. All later samples use strictly positive `Animator.Update(dt)` steps, ending at the exact authored length. The 30Hz trial contains57 samples and the60Hz trial112: the source duration lies slightly beyond55/110 nominal frame intervals, so each includes a short exact-terminal interval. There are169 samples total.

Each entire trial runs synchronously in one main-thread callback. No player-loop frame occurs between its manual evaluations, preventing automatic elapsed-time motion from being mixed with requested steps. The sampler checks actual playable time against accumulated requested time at every sample and records the frame number for independent validation. A clock mismatch fails closed; successful offline compilation does **not** establish that this old experimental API advances exactly as requested in the shipped runtime.

Records include actual clone world position/quaternion, Animator root position/quaternion, each delta, cumulative delta translation and ordered quaternion product. Unity XYZ is retained, with initial identity orientation and positiveZ forward; no native projection-X conversion, yaw clamp, endpoint correction or trajectory rescaling occurs. Both trials use the same copied source local pose. Source controller transitions, procedural IK, gameplay animation speed, hit slowdown and physics are intentionally absent from the measurement and remain separate recovery work.

## Initial manual-advance trials01–02: bounded lifetime

Limits are two retained graphs,512 transforms per graph,180 samples,1MiB output,16KiB per record,30 real seconds per trial and a configurable arm/player wait of1–1800 seconds. Stop-file checks occur before sampling and between steps. Error/stop/end deactivates all owned clones and closes output. Native graph disposal is deliberately avoided: the earlier pose sampler found cleanup crashes, so each playable is strongly retained with its finalizer suppressed until this disposable process exits. A120-update watchdog is logged after output closure. Output closure, watchdog survival and process exit are distinct evidence.

No existing pose sampler source, graph lifetime logic, capture files or recovered runtime assets are changed by this separate tool. Do not rebuild its DLL while an original process has loaded it.

## Commands

The installed UnityPy environment can write a fresh source plan. This plan already exists locally after the offline preparation; skip the first command unless creating a new plan path.

```sh
rebuild/build/unity-tools/bin/python rebuild/tools/reference_root_motion_bake.py --inventory
python3 rebuild/tools/reference_root_motion_bake.py
python3 rebuild/tests/test_reference_root_motion.py
```

The current default is the frame-driven schema2 retry described below. Default execution compiles only against the installed original's assemblies. It does not launch a game or change profile settings. The separate DLL is `rebuild/build/root-motion-bake/ReferenceRootMotionBake.dll`.

After any previous reference capture has ended, the sole desktop operator may explicitly launch:

```sh
python3 rebuild/tools/reference_root_motion_bake.py --launch \
  --single-threaded-rendering \
  --output "$HOME/.local/share/planet-explorers-bake-reference/pose-captures/female-running-root-01.jsonl"
```

Load a tutorial with a female player, then create the printed `.arm` file. The player need not obtain or swing a knife: only the existing controller's exact clip reference is read. Use the matching `.stop` file to cancel. The launcher preserves source/assembly/engine hashes in a fresh `.launch.json` and records native return code/signal separately in `.exit.json`. The process-scoped single-threaded rendering switch has bounded success evidence in [REFERENCE_RUNTIME](../REFERENCE_RUNTIME.md); it is not a proven crash fix.

```sh
python3 rebuild/tools/analyze_reference_root_motion.py \
  "$HOME/.local/share/planet-explorers-bake-reference/pose-captures/female-running-root-01.jsonl" \
  --require-motion \
  --report rebuild/build/root-motion-bake/running-root-01-report.json
```

[The analyzer](../../rebuild/tools/analyze_reference_root_motion.py) rejects partial captures, duplicate keys, changed source/isolation settings, nonfinite values, invalid rotations, clock drift, wrong sample order, clone reuse, intervening trial frames and inconsistent cumulative sums. It reports applied-root versus accumulated-delta errors and cross-rate endpoint differences. `--require-motion` additionally requires nonzero displacement, translation agreement within1mm, cross-rate endpoint agreement within5mm and quaternion-coordinate error below.001. These are explicit **validation tolerances**, not source gameplay thresholds. Every report marks gameplay motion and process teardown as unvalidated. This tool produces no native root-motion asset and cannot silently install a provisional curve.

Twelve offline tests pass, including synthetic translating/rotating sequences, stopped/truncated traces, nonfinite or malformed samples, nonadvancing clocks, automatic-frame interference, shared clones, nonapplied deltas, cross-rate disagreement, exact source hash/identity checks, build-only isolation, preload refusal and negative process-exit preservation. The managed sampler compiles against this installation's Unity5.2.4f1 API. Tests use synthetic curves only to validate parsing/math; they establish no source trajectory.

## Review before exporter or runtime integration

The following is the original proposed implementation sequence. The isolated exporter/sampler foundation and separate running pose stage are subsequently implemented below and in [RUNNING_PLAYER_POSES](RUNNING_PLAYER_POSES.md); the plan does not establish completed live movement/contact integration. It assumes a supervised capture passes the structural/clock checks, clone displacement agrees with accumulated deltas, both sampling rates agree within the stated diagnostic tolerances, and post-sampling/process-lifetime evidence is recorded separately.

1. **Preserve evidence before making an asset.** Keep the raw capture and launch/exit sidecars in the isolated profile; create a bounded report with their hashes. A future `export_running_knife_motion.py` should require the exact compatible source plan and successful complete capture, reject conflicting identity/timing or partial trials, and provide a deterministic `--check`. Keep both trial summaries in provenance and select the declared60Hz trial explicitly. Never average mismatched curves or alter an endpoint to force agreement. Store source authored timestamps and delta/cumulative translation; translation relative to the recorded time0 root is a declared coordinate conversion, not a replacement for the raw evidence. Save rotations as evidence even if gameplay does not consume them. Initially produce a separate ignored recovered asset, not a generated fallback curve or automatic installation.

2. **Recover the complete pose group independently.** The new root sampler records no bone palettes and cannot supply running-body or blade geometry. Recover female408 body, exact `mountMain` held Knife33 and compatible23-joint defence poses from an independently validated pose bake. All three need the same source identity, ordered clip clock, duration and skeleton checks already applied to Attack6–9. Extend pose/exporter allowlists through their owners; do not relabel Attack6 as RunSwordAttack. Root removal in PEAC must remain consistent with the one gameplay root translation. A source root-motion curve alone is not permission to admit a moving swing.

3. **Make skill selection explicit.** [pe_combat](../../rebuild/src/pe_combat.h) currently hardcodes stationary20110011 and four clips; [pe_sandbox_melee](../../rebuild/src/pe_sandbox_melee.h) additionally requires no requested movement and horizontal speed≤.05. Add a bounded running first-stage variant with source20110014, female408 event metadata and the existing Attack7–9 continuation, without silently changing existing stationary/FP APIs. Source selection checks full actual `move.velocity.magnitude`, with air and water taking precedence; serialized4.5/7 thresholds still need installed/live verification. Input intent is not the source selection variable. Unsupported branches must fail before stamina debit. Existing Attack7–9 motion has not been recovered sequentially by this sampler; retaining current stationary continuation movement is an explicit remaining adapter, not full source running-combo motion parity.

4. **Separate clock advance from spatial contact.** Current `pe_sandbox_melee_step` advances the clock and immediately builds the blade/contact at its supplied position; the sandbox host advances ordinary movement separately, and the active-hunt branch calls melee after that movement. Running integration needs a staged transaction: determine old/new authored times and events once, derive the displacement interval, apply supported player collision once, then evaluate body, blade, defence and contacts from the resulting authoritative position and clock. Both hunt and no-hunt paths must use that ordering. Do not advance the combat clock twice, add ordinary WASD displacement on top, or move only the renderer. A failed validation must not partially debit stamina, move the player or damage a target.

5. **Apply only the source-supported motion components.** `Action_Sword.UpdateSkillState` projects `m_LastMove / Time.deltaTime` horizontally, compares it with current horizontal physics velocity and limits their difference to length12 **per update** when its magnitude is≥12. This is not acceleration12m/s². A fixed60Hz native application would be an explicit update-rate adapter until the live frame/motor path is compared. Root-motion Y is retained in evidence but is not automatically vertical grounded-player motion. The nearby `trans.rotation = anim.m_LastRot * trans.rotation` line is **commented out**: captured delta rotation must not become an invented automatic player yaw. Active source attack facing follows its target/look-direction logic separately. A locally sampled displacement interval must be transformed once through the chosen body heading; changing heading during the interval needs an explicit consistent convention.

6. **Keep interruption and persistence honest.** A running stage remains transient, bound to the exact held inventory instance and the shared action clock. Pause/modal ownership, load, death, equipment replacement and cancellation clear pending root deltas and contact history. Save payloads should not acquire borrowed asset pointers or half-applied motion. Source running20110014 is interruptible; sprint20110013 is not equivalent. Full transition blending, hit-response slowdown and state-machine entry behavior remain separate from this clip-only dataset. The clip has1.83334s of authored data, but its EndAction event is at1.168072581s; a completed full-clip capture does not authorize playing/moving through the unused tail during a normal finished action.

Useful tests for that future slice are rate-independent interval composition at fixed authored endpoints, exact final short interval, yaw transforms, zero/invalid intervals, threshold equality and actual velocity versus input, source≥12 velocity correction, wall/tree/building stopping, no doubled walking, stage transitions without surplus-time reuse, pose/contact position agreement, and atomic cancellation/load/equipment changes. Physical running contact, ordinary four-kill completion and partial-progress reload remain separate required observations; none follows from synthetic curve tests or a clone-only bake.

### Prepared408 pose capture, offline integration complete

Read-only inspection of all nine existing JSONL bakes in the animator-bake build area and isolated bake profile found no `RunSwordAttack` clip record. At inspection time neither the Python catalog nor managed pose-sampler allowlist contained that name; the subsequent coordinated update below adds it. The new local `rebuild/build/root-motion-bake/running-knife-pose-plan.json` uses the existing sampler's schema and exact asset hash; it contains female408 and its separately decoded male315 override, with their own timing/events. The intended capture selects **female408 only,57 poses at30Hz**. This is a prepared and now accepted plan, not a completed capture.

After the operator confirmed no original process was running, the coordinated update added `RunSwordAttack:408` to `inventory_reference_bake.CLIPS`, retained direct `FullBody.Attack.RunSwordAttack` state metadata, and added the same name to `ReferenceAnimatorBake.Allowed`. Defaults and the current32-clip/3,600-sample retention bounds remain unchanged by this addition. Twenty combined catalog/launcher tests pass, including exact running-state leaf selection,57-sample optional capture and cross-language allowlist equality. The pose sampler compiles against the installed assemblies with the minimal running plan. The root-motion DLL is separate and was not rebuilt. Do not rebuild a DLL loaded by a running original.

The bounded capture command is:

```sh
python3 rebuild/tools/reference_animator_bake.py --launch --single-threaded-rendering \
  --plan rebuild/build/root-motion-bake/running-knife-pose-plan.json \
  --clip RunSwordAttack \
  --output "$HOME/.local/share/planet-explorers-bake-reference/pose-captures/female-running-pose-01.jsonl"
```

The existing body bake analyzer/exporter accepts compatible source-bound clip names. `export_baked_knife.py --attachment drawn` already derives the exact mountMain attachment and blade PEHB from those poses and requires identical body/knife clip clocks. `export_player_defence.py --extra-capture` already supports adding a unique compatible clip with the23-joint defence palette. Export to fresh staging directories first; merge into the existing complete groups only after verifying geometry/skeleton hashes and all shared clip clocks. None of those exporters needs a fabricated new attachment or capsule. The host's fixed Attack6–9 clip arrays and admission rules are a separate runtime change; asset recovery alone must not loosen them.


## Original trial01 rejected; diagnostic retry prepared

The operator's `female-running-root-01.jsonl` in the isolated strafe profile contains a matching female source (78 hierarchy nodes), a single time0 sample, and a closed `end(reason="error", samples=1)`. The first positive-dt step hit the strict playable-clock guard. This original implementation did not log the actual mismatched clock, so the capture establishes **neither** that time remained0 **nor** that it advanced by another amount. It contains no usable motion curve. Its capture hash and copied failure Player.log are preserved in ignored `rebuild/build/root-motion-bake/root-01-failure.json` before another original launch could overwrite that log. The operator observed the120-update watchdog and a responsive Camera Tips dialogue after rejection, then used Quit Game and confirmation; exact process status is recorded separately by the operator.

After the operator confirmed the original had exited, the sampler was changed to let both paused pure graphs receive one full player-loop initialization frame before beginning the synchronous positive-dt trials. This follows the older pose sampler's established need to wait a frame, but its relevance to root motion remains a hypothesis until retry. A future rejection now writes a bounded `clockFailure` record with expected and actual playable times, requested dt, play state/speed, frame, root position and deltaPosition before closing. The strict analyzer continues to reject such output; diagnostics cannot become recovered movement. The retry sampler compiles against the installed assemblies and its twelve existing parser/lifecycle tests pass. The separate ordinary pose-sampler DLL is unchanged by this retry preparation.

Installed managed API inspection found `Playable.time/state`, `AnimationClipPlayable.speed`, and `DirectorPlayer.SetTimeUpdateMode(Manual)/SetTime/GetTime`, but no exposed graph `Evaluate(dt)` method. These signatures alone do not prove manual animator updates advance a Director clip clock. A subsequent API experiment must retain the same exact-time checks and independent applied-root/delta comparison; merely setting an explicit time is not evidence of valid cumulative root motion.


## Trial02 result and current schema2 frame-driven retry

Trial02 confirms the mismatch rather than merely reporting a guard failure: expected playable time`.03333333507180214`, actual`0`, dt`.0333333351`, play state`Playing`, speed`1`, clone position`[0,0,0]` and deltaPosition`[0,0,0]`. One paused initialization frame did not make `Animator.Update(dt)` advance the Director clock. Its raw capture remains untouched, with copied failure Player.log and metadata under `rebuild/build/root-motion-bake/root-02-failure*`. This is no recovered movement.

The current **schema2** sampler instead allows the ordinary engine player loop to advance the clip-only graph. The [Unity5.2 Playables manual](https://docs.unity3d.com/520/Documentation/Manual/Playables.html) describes automatic playback through `Animator.Play(playable)` and distinguishes explicit playable-time control. Installed metadata independently confirms `Animator` inherits `DirectorPlayer`, including `SetTimeUpdateMode`, `GetTime` and `GetTimeUpdateMode`; no exposed Tick/Evaluate method was found. Changing only each isolated clone to `DirectorUpdateMode.GameTime` makes the expected clock explicit without altering global time or frame rate.

Both pure clones are primed paused. The first then plays at speed1 and the second at.5, with a baseline recorded immediately before each begins. No Animator.Update, seek or explicit time assignment occurs during advancement. Each end-of-frame sample records its actual playable time, inherited Director time/mode, `Time.deltaTime`, root transform and engine delta. The guard requires actual playable advancement to match scaled frame dt×clone speed within.0001s. Sample frame IDs must increase. Zero scaled-dt pauses produce no fabricated advance and are bounded by the30-real-second trial timeout.

Each trial stops after its first naturally observed playable time at or beyond source clip duration. **There is no invented exact-terminal sample.** The actual terminal overshoot is preserved. Schema2 permits at most3,600 samples and4MiB output across two retained graphs; transform and per-record bounds remain512 and16KiB. Inactivation, bounded retention, stop handling and the120-update watchdog remain unchanged. Post-sampling survival and native exit still require separate observations.

The analyzer retains strict schema1 support for historical traces and adds schema2 validation for real frame clocks, increasing frame IDs, distinct clones, cumulative translation/rotation and first end crossing. Comparison uses baseline-relative positions and rotations at common authored times **only inside measured brackets**; unequal raw overshoot endpoints are not compared as if simultaneous. Position interpolation is linear, quaternion interpolation normalized with shortest sign, and neither replaces raw evidence. Reports expose maximum authored sample gap and engine frame dt. A passing consistency flag alone does not justify dense runtime interpolation through a coarse observed gap.

Sixteen offline tests and installed-assembly compilation pass; an independent agent also reviewed clock/lifetime/overshoot handling and reran all16 tests. The added cases cover variable frame intervals, shifted baseline roots, explicit overshoot, real clock/frame mismatch, wrong Director mode, invented terminal times and nonapplied root deltas. That was the preparation checkpoint. Trial03 subsequently completed below; its curve remains unqualified. No capture is currently active under this task.


## Completed original trial03: clocks and deltas pass, curve comparison fails

[The bounded report](../reference/running-knife-root03.json) preserves capture SHA256 `f2c0fe0b6d6ab909a7d9713fd6eaff1421c449cf02872deef1436a4424ba489c`, launch/source provenance, diagnostics and separate lifecycle evidence. The70-sample capture closed complete:23 samples at speed1 and47 at.5. Actual playable-clock errors are below3.7e−9s, and applied root positions agree with cumulative engine deltaPosition within1.05e−6m. Root Y and rotation remain unchanged in this capture. Thus ordinary player-loop playback advances this isolated clip; this does not validate live gameplay movement or the source motor.

The original ran near12 frames/s during sampling. Maximum authored gaps are.094793s and.052204s. The fast trial's first positive sample is at.094793s with sourceZ=.597169m; the slower trial already recordsZ=.065337m at.0402195s. Linear interpolation across the fast trial's initial broad bracket cannot reproduce that early nonlinear shape accurately.

The original60Hz-only comparison reported.156867m maximum difference. After independent review, the analyzer now compares the **union of every actual sample timestamp in the common authored range**, the60Hz grid and source end. For piecewise-linear position curves, the maximum difference norm occurs at one of those union knots. The corrected maximum is **.189273m at.040219545s**, over178 comparison times. Raw capture data and the original report are unchanged; the bounded report explicitly records the method correction. A regression test places a peak between60Hz ticks to prevent underreporting it again.

Further offline diagnostics support insufficient sampling resolution as a plausible explanation: decimating only the slow trial to every other recorded point introduces.162081m early interpolation error without changing playback speed. The two naturally reached raw terminal positions agree within2.31e−7m. Interpolating both curves at the exact source duration instead differs by about1.13mm because each terminal measurement overshoots by a different amount. Neither endpoint agreement nor decimation proves that evaluation is speed-invariant, rules out initialization differences, or qualifies the interior trajectory. `--require-motion` correctly fails; no tolerance was loosened and no motion asset was exported.

The root desktop operator observed the120-update post-cleanup watchdog and [responsive Camera Tips dialogue](../reference/running-knife-root03-responsive.png), then selected Quit Game and Yes. The exit sidecar records−9/signal9 at2026-10-06T16:30:14.496484Z, consistent with the previously documented original self-kill exit behavior; this is not an exit0 claim or graph-disposal validation. The analyzer's process-teardown boolean remains false because trace contents alone cannot establish lifecycle success; the separate operator/exit evidence is included in the bounded report.

## Earlier dense comparison preparation

An opt-in `--dense` profile is now compiled offline. It changes only the two clone playable speeds to.1/.05 and permits60 real seconds per trial; standard mode remains1/.5 with30 seconds per trial. Total capture bounds stay two graphs,3,600 samples and4MiB. At the observed12fps, the dense profile should collect roughly.008/.004 authored-second intervals, but actual gaps will be measured and reported rather than assumed. Its nominal full-clip playback takes about55 seconds across both trials. It changes no global frame rate, time scale, source player state or save.

The analyzer admits only the two explicit speed profiles, retains the same strict clocks/delta/error tolerances, includes all measured comparison knots, and separately reports whether every authored gap is at most1/60s. A future exporter must consider both curve agreement and real gap coverage. Eighteen tests now pass, including the dense clock profile, profile propagation, and off-grid comparison maximum; installed-Unity compilation also passes. At that preparation checkpoint no dense capture had been launched. The subsequent04–06 results below supersede that status.

For a future supervised run with a fresh output path:

```sh
python3 rebuild/tools/reference_root_motion_bake.py --launch --dense \
  --single-threaded-rendering \
  --user-data "$HOME/.local/share/planet-explorers-strafe-reference" \
  --output "$HOME/.local/share/planet-explorers-strafe-reference/pose-captures/female-running-root-04.jsonl"
```

Do not rebuild the sampler DLL during that original process. Load the matching female tutorial, arm explicitly, record completion/watchdog and exit separately, and rerun the strict analyzer before proposing any runtime motion asset.


## Subsequent user-operated visual reference

The [October6 original demonstration](../PLAYTEST.md#october-6--user-operated-original-running-attack-demonstration) records separated swings, travel-to-attack transitions and a longer attack chain on open ground. Two recordings closed successfully, and63 preserved manual-checkpoint files stayed unchanged. [Video hashes and bounded observations](../reference/running-knife-user-result.json) distinguish requested W/no-Shift inputs from actual uninstrumented behavior. This reference helps assess visible poses/recovery, but cannot identify source skill branches or separate camera movement from metric player displacement. It does not qualify the root curve or remove the dense-capture/live-telemetry gates above.

## Implemented isolated export and sampling foundation

[export_running_knife_motion.py](../../rebuild/tools/export_running_knife_motion.py) now implements the bounded export gate independently of gameplay. It accepts only a complete schema2 capture that passes `analyze(..., require_motion=True)` **and** has no authored sample gap greater than1/60s in either trial. Standard, dense, denser and ultra-dense profiles have explicit speed pairs1/.5,.1/.05,.05/.025 and.025/.0125; the launch manifest and capture must agree. It never loosens the5mm cross-speed comparison tolerance, merges mismatched trials or exports a fallback from an unqualified capture.

The exporter checks the supplied source plan against the installed asset, pins the known `sharedassets1.assets` hash, Unity5.2.4f1, FemaleAnimator2/controller779, FemaleAvatar, clip408 and skill20110014. It also verifies the launch's embedded plan/hash, capture path and isolated profile, executable/profile command, probe source hash, compiled assembly hash and UnityEngine hash. If a retained capture used earlier sampler code, provide those exact preserved files with `--probe-source` and `--probe-assembly`; rebuilding or using the current DLL does not establish old provenance. Process lifecycle remains separately observed evidence, not inferred from a closed trace.

The output is a fresh directory containing `running-knife.perm` and `provenance.json`. The latter preserves the complete source plan/launch, raw input hashes, both trial diagnostics, the selected trial's raw samples including rotations/deltas, and the binary hash. Trial1—the declared slower trial—is selected explicitly. Its original double timestamps and measured positions are retained, with only the measured time0 position subtracted. Source XYZ remains unchanged, with positiveZ forward. The final observed overshoot knot is retained to bracket the authored end; it is not replaced with an invented exact-terminal sample. No installed pose directories or generated fallback assets are modified.

The binary is little-endian PERM1: four-byte `PERM`, then version, clip ID, skill ID, sample count and reserved0 as five32-bit integers, then the double authored duration; each sample is four doubles `(authored_seconds, x, y, z)`. Sample count is bounded to3600 and each displacement component to±10000m as parser bounds, not gameplay limits. [pe_root_motion](../../rebuild/src/pe_root_motion.h) owns the loaded curve and rejects nonfinite, unordered, oversized, truncated or trailing data, wrong IDs/duration, nonzero origin, gaps over1/60s and missing/extra terminal crossings.

The headless API is:

```c
PeRootMotion *pe_root_motion_load(const char *path, char *error, size_t capacity);
void pe_root_motion_free(PeRootMotion *motion);
bool pe_root_motion_sample(const PeRootMotion *motion, double seconds, PeVec3 *out);
bool pe_root_motion_delta(const PeRootMotion *motion, double from, double to, PeVec3 *out);
```

Sampling returns absolute displacement relative to time0; interval sampling subtracts the two double-precision interpolated endpoints before converting to `PeVec3`. Both require finite times within `[0, authored_duration]`, and intervals require `from<=to`; equal endpoints produce zero. Invalid calls leave output unchanged. Interpolation stays inside observed brackets. The module owns no clock, action state, yaw/projection transform, collision or save data. Captured Y and quaternion evidence do not authorize grounded vertical motion or automatic yaw. Action EndAction can end before the clip's full duration; consumers must use their actual outgoing authored interval rather than moving through the unused tail.

After a strictly qualified capture exists, the desktop-independent export/check commands are:

```sh
python3 rebuild/tools/export_running_knife_motion.py /path/to/qualified-capture.jsonl \
  --output rebuild/build/running-knife-motion-qualified
python3 rebuild/tools/export_running_knife_motion.py /path/to/qualified-capture.jsonl \
  --output rebuild/build/running-knife-motion-qualified --check
```

The exporter completes all validation before creating output and refuses an existing output directory except in deterministic `--check` mode. This command is an export, not gameplay installation or permission to enable running attacks.

Eight synthetic exporter tests pass, including strict provenance, unqualified and otherwise-qualified-but-coarse rejection, profile matching including the required ultra-dense180s header, fresh-output/check behavior and denser-profile Python export consumed by the actual C loader. Focused strict optimized and ASan/UBSan C checks pass with `detect_leaks=1`: original XYZ, irregular end bracket versus exact terminal, interval composition, zero interval, finite/range/order guards and malformed file rejection. [C tests](../../rebuild/tests/test_root_motion.c) and [export tests](../../rebuild/tests/test_running_knife_motion_export.py) validate software and synthetic math only. No source trajectory or running gameplay is established by these tests; capture qualification and live motion/contact comparison remain separate gates.


## Trials04–06: strict qualification and staged source curve

The three subsequent frame-driven captures use progressively slower clone playables without changing the global engine clock, source player or tolerance. Their captures and earlier reports remain unchanged; current strict analysis independently rejects04/05 and accepts06. Every trial below meets the separate maximum authored gap of1/60s.

| Capture | Clone speeds | Samples, both trials | Maximum cross-speed position error | Strict5mm comparison |
|---|---|---:|---:|---|
| 04 dense | .1 / .05 | 654 | .010034785m | Rejected |
| 05 denser | .05 / .025 | 1294 | .007640744m | Rejected |
| 06 ultra-dense | .025 / .0125 | 2597 | .002425775m | Passed |

Trial06 has823 and1774 measured samples. Their maximum authored gaps are.007265151s and.002825975s. Maximum applied-root versus accumulated-delta errors are3.374µm and10.390µm; the cross-speed quaternion error is0. The comparison still uses the union of all observed common-range knots, the60Hz diagnostic grid and authored end. No timing shift, fitted curve, rescaling or tolerance change was used to qualify06. Denser agreement is evidence for this bounded curve; it does not prove speed-invariant behavior under every engine frame pattern.

The slower trial's selected end displacement, interpolated strictly inside its measured final bracket at source duration1.8333396911621094s, is approximately sourceXYZ(-.076067522,0,3.984784740)m. Its final measured knot is at1.833589792251587s, an overshoot of.00025010108947753906s; that original knot remains in the asset. These are clone-root displacements, not a live player's path. The normal running skill's earlier EndAction event still limits actual action use; the complete clip endpoint does not authorize moving through its unused tail.

Matching06 source, compiled DLL, launcher, analyzer, exporter and plan were copied to ignored `rebuild/build/root-motion-bake/root-06-provenance` before any later sampler changes. The operator also preserved Player.log there. Capture06 SHA256 is `b6ccd0199470a14b10182943d18db566409a03d27029a65bea657a06e63a43b4`. The retained fresh export is `rebuild/build/running-motion-20261006`:1774 knots in the56,800-byte PERM file, plus full provenance JSON. Curve SHA256 is `1643255b87413534856f9c7635cb0f22d3c47a9802e1a64a826eef8bd3093860`. That export checkpoint preceded installation. The subsequent authorized transaction installed the unchanged curve under `rebuild/build/reference-assets/player-running-motion` together with the four matching pose groups; prior pose folders are preserved in `rebuild/build/player-before-running-20261006`. [The installation record](../reference/running-player-install.json) separates this later state from the root06 export-time record. No native graphical/gameplay result follows from installation.

Both strict export and deterministic `--check` pass. The actual C loader/sampler then passed3,547 source-derived checks covering all in-range observed knots, adjacent midpoints and exact authored end, with interval deltas between consecutive query times. Expected values were independently calculated from raw trial1 records. The maximum absolute component error is2.384×10⁻⁷m; maximum interval component error is2.299×10⁻¹⁰m; accumulated interval composition differs by1.261×10⁻⁹m. This runner passes ASan/UBSan with `detect_leaks=1`, exit0. Local runner, expected values and result are `root-06-native-check.c`, `root-06-native-expected.txt` and `root-06-native-check.json` under `rebuild/build`. These validate native decoding/interpolation, not gameplay movement.

Recheck the retained stage without changing installed assets:

```sh
python3 rebuild/tools/export_running_knife_motion.py \
  "$HOME/.local/share/planet-explorers-strafe-reference/pose-captures/female-running-root-06.jsonl" \
  --plan rebuild/build/root-motion-bake/root-06-provenance/running-knife-plan.json \
  --probe-source rebuild/build/root-motion-bake/root-06-provenance/ReferenceRootMotionBake.cs \
  --probe-assembly rebuild/build/root-motion-bake/root-06-provenance/ReferenceRootMotionBake.dll \
  --output rebuild/build/running-motion-20261006 --check
```

The root desktop operator observed the120-update post-cleanup watchdog, acknowledged Andy's dialogue and inspected the [responsive Camera Tips frame](../reference/running-knife-root06-responsive.png). Quit Game→Yes then produced the recorded exit−9/signal9 at2026-10-06T17:54:59.964570Z. This matches the prior original self-kill behavior; it is not exit0 or successful graph disposal. The analyzer correctly continues to mark lifecycle as unvalidated by trace contents alone. [The bounded record](../reference/running-knife-root06.json) separately includes these operator observations, the exit sidecar, hashes for04–06, preserved provenance and native sample-check results. Source transitions, live skill clock/motor behavior, collision and actual running contact remain separate validation work.


The installed motion folder also passes the exact root06 exporter `--check` and the3,547-query native ASan/UBSan/leak validation above. Use the same recheck command with `--output rebuild/build/reference-assets/player-running-motion` to verify it. The clone curve's qualification and source evidence are unchanged by installation; live motor/collision/contact behavior remains separately documented by the operator.
