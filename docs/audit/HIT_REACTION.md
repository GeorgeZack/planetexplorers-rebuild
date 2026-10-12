# Source-derived hit-reaction decisions

October12 presentation follow-through: the [animation contract](REACTION_PRESENTATION.md) and disconnected `pe_whacked` recover what Whacked and Repulsed play and when Whacked ends. Reaction poses are now baked and staged as an uninstalled standalone group; nothing presents them.

October12 follow-through: [HumanPhyCtrl consumption](HUMAN_PHY_CTRL.md) of the Repulsed velocity request is qualified from installed CIL in the disconnected `pe_human_phy`. Reaction poses, hit IK and host integration remain gated.

October10 follow-through: [admitted Repulsed](REPULSED_ACTION.md) now has source-bound female curves, row6 relations and a disconnected C clock/request owner. Installed Unity5 static IL also qualifies the zero-direction angle as90. HumanPhyCtrl consumption, reaction poses, hit IK and host integration remain gated; the decision owner below still changes no live damage or movement.

October7,2026. [pe_hit_reaction](../../rebuild/src/pe_hit_reaction.h) is a disconnected, pure decision owner. It does not change live damage, activate shielding, admit actions, animate a reaction or move any actor. The ordinary Sword/Shield presentation gate remains intact.

[The exporter](../../rebuild/tools/export_hit_reaction.py) follows actual FemaleModel BeatParam11471154 through BiologyViewRoot11424092 and EntityPlayer Motion_Beat11400038. It recovers the two authored three-key angle curves and source air multiplier rather than treating class defaults as the model configuration. [Generated metadata](../../rebuild/generated/pe_hit_reaction_source.json) records input hashes and bindings.

[Motion_Beat.ApplyMoveEffect](../../Assets/Scripts/PeEntity/Cmpt/Motion/Motion_Beat.cs) scales force by the force-angle curve, scales thresholds by the separate threshold-angle curve, and halves threshold scale while InAir. It then uses strict ordered comparisons. Default player thresholds are250/400/750/1750. The mounted shield buff30200177 supplies250/300/9999/9999; merely equipping a shield does not establish that buff is active. Hare trigger10100/10400/10250 supplies force100/400/250 respectively.

The output distinguishes Null, Whacked, Repulsed, Wentfly and Knocked effects. Whacked requests action5, Repulsed requests6, and both Wentfly and Knocked request7. Only Null and Repulsed request hit-IK. These are requests, not successful action admission. The caller must supply permitted hit context, current summed thresholds and source Vector3.Angle(forward,forceDir). Damage, Rigid assignment, buff lifetime, force direction and lifecycle ownership remain outside this helper.

Finite nonnegative inputs and angle0..180 are validated before publishing output. Thresholds are deliberately not sorted: source comparison order matters. The curve arithmetic retains the previously qualified bounded polynomial evaluation order; this is not a new full animation-curve engine. Source method/binding mutation tests prevent regeneration from silently accepting changed contracts.

Strict optimized tests and focused ASan/UBSan with leak detection pass. Nineteen exporter tests pass with no skips under the local UnityPy interpreter, and `--check` passes. Tests cover exact thresholds, front/side/back angle curves, air scaling, mounted-buff snapshots, unsorted thresholds, action mapping and failure rollback. No original game, graphics or desktop input was used for this extraction.

Before connecting this owner, recover actual reaction poses, action relations, collision/motor behavior and source hit-IK. In particular, Action_Repulsed tests its previous `m_MoveDir` before assigning the new force direction; do not substitute invented knockback or stun timing. No numeric damage rebalance is justified by this extraction. The inspected shield factor, per-hit wear, breakage and damage scaling already have tested native owners.
