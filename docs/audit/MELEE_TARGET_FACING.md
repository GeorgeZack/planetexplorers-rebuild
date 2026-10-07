# Adventure melee target acquisition and body facing

October7,2026. This increment connects source-derived attack direction and bounded target acquisition to the existing female third-person Knife33 host. It does not enable Wood Sword/Shield equipment, change damage or enemy counts, or introduce an aim-IK pose. The original game was not needed for extraction. [Shield composition](SHIELD_LAYER_CAPTURE.md) remains a separate gate.

## Source contract

[MainPlayerCmpt.UpdateOtherAction](../../Assets/Scripts/PeEntity/Cmpt/MainPlayerCmpt.cs) passes normalized requested movement to SwordAttack when the source default `AttackWhithMouseDir=false` is selected. [Action_Sword](../../Assets/Scripts/PeEntity/Cmpt/Motion/PEAction/Action_Sword.cs) falls back to current body forward for zero direction. Every running-action ResetAction updates its pending direction, even if a combo was already queued. Acquisition happens at OnStartAttack/OnComboAttack; the resulting direction is stored, not continuously recomputed as the target moves.

The serialized player query is a sphere3m centered1.5m above the player, layer25, strict horizontal angle<115degrees. `pe_sword_targeting` retains source view deduplication, centre-bone priority, inside-capsule zero direction and the unusual defence branch that does not lower the outer distance minimum. It is not nearest-enemy sorting. There is no extra line-of-sight test in this source call chain.

The new [hunt exporter](../../rebuild/tools/export_hunt_targeting.py) pins the installed hare's exact enabled trigger capsule on its Defence GameObject: radius1.2000000477, height3.069999933, local centre(0,.5,0), axisZ, layer25. Its identity hierarchy is separate from the hare's movement capsule. It also binds the original15 defence records and Body1 centre-bone identity to the recovered skeleton. [Generated metadata](../../rebuild/generated/pe_hunt_targeting_source.json) retains hashes, IDs and explicit adapters. Only the living, present pinned hunt cohort participates; existing source player1/hare4 conflict does not require that the hare has first perceived the player.

Without a selected nonzero direction, UpdateSkillState applies Quaternion.Lerp toward attack direction with serialized rotation factor60. [pe_melee_facing](../../rebuild/src/pe_melee_facing.c) uses normalized shortest quaternion interpolation, not linear Euler-angle interpolation; the exporter now owns that factor instead of the C# default3. A selected direction directly sets horizontal body yaw. Vertical aim IK is not reconstructed; a vertical-only selected direction keeps the native upright heading.

## Native integration and ownership

[pe_hunt_targeting](../../rebuild/src/pe_hunt_targeting.h) builds candidates from the actual current hare poses and the source query capsule. It uses stable encounter slot order as an explicit replacement for unobserved PhysX OverlapSphere order, and inclusive analytic sphere/capsule overlap as the native broadphase. Those policies can select a different target from Unity when several candidates qualify. The existing posed-capsule interpolation is unchanged.

`SandboxBladeFacing` in [the host](../../rebuild/src/pe_sandbox_view.c) is transient, instance/stage-bound state. `sandbox_blade_tick` stages action, stamina, pending direction, target result and heading together; invalid target geometry cannot charge a failed start. Every admitted attack press refreshes the pending continuation direction. Stow, equipment change, UI/camera cancellation, death and load clear this state. It adds no save payload.

Initial acquisition precedes movement/contact. During a combo transition, the retained outgoing root interval keeps its previous heading; the new stage acquires at the next fixed tick, from its unadvanced zero-time state. This preserves the existing native split-clock contract and is an explicit scheduling adapter to Unity callbacks. The pure rotation rule is source-derived, while production advances it on the existing60Hz combat ticks; it is not a claim of Unity display-update equivalence.

One authoritative body yaw rotates root motion, the visible body/knife and incoming defence. Camera yaw/pitch do not change. Active stages keep their acquired direction even if movement input or the target changes; the next admitted stage can choose again. There is no persistent target lock or saved aim. Tutorial combat remains its separate stationary lesson host; this change concerns Adventure combat and does not fork shared UI.

## Verification

Focused tests cover source query/centre identity, exact loaded hitbox bindings, every installed hare pose, stable candidate order, living/dead filtering, source cone, invalid late-candidate rollback, normalized quaternion interpolation and source default input. Actual host tests use real blade assets to check stationary body-facing retention, frozen stage direction, queue-left/request-right continuation, outgoing interval heading, equipment/cancellation and failed-start stamina rollback. Actor tests check that body, attachment and defence-facing snapshots share the new yaw while camera state remains unchanged.

The integration and physical check results are recorded in [PLAYTEST](../PLAYTEST.md). This audit does not establish improved hunt survival, full Animator transitions, masked shielding or exact original target enumeration.
