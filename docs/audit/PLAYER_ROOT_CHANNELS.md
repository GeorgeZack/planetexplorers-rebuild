# Offline female attack root-channel evidence

This audit compares serialized root channels with an existing, qualified original-engine running capture. It launches no game, initializes no engine, installs no motion asset, and does not enable ordinary moving attacks. The follow-up additionally replays four bounded, call-free arithmetic slices from the pinned native executable in a limited child process. The reproducible report is [player-root-channel-verification.json](../reference/player-root-channel-verification.json); the verifier is [verify_player_root_channels.py](../../rebuild/tools/verify_player_root_channels.py).

## Source inputs and bounded result

The installed `sharedassets1.assets` supplies FemaleAvatar777 and female Attack6/7/8/9 clips231/240/247/254 plus RunSwordAttack408. The report pins the entire asset, native ELF and preserved `female-running-root-06.jsonl` capture with SHA256, and hashes the verifier plus its scalar-decoder and original-capture-analyzer dependencies. No copied executable or recovered animation asset is distributed.

All five clips have RootT attributes7–9 and RootQ10–13, with the complete 127 scalar humanoid binding set 7–133, no Motion 0–6 channels, no streamed curves and 30Hz dense samples. Additional direct-transform channels animate auxiliary bones; they are not root-motion channels. Avatar human scale is 0.9932681322097778. All five have these same relevant flags:

- `LoopTime=false`, `LoopBlend=false`, `LoopBlendOrientation=true`.
- `LoopBlendPositionY=true`, `LoopBlendPositionXZ=false`.
- `KeepOriginalOrientation=true`, `KeepOriginalPositionY=true`, `KeepOriginalPositionXZ=false`.
- HeightFromFeet and Mirror false; start time, cycle/orientation offsets and level zero.

Consequently **zero horizontal root displacement is not justified**. Ordinary clips contain substantial planar RootT changes. Conversely, changing raw RootQ is not permission to turn the gameplay actor: the baked-orientation branch selects a common root quaternion.

For 408, `(RootT.xz(t)−RootT.xz(0))*humanScale`, with linear dense interpolation and Y=0, agrees with the preserved original running trials at every in-range observed timestamp. Trial 0 compares 822 samples with maximum error 3.546686μm; Trial 1 compares 1773 samples with maximum error 10.562890μm. Exact-end comparison uses the original measured crossing bracket, separately reported. These errors are observations, not an adjustable acceptance tolerance.

The candidate ordinary displacement at each source `TP_EndAction` is approximately:

| Clip | EndAction seconds | X metres | Z metres |
|---|---:|---:|---:|
| Attack6 /231 |0.709125936|0.082956|1.375707|
| Attack7 /240 |0.836943209|0.394142|1.209979|
| Attack8 /247 |0.873209715|−0.325158|1.243879|
| Attack9 /254 |0.534950972|−0.216716|1.399448|

These are **source-derived unblended trajectories**, qualified under the bounded predicate below. They are not observations of ordinary actor travel or collision results.

## Static native evidence

The inspected installed ELF SHA256 is`1eb5ad6ff19dddb960a71453603ed7c67403403186c9dfeadc798a6021c4d2a6`; BuildID`651e01fe3d5be6ff003977729e62e91ebd3390e6`. The report records exact half-open virtual-address byte ranges and their hashes. In these text/rodata ranges the file offset is VA−0x400000. Names below describe inspected behavior; stripped functions have no trustworthy recovered C++ symbol names.

1. `b99b53..b99be5` serializes explicitly named importer flags, binding offsets77b/77c/77d to LoopBlendOrientation/PositionY/PositionXZ and77e/77f/780 to KeepOriginalOrientation/PositionY/PositionXZ. This uses literal names, not a guess based on public WrapMode or importer labels.
2. `c1ffd0..c200da` reads RootT index entries at clip+514/518/51c, i.e. `m_IndexArray+4*{7,8,9}`, calls scalar sampler`c2c470`, and packs the three values as translation. This directly links the existing [humanoid property mapping](HUMANOID_CHANNEL_MAPPING.md) to root translation input.
3. Humanoid evaluator`c23710`, branch`c251d3..c252cd`, tests the named orientation flag and substitutes the same baseline quaternion into the sampled root transforms. PositionY then selects baselineY while preserving sampledXZ, using mask16fc7b0 with words`FFFFFFFF,0,FFFFFFFF,FFFFFFFF`. PositionXZ=false branches past replacement ofXZ. The byte-range report contains the branch through that decision; its target is`c25bb0`.
4. Registered `UnityEngine.Animator::get_humanScale` maps through`c51e60`→`bc96f0`→`bae650`; the final getter reads Human+1c0. In`c04470..c044dd`, that same Human+1c0 is loaded, broadcast and multiplied into a translation vector at`c044be`, before transform composition`c01b60`. This establishes the scale field and its use in translation, with the final root-delta path closed by the follow-up below.

