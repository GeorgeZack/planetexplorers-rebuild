# Live running Knife observation

October 6, 2026: the third original launch completed a bounded software-rendering capture and confirmed the grounded Knife33 running branch. Earlier two launches failed as recorded below. The successful trace is coarse; it does not qualify precise live root/motor timing. The [root sampler](RUNNING_KNIFE_ROOT_MOTION.md) measures isolated clip displacement; the [pose stage](RUNNING_PLAYER_POSES.md) holds compatible body/weapon/defence geometry. Neither identifies a live attack's actual selected skill or final physical movement. The uninstrumented user demonstration supplies visual chronology, not those hidden values.

## Separate observation tool

[ReferenceRunningAttackProbe.cs](../../rebuild/tools/ReferenceRunningAttackProbe.cs) is installed through the existing launch-only main-thread shim in a **separate** `rebuild/build/running-attack-probe` directory/DLL. It never attaches to an existing PID. Default [reference_running_attack.py](../../rebuild/tools/reference_running_attack.py) execution compiles offline. No root-sampler source, DLL, recovered asset or original save is edited.

The operator loads the player completely before explicitly arming. At `WaitForEndOfFrame`, the observer reads:

- Entity and model positions/quaternions; actual `Motion_Move.velocity`, grounded status and `Action_SwordAttack.phyMotor.velocity`.
- The existing `Motion_Equip.m_SwordAttack` object's runtime move/sprint thresholds, air/water/move/sprint flags, first-person flag, directions, combo count and end-action flag. Flags can outlive an active skill; the analyzer does not treat flags alone as an attack.
- Exact held sword item prototype and the action's existing `m_SkillInst` reference: observed identity, numeric skill ID, guide step, active state, external-contact enable, start time and cooldown flags. Source guide values include ending `-1`, cooling `-2` and ended `-99`; these are not invalid positive guide indices.
- `AnimatorCmpt.m_LastMove/m_LastRot`; initialized active Animator identity/controller/avatar, speed, root/delta transforms, layer weight/transition, current/next state hashes, normalized time, length, speed/multiplier and current/next weighted clip names. Runtime clip instance IDs are recorded as runtime identities, **not** serialized asset path408.
- Scaled time/dt, fixed dt, real time, Unity frame ID and player stamina.

Required private fields and their types were confirmed through read-only metadata inspection of the installed `Assembly-CSharp.dll`, recorded locally in `build/running-attack-probe/installed-fields.txt`. Direct APIs compile against this installation's Unity5.2.4f1 assemblies. Field-contract validation fails closed before capturing. The Animator guard requires a loaded view/model, enabled active Animator, attached controller and `isInitialized`; it avoids native state calls during known loading states. This is an offline safety check, not proof of live runtime stability.

The observer has no input, movement/Animator setters, skill start/cancel calls, component creation on a source entity, save writes, RNG, equipment mutation, cloned graphs or pool traversal. It adds its own transient observation GameObject/component and therefore changes process state and costs some frame time. It retains at most1,024 observed skill references until its component is destroyed.

## Bounds and capture

The default post-arm observation lasts at most120 real seconds or7,200 frame opportunities, with32MiB total and128KiB per-record limits. Frames with unready players are skipped but count toward the frame/time bounds. An explicit stop file ends early. The arm wait is bounded independently, default900s and maximum1,800s. On completion/error the stream closes and the observer object is destroyed; no original process is automatically killed.

After the desktop operator confirms the previous original has exited:

```sh
python3 rebuild/tools/reference_running_attack.py --launch --single-threaded-rendering \
  --user-data "$HOME/.local/share/planet-explorers-strafe-reference" \
  --output "$HOME/.local/share/planet-explorers-strafe-reference/pose-captures/female-running-live-01.jsonl"
```

Load the female tutorial/open-ground checkpoint with the original Knife33. Create the printed `.arm` file only once ready. Capture several idle frames, ordinary forward travel to steady speed, a separated attack, recovery and a short chain. The operator supplies all inputs; this tool does not arrange the player, equip anything or ensure a requested input selected a particular branch. Use the printed `.stop` file when enough evidence is present, then verify responsiveness and ordinary UI exit separately. Preserve baseline saves and do not rebuild this observer's DLL while its process is running.

