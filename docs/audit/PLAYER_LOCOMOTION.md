# Player locomotion selection

`pe_player_locomotion` is the shared twelve-state native presentation adapter. [Facing/blending](PLAYER_STRAFING.md) and [directional assets](DIRECTIONAL_PLAYER.md) own current host/asset qualification. The initial six-clip scope is obsolete; this is not the full Unity Animator.

## Source/controller evidence

The inspected installed controller is `FemaleAnimator2`, object779 in `PE_Client_Data/sharedassets1.assets`, SHA256 `d9c65f820a2186116d9eb73d815fe07d9f46924ddfd103ee9510bf254f9a3d67`. `MaleAnimator`, object780, overrides its clips. The source prefab cannot supply the missing controller asset, but the installed type tree retains its compiled state/blend constants.

`Base Layer.GroundLocomotion.Move` runs at state speed1. Its top blend is BeatWeight; the normal branch uses MoveStyleH/MoveStyleV and then a two-dimensional RightSpeed/ForwardSpeed tree. Relevant normal-branch anchors are:

| Motion | RightSpeed | ForwardSpeed | Relevant leaf |
| --- | ---: | ---: | --- |
| Idle | 0 | 0 | NormalIdle |
| Walk forward | 0 | 2 | NormalWF |
| Run forward | 0 | 5 | NormalRF |
| Sprint forward | 0 | 8 | NormalSprint |

The normal tree also includes side/back/diagonal leaves. Forward walking/running pass through ForwardAngle blends: WF appears at the upper end of thresholds−25/−15; RF at−45/−30, with NormalRunForwardDown as another leaf. Their compiled leaf duration multipliers are1. The native selector does not reproduce these complete blends; ordinary locomotion sprint fallback is distinct from qualified Knife sprint-attack401.

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

[Header](../../rebuild/src/pe_player_locomotion.h): twelve enum-indexed loaded durations; no hard-coded actor indices. Input is measured velocity/ground/walk/sprint plus one accepted jump edge, and optional body-local directional axes. Select dominant axis with forward/back ties; preserve gait phase. Finite dt0..1, zero no-op; invalid input preserves state. No allocation/graphics/physics/events.

Preserve source-backed .85 Jump exit, .7 Land exit, strict .3s ledge delay and .1s jump-ground grace. Idle/gaits/Fall wrap, Jump/Land clamp. Discrete selected state and continuous drawn snapshot are distinct: .12s exponential weight adapter advances outgoing clips and phase-aligns gaits. Init/reset snaps Idle. Hosts map the same exact clocks/weights to body, attachments and defence; actions override the whole locomotion snapshot.

Speed scaling/sprint fallback and blending are adapters, not root-motion or full Animator parity. Tests: [test_player_locomotion.c](../../rebuild/tests/test_player_locomotion.c); [PLAYTEST](../PLAYTEST.md) owns physical/native fixture observations. Equipment/turn/lean/masked style blending remains separately qualified; do not request already-recovered directional leaves again.