## Completed source-to-delta follow-up

The initial report at commit`c5055ac7` was preserved before regeneration at`rebuild/build/walking-audit/checkpoint-c5055ac7/report.json`; Git retains its committed version. The new report sets `ordinary_source_extraction_qualified=true`, while `ordinary_clips_observed_sequentially=false` and `ordinary_runtime_motion_qualified=false`. These separate source proof, observations and implementation.

The closed chain is:

1. **Prior/current scalar values.** `c227e0..c228bd` normalizes the previous authored clock, samples RootT through`c1ffd0`, and stores the previous body root at result+f0. `c27310..c27387` reads current RootT indices7–9 directly from the evaluated scalar array. In the no-Motion0–6 case, `c237f3` and`c25678..c256a4` select the body-derived root branch. `c25378..c254c8` copies these previous/current translations into the extracted-root samples. No limb solver supplies those translations.
2. **Starting basis cancels.** Named serializer`b998c6..b998da` binds `m_StartX` to clip+430. Its projected quaternion is at local stack+660. With KeepOriginalOrientation=true, `c27260..c272f5` computes `normalize(offsetQ * conjugate(projectedStartQ))`. At zero orientation offset, `c24ff3..c25064` multiplies that adjustment by projectedStartQ, normalizing the result. In real arithmetic this is identity for any valid starting orientation, including the markedly different ordinary clips. The LoopBlendOrientation branch copies this same root quaternion to previous/current/end samples. Changing bodyRootQ does not rotate translation.
3. **Origins cancel.** `c24f1d..c24ff3` applies common additive positional offsets; KeepOriginalPositionXZ=false skips the additional XZ offset branch. PositionY baking then selects the same Y for previous/current samples. For LoopTime=false and advancing time, `c24c18..c24d2d` computes `rotate(conjugate(previousRootQ), currentRootT − previousRootT)`. The subtraction is explicit at`c24ccb`; vector rotation is`c24cf7`→`c1a740`. Equal clocks retain the initialized zero delta. Since the extracted basis is identity and the additive offsets are common, this reduces to consecutive RootT differences in XZ, with Y=0. Summing intervals therefore gives `RootT(t)−RootT(0)`, without a dependency on absolute StartX translation.
4. **Interval representation.** `c24be0..c24c14` forms the interval factor from authored times, duration and playback scale. `c24b7b..c24b8a` divides the displacement by that factor and stores it at output+10; `c1f830..c1f843` multiplies the same representation's interval and vector back together. There is no extra per-frame integration from independently sought poses. Rounding in division/multiplication remains distinct from the report's double interpolation.
5. **Scale and public delta.** Animator update`bc6e05..bc6e48` reconstructs that delta using`c1f830`, multiplies by actor scale and Avatar human scale (Avatar+48-relative Human+1c0, addressed as Avatar+relative+208). `bc6eef..bc7000` applies the actor input quaternion once and writes Animator+1c0/1c4/1c8. Its identity-basis branch is`bc7c70..bc7c8c`. The registered `INTERNAL_get_deltaPosition` wrapper`c51ec0..c51ef5` calls`bc4920..bc4939`, which returns exactly those three fields. Thus world translation cancels, and actor facing belongs downstream of the authored local delta.

This qualification is restricted to the pinned female Avatar/clips, exact listed importer flags, no Motion channels, nonloop advancing playback, unit source/actor scales and one unblended clip. State transitions, layer mixtures, matching/warping, actor scale changes, mirroring and physics remain excluded. The verifier checks the source StartX is finite with unit scale and a normalized quaternion; it does not assume StartX is identity.

