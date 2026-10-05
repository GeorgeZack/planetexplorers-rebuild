# Player locomotion selection for recovered clips

The standalone [pe_player_locomotion.c](../../rebuild/src/pe_player_locomotion.c) selects the six recovered normal locomotion clips from actual host movement and maintains a bounded authored animation clock. It provides walking/running instead of a frozen avatar while keeping its approximation explicit. It does not implement Unity's full blend graph, movement physics, root motion, attack selection or weapon-style layers. [Animator baking](ANIMATOR_BAKE.md) documents how the original Unity runtime evaluated the skeletal samples; source-derived samples do not make a simplified selector equivalent to the original Animator.

## Source/controller evidence

The inspected installed controller is `FemaleAnimator2`, object779 in `PE_Client_Data/sharedassets1.assets`, SHA256 `d9c65f820a2186116d9eb73d815fe07d9f46924ddfd103ee9510bf254f9a3d67`. `MaleAnimator`, object780, overrides its clips. The source prefab cannot supply the missing controller asset, but the installed type tree retains its compiled state/blend constants.

`Base Layer.GroundLocomotion.Move` runs at state speed1. Its top blend is BeatWeight; the normal branch uses MoveStyleH/MoveStyleV and then a two-dimensional RightSpeed/ForwardSpeed tree. Relevant normal-branch anchors are:

| Motion | RightSpeed | ForwardSpeed | Relevant leaf |
| --- | ---: | ---: | --- |
| Idle | 0 | 0 | NormalIdle |
| Walk forward | 0 | 2 | NormalWF |
| Run forward | 0 | 5 | NormalRF |
| Sprint forward | 0 | 8 | NormalSprint |

The normal tree also includes side/back/diagonal leaves. Forward walking/running pass through ForwardAngle blends: WF appears at the upper end of thresholds−25/−15; RF at−45/−30, with NormalRunForwardDown as another leaf. Their compiled leaf duration multipliers are1. The selector cannot reproduce these blends with the limited six-clip locomotion set. In particular **NormalSprint is not among the current ten baked clips**; choosing RF and adjusting its clock is a fallback, not recovered sprint animation.

`Motion_Move_Human.cs` declares default walk/run/sprint speeds2/5/8m/s and AnimLerpSpeed5. `Action_HumanMove.cs` feeds ForwardSpeed/RightSpeed using movement transformed into player-local coordinates; ordinary updates lerp with5×deltaTime, while aiming uses a different immediate path. `Motion_Move_Human.UpdateAnim` supplies OnGround. Source movement commonly uses desired movement direction, so the native selector's use of actual host velocity deliberately differs when blocked by a wall.

The installed jump substates are `Base Layer.GroundLocomotion.Jump.Jump`, `.Fall` and `.Land`, each state speed1. Jump/Fall blend on JumpForward thresholds0/7; Land uses3/10. These branches include normal, running and equipment-specific leaves. The selector uses only the baked normal leaf.

| Source transition | Exit condition | Serialized normalized transition duration |
| --- | --- | ---: |
| Jump→Fall | normalized exit .850000024 | .150000006 |
| Jump→Land | OnGround, no exit-time requirement | .105642959 |
| Fall→Land | OnGround, no exit-time requirement | .200000003 |
| Land→Move | normalized exit .699999988 | .300000012 |
| Land→Move | StartMove, no exit-time requirement | .300000012 |

All listed transitions have `m_HasFixedDuration=false`: these durations are normalized fractions, not seconds. The selector does not apply their blend intervals. A compact recovered node/transition fixture is saved in ignored `rebuild/build/player-locomotion-source.json`, including the source hash and exact node arrays.

Additional EndJump, water and competing-action transitions are present but are not reproduced. The source `Action_Jump` issues Jump, resets EndJump, and uses a .1s elapsed-time guard before accepting ground contact. `Motion_Move_Human.CheckFall` normally requests Fall after more than .3s since ground contact. Its separate height expression is literally `lastGroundHeight - currentY < -3`, not an ordinary downward fall-distance comparison; this selector does not reinterpret or implement that branch. Action-mask permissions and the full trigger graph can affect which request actually takes effect.

## API and bounded behavior

The public [header](../../rebuild/src/pe_player_locomotion.h) defines enum order IDLE/WALK/RUN/JUMP/FALL/LAND. `pe_player_locomotion_name()` maps these to NormalIdle, NormalWF, NormalRF, Jump, Fall and Land. The host reads those clips' actual durations from its loaded actor and calls `pe_player_locomotion_init(state,durations)`. No baked part index, clip index or fixed duration is assumed.

