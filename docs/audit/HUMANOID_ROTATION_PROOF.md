# Offline humanoid rotation proof

October6, 2026. This is an extraction-tool increment, outside runtime CORE/FRONTEND. No original/native game, graphics context or desktop input was used. It implements and checks a useful part of humanoid evaluation while deliberately retaining the unresolved limb results. It does not install replacement animation assets or enable ordinary shield gameplay.

## What is implemented

The experimental C [joint helper](../../rebuild/tools/human_rotation_math.h) reproduces the exact installed `Avatar.GetZYRoll` numerical operation for a bounded domain. [The binary audit and replay](UNITY5_HUMAN_ROTATION.md) qualify272,217 inputs bit for bit, including every actual female Avatar axis record, sign/range combinations and all92 Attack6 muscle extrema. Its portable square-root fallback has separate measured error; the SSE result is specific to the tested build/CPU arithmetic.

[Channel mapping](HUMANOID_CHANNEL_MAPPING.md) establishes the127 source humanoid scalars: root7..13, goals14..41, and92 muscles42..133. Existing exporters already recover these values. Normalized muscles are not clamped to±1. HumanTrait, capture-node, Avatar-skeleton, human-skeleton and axes indices are distinct namespaces. The comparator joins exact paths, validates reciprocal indices and preserves signed local scale; right distal finger matrices can contain reflections that quaternions alone cannot reproduce.

[probe_humanoid_rotation.py](../../rebuild/tools/probe_humanoid_rotation.py) reads an existing closed, strictly validated unlayered bake and tests the candidate local basis:

```text
local rotation = preQ * GetZYRoll(sampled muscles, source axes) * inverse(postQ)
local basis = normalized candidate rotation with the source signed local scale
```

It compares all40 mapped non-root human bones, including the12 limbs that fail. Hips are separately reported as a body/root omission; unbound eye/finger nodes remain absent. The current candidate covers source axes type1 only. It does not estimate translation, world-space bone positions, limb goals, twist distribution, root extraction, layer masks or IK.

The tool requires matching capture launch-plan asset SHA, clip IDs/names/loops/clocks, a fresh installed Avatar decode matching the rig, and the hash-bound arithmetic helper/proof. The Avatar comparison canonicalizes JSON tuple/list representation while preserving numeric serialization. Inputs are hashed before and after comparison; changed inputs fail. Reports require a fresh output path and exclusive creation. No report is accepted merely because its clip has a familiar name.

## Loop-pose correction recovered offline

Extending the initial Normal/Attack comparison to Sword captures exposed steadily growing local-basis errors in SwordWB441 and AttackIdle90. Both have source `m_LoopBlend=true` and serialized delta poses. The body52 and left/right hand20+20 deltas match the start-minus-end muscle difference for the inspected clips.

Native disassembly independently establishes the positive correction. Branches at `0xc24097..0xc240d3` require both LoopTime and LoopBlend. The call at `0xc26570` scales the serialized delta pose through `0xc3a0e0`; instructions at `0xc3a310` multiply52 body DoF values, and `0xc3a338` multiplies both20-value hand groups. The subsequent `0xc39520` call adds them to the sampled pose; body/hand additions are at `0xc39900..0xc39970`. There is no subtraction or fused multiply-add:

```text
corrected muscle = float(sampled muscle + float(delta * phase))
```

The phase mapper at `0xc1fbd0` includes cycle/state offsets, direction and integer-loop handling. Serialized clip start/stop/cycle fields are bound by the named reader at `0xb999d1..0xb99a30`. For the comparator's forward, zero-start, zero-offset interior samples, phase is normalized authored time. The tool rejects unsupported mirrored/offset branches when applying the correction. It uses corrected terminal keys at the endpoint; the original integer-loop path wraps to the start. These are seam-equivalent through the delta but can round differently. Dense interpolation and terminal evaluation remain explicit diagnostic adapters, not a general animation clock implementation.

