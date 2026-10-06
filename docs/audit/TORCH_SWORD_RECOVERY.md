# Torch Sword pose recovery

Initial inventory: October5, 2026. Updated October6: the exact ten-leaf female Sword capture is complete (304 poses), and a combined31-clip body/wood-attachment plus40-clip defence group is staged and headlessly checked. These are recovered authored poses, not an original held-Torch observation or installed runtime replacement. Existing installed `player`, `player-torch` and `player-torch-stowed` groups remain unchanged. See [the combined stage](STARTER_SWORD_POSE_GROUP.md).

The current NormalIdle bake places the Torch's original `(0,0,.45)` light at model-local `(0.197725,0.741991,0.462064)`. The native PEAC sample agrees with the original captured `mountMain` matrix. The held model is horizontal at roughly .71–.80m above the player root; a rear camera can obscure it. This does not establish a broken attachment transform. The source Torch requests Sword movement style, while the current bounded native host selects Normal locomotion.

## Exact source leaves

`Motion_Move_Human.UpdateAnimState` maps movement style1 to `(cos20°,sin20°)`, using `MoveStyleH/V`. The installed `FemaleAnimator2` controller779 has the corresponding authored blend point approximately `(.94,.342)`. Its idle leaf is **AttackIdle**, not a clip named SwordIdle. The new `sword_states()` inventory validator follows the exact branch ancestry, verifies that style point and each leaf's source object reference, and retains branch parameters/coordinates with the clip plan. Other blend inputs, masks, transitions and IK are not thereby implemented.

| Role | Clip | Female ID | Female duration seconds | Loop |
| --- | --- | ---: | ---: | --- |
| Idle | AttackIdle |90|1.7333335876464844|yes|
| Forward walk | SwordWF |133|1.0999984741210938|yes|
| Forward run | SwordRF |435|.6666679382324219|yes|
| Forward sprint | SwordSprint |113|.6000001430511475|yes|
| Stationary jump | SwordJump |756|1.0333404541015625|no|
| Stationary fall | SwordFall |757|.6666717529296875|no|
| Stationary land | SwordLand |758|1.066680908203125|no|
| Running jump | SwordRunJump |272|1.4000015258789062|no|
| Running fall | SwordRunFall |284|1.0000038146972656|yes|
| Running land | SwordRunLand |279|.23333740234375|no|

The Move state's node paths are `0→1→41→42` for idle, `0→1→41→61→63` for walk, `0→1→41→43→45` for run, and `0→1→41→74` for sprint. The `Base Layer.GroundLocomotion.Jump.{Jump,Fall,Land}` states select stationary leaves through `0→1→3` and running leaves through `0→17→19`. Jump/Fall use JumpForward thresholds0/7; Land uses3/10. Ground turns and slopes also contain Normal clips, so choosing these forward leaves alone is still a bounded locomotion adapter.

The installed source asset SHA-256 is `d9c65f820a2186116d9eb73d815fe07d9f46924ddfd103ee9510bf254f9a3d67`. Inventory also verifies male override780 identities; the prepared Torch Sword exporter currently requires the female group used by the native avatar. Male controller availability must not be mistaken for a recovered male runtime actor.

## Reproducible tools and completed supervised sampling

The Python and C# sampler allowlists include all ten leaves above. `--preset torch-sword` selects those ten without changing the old default Normal/attack selection. The current sampler allows at most32 selected clips/retained graphs and3,600 samples; the earlier ten-clip bound was expanded for directional recovery. This preset plans304 female samples at30Hz; the original inactive-graph retention and120-update cleanup verification remain unchanged. This does not instrument a running process. Read [the sampler audit](ANIMATOR_BAKE.md) and [probe instructions](../../rebuild/tools/REFERENCE_POSE.md) before launching.

Offline preparation already completed:

```sh
rebuild/build/unity-tools/bin/python rebuild/tools/inventory_reference_bake.py \
  --output rebuild/build/animator-bake/clip-plan-sword.json
python3 rebuild/tools/reference_animator_bake.py \
  --plan rebuild/build/animator-bake/clip-plan-sword.json --preset torch-sword
```

The optional UnityPy environment now exists under ignored `rebuild/build/unity-tools`; the prior `/tmp/pe-asset-tools` environment was absent. The source-bound plan is ignored build output and can be regenerated. No live original process was touched by preparation.

The root operator completed the following preset on October6. For a repeat, choose a **fresh** capture filename; `female-sword-01.jsonl` is preserved completed evidence and must not be overwritten:

```sh
python3 rebuild/tools/reference_animator_bake.py --launch \
  --plan rebuild/build/animator-bake/clip-plan-sword.json --preset torch-sword \
  --user-data "$HOME/.local/share/planet-explorers-bake-reference" \
  --output "$HOME/.local/share/planet-explorers-bake-reference/pose-captures/female-sword-01.jsonl"
```