Each `step(state,input,dt)` consumes actual velocity (the training frontend uses horizontal position displacement divided by the fixed step, rather than the controller’s pre-collision requested velocity), grounded state, walk/sprint mode and **one accepted physical jump edge**. Held Space is not a jump edge. It returns the selected enum and authored seconds in the state. Time must be finite and between0 and1s; zero is a no-op. Invalid values leave the state unchanged. The module allocates nothing and makes no graphics, physics or event calls.

The current presentation policy is:

- Ground speed above.05m/s selects walking when walk is set without sprint, otherwise running. Below that threshold selects idle. This dead zone and discrete selection are provisional; there is no 2D blend, strafing or reverse clip selection.
- WF runs at actual horizontal speed/2 and RF at speed/5, each clamped to.25–2 times authored rate. Thus8m/s sprint uses RF at1.6×. These rates are presentation approximations based on source speed anchors, not recovered Animator speed multipliers. Walk↔run preserves normalized gait phase; other changes reset the selected clip to its first frame. New states do not consume that update's delta, another explicit bounded approximation.
- An accepted jump selects Jump at time0. While airborne it switches to Fall at the source .85 normalized Jump exit; it does not infer that transition solely from vertical velocity. Natural ledge departure waits strictly more than.3s before Fall. Short ground-probe gaps do not replay landing. The .1s jump-ground grace avoids immediately cancelling a just-accepted jump.
- Stationary contact after a jump/fall selects Land; Land exits at.7 normalized time. Actual movement immediately selects its gait, approximating the source StartMove path. This does not prove an actual source StartMove trigger was delivered on that frame. No transition crossfade is applied.
- Idle/walk/run/fall wrap by their loaded authored durations. Jump/land clamp to their duration if necessary. Air-age tracking saturates and clip clocks remain bounded in long sessions.

The selector does not use the baked Attack6–9 clips or authorize combat. Equipment-specific normal/sword/rifle poses, FP/TP layering, IK, turn/lean blending, humanoid retargeting, root displacement and collision remain separate work. Choosing normal locomotion while a weapon is equipped is therefore still an approximation. Rendering integration remains owned by the training frontend; this audit does not claim a graphical locomotion smoke succeeded.

## Validation

[tests/test_player_locomotion.c](../../rebuild/tests/test_player_locomotion.c) exposes `pe_test_player_locomotion()`. Strict C11 compilation and standalone AddressSanitizer/UndefinedBehaviorSanitizer/leak checks pass. Fixtures exercise idle→walk→run, preserved gait phase, speed-scaled sprint fallback, actual zero-speed stop, accepted jump and authored Jump→Fall timing, stationary/moving landing, .1s ground grace, strict .3s ledge-fall threshold, brief ground gaps, immediate movement out of Land,10,000 update clock bounds, invalid input and atomic initialization failure. These checks establish deterministic selector behavior, not Unity blend or root-motion parity.


## Next directional/blend recovery scope

The normal branch contains19 unique leaves: NormalIdle; NormalWF/WB/WL/WR; NormalRF/RB/RL/RR and RFL/RFR/RBL/RBR; NormalRunForwardDown; NormalSprint; and NormalWalkTurnLeft/Right plus NormalRunTurnLeft/Right. The existing bake covers only Idle/WF/RF from that branch. A useful next bounded expansion would first bake the missing normal directional and sprint leaves on the same source rig, preserving exact durations and loop flags. Equipment-style branches should be a subsequent explicit selection, not silently mixed into the normal branch.

The next controller fixture should preserve node2's serialized blendType2, RightSpeed/ForwardSpeed anchors, parent MoveStyleH/V and BeatWeight branches, ForwardAngle child thresholds, and source local-velocity smoothing. The presence of compiled pair/neighbor arrays does not establish a verified blending formula. Validate candidate weights and synchronized clocks against the original Animator at selected parameter points: zero, each authored anchor, an interior forward walk/run point, side/back diagonals, and a turn/lean threshold. Isolated leaf baking cannot validate those blends. Only then add a bounded multi-clip pose blender with compatible-skeleton checks and use actual source-local movement direction; keep host/world root motion separate. This staged scope avoids treating today’s velocity-scaled forward fallback as directional Animator parity.