Only muscle channels receive this correction in the probe. Serialized root/goal delta poses are retained as unresolved work. The report preserves both uncorrected and corrected residuals for looping clips, so the improvement does not hide earlier disagreement.

## Evidence from existing bakes

[The compact comparison manifest](../reference/humanoid-local-rotation-comparison.json) retains per-bone maxima, per-clip raw/corrected metrics, source IDs and every input/report hash. Full local reports remain under `rebuild/build/humanoid-proof`.

| Existing closed capture | Clips | Poses | Maximum non-limb basis error after correction |
|---|---:|---:|---:|
|female-clips-06|10|345|6.76185e−7|
|female-sword-01|10|304|3.36807e−6|
|female-sword-backward-02|2|48|2.47121e−5|

Across697 poses, the tool compares27,880 local3×3 bases. The anatomical grouping is28 non-limb bones and12 upper/lower arms, hands, upper/lower legs and feet. **The grouping is not a relaxed acceptance threshold.** Remaining non-limb maxima are near terminal samples in SwordWF/RB. SwordWB's maximum non-limb residual falls from about0.20145 before loop correction to5.88524e−7 afterward. These numbers are absolute matrix-component errors, not meters or whole-character pose errors.

The12 limb bones retain substantial disagreement, with maximum1.35407013 in Attack6's right lower arm. Parent errors also affect descendant world poses, so matching a finger's local matrix does not establish a matching hand position. The candidate is not ready to replace engine-baked skeletons or supply blocking defence capsules.

A separate direct-channel check confirms Attack6's3,978 dense floats are exactly34×117 with no hidden frame scalar. Nine complete direct Transform tracks match34 captured phases within1.24850e−6 per local-matrix component. Thus large adjacent-frame values in this clip are present in the evaluated reference too; changing the stride to make them look smoother would corrupt source data. Its script/report remain local under `rebuild/build/retarget-proof`.

## Checks and reproduction

`make -C rebuild humanoid-rotation-test` runs the C helper's independent native golden values, invalid-input/alias/rounding-mode checks and35 CPU-only Python cases. These pass with no skips. Focused C ASan/UBSan with leak detection passes. The helper also passed the full arithmetic replay; no full runtime graphics/core rerun was needed because this increment changes extraction tools only.

Build and qualify the shared library with [the exact oracle commands](UNITY5_HUMAN_ROTATION.md). Rebuilt libraries need their own matching arithmetic report. Then, using fresh output names:

```sh
rebuild/build/unity-tools/bin/python rebuild/tools/probe_humanoid_rotation.py \
  --rig rebuild/build/reference-player-starter/player-female-starter-rig.json \
  --traits rebuild/build/pose-analysis/female-starter-tp-attack6-v2.json \
  --capture /home/admin/.local/share/planet-explorers-bake-reference/pose-captures/female-clips-06.jsonl \
  --library rebuild/build/humanoid-proof/human_rotation_math.so \
  --rotation-proof docs/reference/unity5-human-rotation-arithmetic.json \
  --game '/home/admin/.local/share/Steam/steamapps/common/Planet Explorers' \
  --output rebuild/build/humanoid-proof/new-comparison.json
```

The same command with the two Sword capture paths reproduces the other rows. Optional UnityPy is needed only for installed-asset decoding; numerical/unit checks use ordinary C/Python.

## Next boundary

The next offline target is the source limb solver: body/root and goal coordinate transforms, human scale and the Avatar's arm/forearm/upper-leg/lower-leg twist distribution. All four twist factors are0.5 for this Avatar; that fact alone is not a solver. Derive the actual operation and test against the same failed limb matrices before touching production assets. Then validate complete local/world poses and independently advancing base/block composition against the preserved shield clone evaluations.

This work shows that important engine-specific animation computation can be recovered without another game session. It does not yet prove that completing the entire humanoid solver is cheaper than a prepared one-time engine bake. Keep those choices evidence-based. Ordinary native four-hare completion and partial-progress physical reload remain separate acceptance work; no headless pose proof substitutes for them.