### Independent arithmetic replay

The tool copies only call-free projection`c1dc20..c1dd5b`, quaternion product`c1a7a0..c1a7fb`, vector rotation`c1a740..c1a798`, and normalization slice`c1dd1a..c1dd5b`. Each instruction, range hash and RIP-relative rodata reference is checked against the pinned installed ELF. The normalization wrapper explicitly restores its inherited XMM2 coefficient from source rodata16fb440. No ELF loader, Animator, Avatar engine object, callback, allocator, or broad engine function is invoked. Private pages transition from writable to read/execute; execution is a child with a5-second CPU limit,15-second parent timeout and disabled core dumps. Unsupported architecture, binary hash or floating-point rounding mode fails closed.

Exact integer-valued cases establish quaternion operand order and vector-rotation handedness. The replay then uses all five actual StartX quaternions and seven synthetic starting yaws from−3 to+3 radians. Four source bases and all synthetic bases cancel exactly. Attack7 leaves native quaternion Y=−2.9802318834981634e−8; the greatest tested planar-vector difference is8.940696716308594e−8m on the1.4m vector. The report preserves that residual. It uses no user-adjustable acceptance tolerance and makes no bit-identical claim for the simpler source-channel formula. This is measured floating-point residue after a statically established algebraic cancellation, not an unexplained orientation fit.

The existing408 engine comparison still independently covers822/1773 observed in-range samples. Its separately interpolated terminal brackets differ from the formula by65.320174μm/19.499102μm; those figures are not folded into the observed-knot maxima. Ordinary clips have not been sequentially run in the engine during this task.

## Source gameplay and existing captures

[AnimatorCtrl.OnAnimatorMove](../../Assets/Scripts/PeEntity/Cmpt/AnimatorCtrl.cs#L26) stores Animator deltaPosition and deltaRotation. [Action_Sword.UpdateSkillState](../../Assets/Scripts/PeEntity/Cmpt/Motion/PEAction/Action_Sword.cs#L209) leaves root-rotation application commented out, projects deltaPosition/Time.deltaTime onto the horizontal plane, limits requested velocity change to 12, and submits it to the physics motor. Therefore a qualified root trajectory still must feed the shared motor once per authoritative interval, with collision and existing facing ownership; it is not a teleport or another gameplay-yaw update.

The previous Attack6–9 body capture uses `ReferenceAnimatorBake.cs`: applyRootMotion=false at 232; independently assigned paused playable time followed by Animator.Update(0) at 261. Static rootWorld/rootPosition there cannot establish zero displacement. Summing that sampler’s running 408 deltaPosition produces an endpoint about 0.287688m from the qualified sequential running endpoint. That sum must not become a motion asset.

## Reproduction and scope

```sh
python3 rebuild/tests/test_player_root_channels.py
rebuild/build/unity-tools/bin/python rebuild/tools/verify_player_root_channels.py \
  --output rebuild/build/walking-audit/fresh-verification.json
```

The output must be fresh. The real verifier requires UnityPy and pinned local original assets/capture; the ten synthetic decoder/comparison tests require neither Unity nor UnityPy. They cover origin subtraction, scale, nonidentity starting bases/common offsets, start-transform/native index-table validation, changing body RootQ, root Y exclusion, unsupported importer flags/offsets, missing/duplicate/Motion bindings, auxiliary direct transforms, invalid clocks/values, and ordered origin/end-bracket comparison. Synthetic tests do not establish Unity behavior. The real run independently repeats the preserved capture’s unchanged qualification before comparison, then runs the bounded arithmetic child. It requires Linux x86-64, objdump and round-to-nearest; ordinary unit tests do not execute native slices.

No layer transition, blend, full humanoid solver, ordinary sequential playback, or source motor/collision parity is claimed. The comparison uses double-precision interpolation, not bit-identical Unity arithmetic. This proof qualifies bounded offline source extraction. The subsequent [walking implementation](SANDBOX_WALKING_KNIFE.md) exports and integrates these curves; its C tests and remaining graphical acceptance are separate evidence. The proof report itself continues to leave `ordinary_runtime_motion_qualified=false`.
