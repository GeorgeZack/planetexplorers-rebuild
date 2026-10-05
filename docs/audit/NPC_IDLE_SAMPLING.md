# Isolated tutorial NPC idle sampling

The nine tutorial humanoids have dedicated original Avatars and skinned assets. A player idle palette cannot safely substitute for those Avatar-specific poses. `reference_npc_bake.py` and `ReferenceNpcBake.cs` provide a separate optional original-runtime sampler; they do not replace the tested player sampler or modify installed game files. The new DLL compiles against the installed Unity5.2 assemblies. Original-runtime sampling completed for all nine NPCs in session `npcs-idle-01`: each closed output has reason `complete`, its own expected Avatar and one NormalIdle clip. The original tutorial remained visibly rendered for the [post-capture screenshot](../reference/original-training-npcs.png). This records original sampling evidence; it makes no claim about subsequent C rendering.

## Offline identity and runtime contract

The asset export supplies `build/reference-assets/npcs/npc-plan.json`: IDs9004,9005,9006,9009,9020,9031,9041,9047,9049; exact model/Avatar/controller names; and the effective NormalIdle chosen through the controller's override mapping. Female Avatars use the approximately2.000015s clip; male Avatars use the approximately2.266667s override. The launcher verifies each installed bundle/clip-asset SHA, the fixed roster/controller/gender contract, finite timing, loop/humanoid metadata and nonzero signed64-bit clip identity. Negative Unity bundle IDs remain negative.

Each NPC uses an independent capture component in the same original process. Before cloning, it finds exactly one enabled, active humanoid Animator matching the expected Avatar, controller and named model ancestor. It reads the live local hierarchy and creates only new Transform objects and a new Animator. Original objects receive no assignments, animation updates or input from this tool. The cloned Animator has no controller, events or root-motion application; its paused clip is sampled at authored times. Source clip events are recorded as metadata, not fired.

Each NPC produces one compatible `pe-animator-bake-1` JSONL and `pe-animator-bake-launch-1` sidecar, suitable for the existing strict analyzer. Sampling at30Hz plans606 total poses across the nine original idle clips. Every clip includes its exact terminal-duration sample. Cloned graphs are disabled and retained in a bounded static list until process termination, following the proven player sampler's retention strategy; the global limit remains10, with nine planned graphs. The observer is designed to wait for120 subsequent Unity updates and then write a console watchdog message after closing each capture. The capture header records that configured count, not proof the wait completed. No such watchdog message was observed in this session. Per-file output is capped at64MiB. A completed file does not by itself prove a clean original-process exit; the launcher separately records the raw process return code/signal.

Malus has two sibling helpers both named `Bip01 Mouth`. Recorded paths distinguish duplicate sibling names by zero-based source child order, `Bip01 Mouth#0` and `#1`; all ancestors receive the same rule. Clone object names remain unchanged to preserve Avatar binding. The asset rig supplies identically canonicalized explicit bone paths, preventing an ambiguous name-only mapping.

## Usage

Build and verify the plan without launching or touching profile settings:

```sh
python3 rebuild/tools/reference_npc_bake.py
python3 rebuild/tests/test_reference_npc_bake.py
```

An explicitly requested original-runtime session uses a dedicated save/preference profile:

```sh
python3 rebuild/tools/reference_npc_bake.py \
  --output "$HOME/.local/share/planet-explorers-npc-bake-reference/pose-captures/npcs-idle-01.jsonl" \
  --launch
```

After entering the tutorial, the desktop owner creates the printed `.arm` file. The supplied output is a session basename; actual poses go to nine files named `npcs-idle-01.npc-ID.jsonl`, with corresponding individual launch sidecars. The basename's launch file records the complete session roster; it is not a combined pose stream. A `.stop` file ends active observations. Fresh-path checks reject existing captures/provenance before launching.

Seven headless tests cover exact roster/source identities, signed64-bit IDs, timing bounds, source hash/path mismatch, duplicate JSON/roster rejection, build-only nonlaunch behavior, nine compatible provenance files, raw signal preservation, existing-output protection and the managed isolation/retention contract. The real plan validated and the DLL compiled. The separate runtime evidence below establishes completed original sampling; those outcomes are not inferred from the headless checks.

## Collision and initial placement evidence

`NpcEntityCreator.CreateTutorialLineNpc` uses each database `training_pos`, identity rotation and unit scale. Later story plots can change orientation; a mission arrival target is not an NPC spawn coordinate. The source `/Model` transforms are identity, with enabled nontrigger Y capsules on layer10. All nine have radius.370000005; eight have height1.75 and centerY.875. Anjum9020 has height1.975000024 and centerY.949999988, placing its capsule bottom.037500024 below its model root. Defence/limb trigger capsules are separate and are not body movement colliders.

`PEMotorPhysics` applies its own gravity and grounds against terrain/scene-static layers; a Rigidbody's serialized `useGravity=false` does not mean the NPC floats at its initial database Y. The C host therefore needs explicit floor placement and must retain root-to-capsule-bottom offsets. Andy and Moric's database starts already overlap the recovered room surface, so a downward-only sweep returns immediate contact; a bounded upward-clear search followed by downward grounding is required for those static native placements. That is a native placement policy, not a reproduction of all PhysX settling behavior.

Do not add these capsules as solid player blockers merely because they exist. The source DynamicsManager matrix excludes layer10-to-layer10 collision. `BiologyViewCmpt.BuildOutCollider` creates enlarged layer9 outer shapes (+.1 radius,+.2 height), but `ResetOutCollider` explicitly ignores all NPC/player outer-collider versus NPC/player model-collider pairs through the shared collider lists. Layer9 also excludes itself. These paths support room grounding while suppressing player/NPC physical obstruction. Native interaction picking can use recovered bounds separately; complete NPC AI, pushing, ragdolls and combat defence remain distinct systems.

## Recorded original-runtime result

Read-only inspection of `~/.local/share/planet-explorers-npc-bake-reference/pose-captures/npcs-idle-01.npc-ID.jsonl` confirmed nine closed `end` records with reason `complete`, totaling606 poses:

| NPC IDs | Samples per NPC | Recorded Avatars |
| --- | ---: | --- |
| 9004,9006,9031 | 62 | Adisa_MostafaAvatar, Vera_NovikovaAvatar, Viola_StrandAvatar |
| 9005,9009,9020,9041,9047,9049 | 70 | Moric_GahnAvatar, Allen_CarterAvatar, Anjum_NahasAvatar, AndyAvatar, Percy_MalikAvatar, Malus_TepesAvatar |

`npcs-idle-01.player.log` records nine complete captures, nine graph-inactivation messages with total retained count9 and no sampler error messages. Neither that copied log nor the isolated engine's Player.log contains `Post-cleanup alive120frames`. The JSONL files also contain no completed-watchdog event: `postCleanupUpdates:120` appears only in their headers as configuration. The source coroutine continues to its120-update wait after `Finish` closes the file; no control-flow defect was found that would inherently prevent it. Its completion was simply not observed before this process was closed. No rerun was performed solely to obtain that message.

The parent investigator captured the visibly running original tutorial after sampling, then requested ordinary window-manager close with `WM_DELETE_WINDOW`. The separate session exit record reports raw returncode−9/signal9, consistent with the already observed original-client self-kill close behavior. This is not an exit0 claim and does not establish120 post-cleanup updates. The retained evidence is completed nine-file sampling plus a rendered post-capture original scene, with explicit process-close status.
