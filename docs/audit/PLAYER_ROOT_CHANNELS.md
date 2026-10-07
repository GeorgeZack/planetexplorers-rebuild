# Offline female attack root-channel evidence

This audit compares serialized root channels with an existing, qualified original-engine running capture. It launches no game, executes no native engine routine, installs no motion asset, and does not enable ordinary moving attacks. The reproducible report is [player-root-channel-verification.json](../reference/player-root-channel-verification.json); the verifier is [verify_player_root_channels.py](../../rebuild/tools/verify_player_root_channels.py).

## Source inputs and bounded result

The installed `sharedassets1.assets` supplies FemaleAvatar777 and female Attack6/7/8/9 clips231/240/247/254 plus RunSwordAttack408. The report pins the entire asset, native ELF and preserved `female-running-root-06.jsonl` capture with SHA256, and hashes the verifier plus its scalar-decoder and original-capture-analyzer dependencies. No copied executable or recovered animation asset is distributed.

All five clips have RootT attributes7–9 and RootQ10–13, with the complete 127 scalar humanoid binding set 7–133, no Motion 0–6 channels, no streamed curves and 30Hz dense samples. Additional direct-transform channels animate auxiliary bones; they are not root-motion channels. Avatar human scale is 0.9932681322097778. All five have these same relevant flags:

- `LoopTime=false`, `LoopBlend=false`, `LoopBlendOrientation=true`.
- `LoopBlendPositionY=true`, `LoopBlendPositionXZ=false`.
- `KeepOriginalOrientation=true`, `KeepOriginalPositionY=true`, `KeepOriginalPositionXZ=false`.
- HeightFromFeet and Mirror false; start time, cycle/orientation offsets and level zero.

Consequently **zero horizontal root displacement is not justified**. Ordinary clips contain substantial planar RootT changes. Conversely, changing raw RootQ is not permission to turn the gameplay actor: the baked-orientation branch selects a common root quaternion.

For 408, `(RootT.xz(t)−RootT.xz(0))*humanScale`, with linear dense interpolation and Y=0, agrees with the preserved original running trials at every in-range observed timestamp. Trial 0 compares 822 samples with maximum error 3.546686μm; trial1 compares1773 samples with maximum error 10.562890μm. Exact-end comparison uses the original measured crossing bracket, separately reported. These errors are observations, not an adjustable acceptance tolerance.

The candidate ordinary displacement at each source `TP_EndAction` is approximately:

| Clip | EndAction seconds | X metres | Z metres |
|---|---:|---:|---:|
| Attack6 /231 |0.709125936|0.082956|1.375707|
| Attack7 /240 |0.836943209|0.394142|1.209979|
| Attack8 /247 |0.873209715|−0.325158|1.243879|
| Attack9 /254 |0.534950972|−0.216716|1.399448|

These are **candidate extracted trajectories**, not observed ordinary actor travel or collision results.

## Static native evidence

The inspected installed ELF SHA256 is`1eb5ad6ff19dddb960a71453603ed7c67403403186c9dfeadc798a6021c4d2a6`; BuildID`651e01fe3d5be6ff003977729e62e91ebd3390e6`. The report records exact half-open virtual-address byte ranges and their hashes. In these text/rodata ranges the file offset is VA−0x400000. Names below describe inspected behavior; stripped functions have no trustworthy recovered C++ symbol names.

1. `b99b53..b99be5` serializes explicitly named importer flags, binding offsets77b/77c/77d to LoopBlendOrientation/PositionY/PositionXZ and77e/77f/780 to KeepOriginalOrientation/PositionY/PositionXZ. This uses literal names, not a guess based on public WrapMode or importer labels.
2. `c1ffd0..c200da` reads RootT index entries at clip+514/518/51c, i.e. `m_IndexArray+4*{7,8,9}`, calls scalar sampler`c2c470`, and packs the three values as translation. This directly links the existing [humanoid property mapping](HUMANOID_CHANNEL_MAPPING.md) to root translation input.
3. Humanoid evaluator`c23710`, branch`c251d3..c252cd`, tests the named orientation flag and substitutes the same baseline quaternion into the sampled root transforms. PositionY then selects baselineY while preserving sampledXZ, using mask16fc7b0 with words`FFFFFFFF,0,FFFFFFFF,FFFFFFFF`. PositionXZ=false branches past replacement ofXZ. The byte-range report contains the branch through that decision; its target is`c25bb0`.
4. Registered `UnityEngine.Animator::get_humanScale` maps through`c51e60`→`bc96f0`→`bae650`; the final getter reads Human+1c0. In`c04470..c044dd`, that same Human+1c0 is loaded, broadcast and multiplied into a translation vector at`c044be`, before transform composition`c01b60`. This establishes the scale field and its use in translation, while the complete root-delta composition and final basis still require a bounded trace.

This is strong static support plus independent 408 numerical evidence. It is not a complete replay of the engine’s root-delta pipeline. In particular, the verifier deliberately reports `ordinary_runtime_motion_qualified=false`: transferring the 408 comparison to all four ordinary clips remains an explicitly identified inference at this checkpoint. The remaining question is the final root-delta composition/basis connection for this exact common flag set, including origin subtraction; no new limb retargeting proof is needed for that question.

## Source gameplay and existing captures

[AnimatorCtrl.OnAnimatorMove](../../Assets/Scripts/PeEntity/Cmpt/AnimatorCtrl.cs#L26) stores Animator deltaPosition and deltaRotation. [Action_Sword.UpdateSkillState](../../Assets/Scripts/PeEntity/Cmpt/Motion/PEAction/Action_Sword.cs#L209) leaves root-rotation application commented out, projects deltaPosition/Time.deltaTime onto the horizontal plane, limits requested velocity change to 12, and submits it to the physics motor. Therefore a qualified root trajectory still must feed the shared motor once per authoritative interval, with collision and existing facing ownership; it is not a teleport or another gameplay-yaw update.

The previous Attack6–9 body capture uses `ReferenceAnimatorBake.cs`: applyRootMotion=false at 232; independently assigned paused playable time followed by Animator.Update(0) at 261. Static rootWorld/rootPosition there cannot establish zero displacement. Summing that sampler’s running 408 deltaPosition produces an endpoint about 0.287688m from the qualified sequential running endpoint. That sum must not become a motion asset.

## Reproduction and scope

```sh
python3 rebuild/tests/test_player_root_channels.py
rebuild/build/unity-tools/bin/python rebuild/tools/verify_player_root_channels.py \
  --output rebuild/build/walking-audit/fresh-verification.json
```

The output must be fresh. The real verifier requires UnityPy and pinned local original assets/capture; the seven synthetic decoder/comparison tests require neither Unity nor UnityPy. They cover origin subtraction, scale, changing body RootQ, root Y exclusion, unsupported importer flags/offsets, missing/duplicate/Motion bindings, auxiliary direct transforms, invalid clocks/values, and ordered origin/end-bracket comparison. Synthetic tests do not establish Unity behavior. The real run independently repeats the preserved capture’s unchanged qualification before comparison.

No layer transition, blend, full humanoid solver, ordinary sequential playback, or source motor/collision parity is claimed. The comparison uses double-precision interpolation, not bit-identical Unity arithmetic. This checkpoint supplies a repeatable offline evidence path; it intentionally leaves ordinary gameplay disabled.