Fresh capture/arm/stop/launch/exit paths are mandatory. Launch provenance records command, profile, limits and probe, DLL, shim, installed game/engine assembly and asset hashes. `--single-threaded-rendering` uses the documented process-only rendering switch; it is not a proven crash fix. The exit sidecar preserves actual return code/signal, including the original's negative exit behavior.

```sh
python3 rebuild/tools/analyze_reference_running_attack.py \
  "$HOME/.local/share/planet-explorers-strafe-reference/pose-captures/female-running-live-01.jsonl" \
  --require-running --report rebuild/build/running-attack-probe/live-01-report.json
python3 rebuild/tests/test_reference_running_attack.py
```

The bounded analyzer rejects duplicate keys, nonfinite required data, frame/time reversal, changing player/skill identity, missing/error closure, count mismatch and conflicting launch provenance. It reports actual threshold pairs, per-instance skill/guide/clip-clock ranges, first/last observed positions and prior observed velocity. `--require-running` requires launch provenance and at least one co-observed active20110014, Knife33, grounded third-person running flag, no air/water/sprint flag, and a positive-weight current `RunSwordAttack` clip on a positive-weight layer. It does not silently equate another skill, a queued next clip or an inactive residual flag with a running attack.

Twelve focused synthetic tests and managed compilation pass. These establish tool contracts only, not any source behavior. The focused command above runs `test_reference_running_attack.py`; Makefile registration is delegated to the integration owner. No graphical test or live capture is claimed.

## Evidence limits

An end-of-frame snapshot can miss a short branch, transition or hit guide. Its previous velocity is not necessarily the instantaneous velocity used by `ApplySkill`; source flags and selected skill provide the direct branch evidence. A positive clip/state observation identifies what was actually evaluated, while exact asset408 attribution additionally depends on the recorded controller/avatar and compatible installed asset provenance.

The source `OnAnimatorMove` callback and action/motor updates have their own scheduling. Simultaneous sampled `m_LastMove`, Animator delta and physical velocity help investigate their relation but do not prove callback order, exact per-update correction, or that physical displacement equals raw clip displacement. Camera perspective is irrelevant to recorded world transforms, but collision, gravity, controller blends, speed scaling and hit slowdown can affect the live path. Full action-entry/exit trajectories require examining actual sample gaps and captured coverage, not merely a positive `--require-running` result.

No native root curve is exported and no running attack is enabled by this tool. Dense clone qualification, live branch/path comparison, source event/motor integration and native collision/contact checks remain separate gates.


## First launch: pre-arm graphics crash

Original PID1859771 loaded preserved manual5 in the isolated hunt profile with `-force-gfx-direct`; the operator equipped Knife33 and moved away from town. At12:46:12CDT, before arming, it aborted. The capture contains only its258-byte header and no player/Animator samples; it is not a valid live-attack capture. The exit sidecar records−6/SIGABRT. [The bounded diagnosis](../reference/running-live01-crash.json) preserves log/capture hashes and system-core evidence.

The underlying fault PC`0x415d9b86` is in a generated `/memfd:/.glXXXXXX` mapping, executing `mov (%rsi),%edi` with RSI=`0xc`. The next five NVIDIA glcore offsets match the earlier [graphics failures](../reference/reference-render-diagnostics-20261006.json): `b5aa2b/c78545/c785fe/c7865c/c799aa`. This time it occurred on the main thread in the direct-rendering launch. All41 thread backtraces were inspected; no OOM-kill or NVIDIA Xid journal evidence appeared in the bounded interval. Later memory availability is not a crash-time allocation measurement.

This establishes recurrence despite the process-only rendering option. It does not establish observer causation, the exact driver-versus-game defect, or any running attack behavior. Player.log and sidecars were copied to a fresh ignored failure directory before another launch. The system core was preserved; the diagnostic copy was extracted into a random private build directory and deleted in finally after inspection. No desktop/system changes were made by the diagnosis.


## Second launch: incomplete diagnostic prefix