Enter a scene with the female main player, then create the exact printed arm file. No Torch purchase/equipment is needed by the isolated clip sampler. Require a complete304-pose recording, its matching launch manifest and cleanup-survival record. The probe never edits the live Animator; it samples separate pure clones. If this filename already exists, choose another fresh name, preserving old recordings.

The successful run was analyzed into `rebuild/build/sword-body-01/all/recording.peas`. These commands describe fresh body/Torch export preparation from that captured input; the Torch outputs below are not claimed installed or graphically verified:

```sh
python3 rebuild/tools/analyze_reference_bake.py \
  "$HOME/.local/share/planet-explorers-bake-reference/pose-captures/female-sword-01.jsonl" \
  --rig rebuild/build/reference-player-starter/player-female-starter-rig.json \
  --report rebuild/build/torch-sword-report.json \
  --export-dir rebuild/build/torch-sword-body
rebuild/build/unity-tools/bin/python rebuild/tools/export_baked_torch.py \
  "$HOME/.local/share/planet-explorers-bake-reference/pose-captures/female-sword-01.jsonl" \
  --rig rebuild/build/reference-player-starter/player-female-starter-rig.json \
  --visual-assets rebuild/build/reference-assets/torch \
  --body-scene rebuild/build/torch-sword-body/all/recording.peas \
  --attachment held --style sword --output rebuild/build/torch-sword-held
```

`--style sword` rejects missing, duplicate, substituted Normal or wrong-ID clips and requires the exact female source group. The attachment exporter still requires byte-matching ordered body clip clocks, source mount ancestry/local transform, hashed shared mesh/textures, and a fresh output directory. It applies no independent hand offset or Torch rotation. A matching stowed group can be exported separately with `--attachment stowed` if a future transition design requires it.

Remaining Torch integration: create and validate matching held/stowed Torch attachments against the selected Sword body clocks, integrate the qualified pose group into the actor host, handle distinct sprint and stationary/running airborne clocks, and compare original/native held-Torch presentation. The staged wood Sword/Shield attachments do not constitute Torch installation. Existing Normal groups remain available for other tools. Backward SwordWB441/SwordRB708 are still uncaptured; NormalWB/RB are not equivalent substitutes. The verified shared third-person turning leaves and the missing directional branches are listed in [the directional mapping gate](STARTER_SWORD_POSE_GROUP.md#directional-mapping-gate). Draw/stow transitions, masked shield composition, the full blend graph and procedural IK remain separate gates.

## October5 preparation verification

The regenerated installed-source inventory accepts the exact female/male clip identities and all ten branch proofs. Python tests cover allowlist synchronization, unchanged default selection and bounds, distinct stationary/running fall loop metadata, wrong branch/point/external reference rejection, and exact complete Sword export prerequisites. All16 sampler tests and4 Torch exporter tests passed. The actual managed probe compiled successfully against the installed Unity5.2 assemblies with the304-sample Sword plan; Python compilation and changed-file whitespace checks also passed. Those were preparation-only results at the October5 checkpoint; they are superseded for capture availability by the completed run below, not by a new Torch graphics result.

## October6 capture and staged integration

The [bounded capture record](../reference/starter-sword-bake-20261006.json) records all304 samples across the ten exact leaves, all42 rig bones, real pose variation and maximum hierarchy reconstruction error1.0572749031823037e−6. Capture SHA256 is `4750b6514742f9c9059e01645187de0b9d2236129573645376c9b2a330f1d4b7`. The root operator observed the120-update cleanup watchdog, a responsive tutorial and ordinary Quit Game/Yes. The exit sidecar records−9/signal9 (shell status137), consistent with the recorded original self-kill behavior; this is not exit0. Root motion, live controller blending and general retargeting remain unqualified by this pose bake.

The subsequent [unified stage](STARTER_SWORD_POSE_GROUP.md) preserves the existing21 body clips and30 defence clips, adds these ten leaves, and supplies matching31-clip modern Wood Sword held/back and Shield forearm attachments plus one40-clip defence actor. Source geometry, existing clip payloads, capsule data and provenance remain intact. `rebuild/build/starter-pair-unified-02` is the current stage; the earlier split-defence experiment is superseded. No installed group was replaced.

Shield ownership/save and combat foundations now exist separately from this pose export; see [ownership and v15](SHIELD_OWNERSHIP.md) and [starter equipment status](STARTER_SWORD_SHIELD.md). Masked left-arm block is still not recovered. The [layer capture audit](SHIELD_LAYER_CAPTURE.md) records source mask/transition contracts, two pre-arm rendering failures, an initial187-frame llvmpipe recording without block, and a subsequent3600-frame user-operated Zink capture with451 steady Sword block frames. That newer recording provides actual mixed base+masked-block comparison evidence. It does not yet supply an authored masked-block asset or native combat/presentation parity claim.