Original PID1888279 also aborted before an explicit stop. Unlike01, the operator armed this run: [634 frames over29.19 game seconds](../reference/running-live02-crash.json) were preserved. The prefix reads installed move/sprint thresholds4.5/7, maximum observed actual speed3.89042m/s and eight samples with stationary skill20110011. It contains no running20110014 observation. Requested movement does not prove the instantaneous speed needed for a running branch; this prefix cannot diagnose why speed stayed below the threshold.

The strict analyzer rejects the missing normal closure. `coredumpctl` confirms main-thread SIGABRT with Mono signal handling and anonymous PC`0x40abee02`, resembling the outer01 failure stack. This bounded second inspection did not extract the core, so it does **not** establish the exact fault instruction, GL mapping or matching driver caller offsets. The journal interval contains no OOM/Xid evidence. Player.log, raw prefix and all available sidecars were preserved before another launch.

A subsequent operator-planned retry uses process-only `__GLX_VENDOR_LIBRARY_NAME=mesa LIBGL_ALWAYS_SOFTWARE=1`; a separate operator `glxinfo` preflight reports llvmpipe. That preflight alone does not prove which renderer the original uses or that the game is stable. No system configuration change follows from this plan.


## Third launch: grounded running branch confirmed

[The bounded live03 report](../reference/running-knife-live03.json) records a complete247-frame capture, closed normally by its120-real-second limit. Strict analysis and `--require-running` pass. Raw capture SHA256 is `749a6288713ac86a64ab66d1ef6a29145088bd30fafae11b298c19598593b933`. Player.log and capture/provenance/input records were copied to ignored `build/running-attack-probe/live-03-evidence` before any later profile launch.

This process used operator-scoped `__GLX_VENDOR_LIBRARY_NAME=mesa LIBGL_ALWAYS_SOFTWARE=1` with `-force-gfx-direct`. The game's own Player.log confirms Mesa26.2.2-arch1.1, OpenGL4.6 Compatibility Profile and llvmpipe, not merely the external glxinfo preflight. The operator reduced the window to800×450 before arming. Observed throughput was about2.05frames/s; maximum sampled scaled-game-time gap is.333344s. No global system graphics setting was changed. This one bounded capture does not establish a permanent crash fix or acceptable ordinary rendering performance.

The first mistakenly named `.jsonl.arm` file had no effect. After the correct `.arm` appeared, ordinary movement and fresh attacks produced stationary20110011, an air-flagged20110012 and then a grounded running20110014. At sample199 the action holds Knife33, reads move/sprint thresholds4.5/7 and selects20110014 with moving=true, air/water/sprint=false and first-person=false. The sampled velocity magnitude is4.99197m/s and grounded=true. Prior sampled velocity is not the exact value at skill selection. Stamina changes100→95 without stat edits.

Samples200 and201 simultaneously observe that active skill and FullBody/current `RunSwordAttack` at layer weight1 and clip weight1, with normalized times.249090046 and.4981801. The recorded controller/avatar are FemaleAnimator2/FemaleAvatar; the compatible installed asset hash ties the state to the separately recovered408 metadata. Animator.speed is1.37. The two normalized-time measurements correspond to approximately1.37 authored seconds per sampled scaled-game second, but their coarse spacing does not resolve transition or callback timing. This confirms the live running branch; it does not prove a dense trajectory.

The trace explicitly records both animation and physical movement. At the first active-pose sample, m_LastMove and Animator.deltaPosition are zero; at the next they both read source-world `[.414544970,0,1.32395017]`. Physical position and velocity evolve separately. These end-of-frame readings do not justify applying a raw sampled delta directly to the player, claiming an exact lag, or bypassing source motor/collision handling. The next snapshot still displays the running clip while its skill reference is cleared, another reason not to infer action state from pose alone.

The exit sidecar records−9/signal9 at2026-10-06T18:20:34.321211Z; it is preserved with the capture and is not an exit0 claim. After closure the operator opened the [responsive SYSTEM menu](../reference/running-knife-live03-responsive.png), selected Quit Game and reached the [YES confirmation](../reference/running-knife-live03-quit.png), then confirmed exit and the process disappeared. Final Player.log is separately preserved. All63 manual-checkpoint files match the pre-session baseline hashes; this does not claim immutable autosave/preferences. Native gameplay implementation/physical checks remain independent of this original capture.
