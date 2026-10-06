# Shield layer capture preparation

October6, 2026. **The original masked shield composition is observed, and three callback-free clone replays now match its actual states, independent clocks, layer weights, clip contributions and parameters exactly. Bone differences remain; an authored masked runtime loop and native block animation are not yet qualified or installed.** The separate ten-leaf Sword locomotion bake is complete. A shield block clip sampled alone is not a substitute for the original masked composition. See [starter pair readiness](STARTER_SWORD_SHIELD.md), [Sword locomotion inventory](TORCH_SWORD_RECOVERY.md), [isolated sampler constraints](ANIMATOR_BAKE.md), and [read-only live pose observer](../../rebuild/tools/REFERENCE_POSE.md).

## Offline reproducible plan

```sh
rebuild/build/unity-tools/bin/python rebuild/tools/inventory_shield_layers.py
rebuild/build/unity-tools/bin/python rebuild/tests/test_shield_layer_inventory.py
```

The first command reads the installed `sharedassets1.assets` and writes the ignored `rebuild/build/shield-layer/plan.json`. It does not launch the game, compile/replace the running sampler, attach a controller, alter a live Animator, or produce runtime pose assets. The plan contains the source asset SHA256, controller779, full serialized layer mask/transitions/states, clip identities/events, state behaviour references, and hashes of the local shield action/equipment scripts. Both `pure_clone_capture_ready` and `runtime_pose_export_ready` remain false.

Eight focused tests pass with the installed asset and local UnityPy. Seven source-contract mutation tests run using ordinary Python; the installed-asset test is explicitly skipped there when UnityPy is absent. Tests reject a full-body mask, missing layer weight, Normal block substituted for Sword block, fixed-second transition substituted for normalized duration, changed MoveStyle thresholds, and wrong ground draw clip. These tests establish source inventory and fail-closed preparation, not runtime layered evaluation.

## Exact simultaneous composition

Installed female controller `FemaleAnimator2`779 has `LArmOnly` at layer3, override blend mode0, default weight1, body-mask words `(0,41947128,1)`, an empty transform mask, no IK pass and no synchronized-layer timing. The mask words are preserved as source data; they are not converted into an invented native bone list.

`LArmOnly.SheildBlock` is a two-leaf one-dimensional `MoveStyle` blend: threshold0 selects `NormalSheildBlock`420, threshold1 selects `SwordSheildBlock`413. The state speed is1; the Sword leaf lasts1.6666717529 authored seconds and loops. Both block clips have no AnimationEvents. Entry from `LArmOnly.DoNothing` requires `SheildBlock=true`; release requires false. Durations are0.09999996423721313 and0.10209385305643082 respectively, both normalized, with no exit-time condition. They must not be treated as fixed seconds.

The base locomotion leaves needed for the first bounded observation are `AttackIdle`90, `SwordWF`133 and `SwordRF`435. The existing Sword inventory verifies their source MoveStyleH/V branch. A useful frame must contain the base and block layer **in the same Animator, at the same captured frame**. Splicing independently sampled bones or replacing the entire body with413 would not establish that result.

Drawing/stowing is separate: `UpbodyOnly.SwordPutOn` and `UpbodyOnly.SwordPutOff` select ground clips270/275 at `InWater=0`, state speed1.2000000476837158. They retain `AnimEvent("PutOnEquipment")` at0.16318383812904358 authored seconds and `AnimEvent("PutOffEquipment")` at0.2606496214866638. Their state-machine behaviour references are also retained. The same clips referenced elsewhere in the controller do not establish this layer or speed. A block capture must distinguish these transient upper-body states from steady locomotion+block.

The source `Action_SheildHold` sets bool `SheildBlock`, action mask `HoldShield`, attribute35 `EnableShieldBlock=1`, and buff30200177; when there is no EquipmentHold it requests Sword movement style. End clears all four and restores the base movement style when appropriate. Admission requires a shield and does not debit stamina. Source action relations prevent concurrent SwordAttack/Sprint block and end block on attack. The input default is held LeftControl. These action effects are separate from pose observation: the generic pose observer reads Animator state, not item identity or damage attributes.

## Smallest safe next capture

Use the existing `reference_pose.py` observer with a **fresh** output in an isolated original profile, as documented in REFERENCE_POSE. The root operator must equip the actual Wood Shield34 through ordinary original-game inventory and obtain the intended sword-held presentation. No probe may grant items, set Animator parameters, invoke actions, play the live controller, or change the original hierarchy. Keep a bounded input log with the ordinary equip/draw actions and weapon/shield inventory evidence.

Capture these phases in one read-only recording, allowing steady state between changes:

1. Sword-held idle without block, then press/hold LeftControl and release it.
2. Sword walk without block, then hold/release LeftControl while continuing movement.
3. Sword run without Sprint, then hold/release LeftControl while continuing movement.
4. Return to idle. If safely practical, record an ordinary attack ending block and a held-key reentry; keep this separate from qualification of the steady three poses.

The observer already records Animator identity/Avatar/controller, every layer's current/next state hashes and normalized clocks, transition flags, clip identities/weights, bool/float parameters, complete human-bone mapping, and evaluated transforms at end of frame. Require female Avatar/controller matches, the actual `LArmOnly` index and weight, `SheildBlock=true` plus the Sword leaf, and simultaneous observed base gait. Record entry/release frames rather than assuming their timing from input wall-clock timestamps. Require a closed capture, sequential clocks, hierarchy validation with `analyze_reference_pose.py`, source hash/launch provenance and subsequent original responsiveness/exit evidence. An observed recording remains a recording affected by live IK/transitions; it does not qualify a repeatable authored runtime loop by itself.

The dedicated analyzer runs the established strict hierarchy/rig analyzer first, then summarizes simultaneous layer evidence:

```sh
python3 rebuild/tools/analyze_shield_pose.py /absolute/path/to/closed-capture.jsonl \
  --report rebuild/build/shield-layer/live-report.json
python3 rebuild/tests/test_shield_pose_analysis.py
```

It records boolean edges, state/clip/weight phases, all source layer spans and exact example frames. A steady gait+block label requires block state439445811, Sword block contribution at least0.9999, other block contributions at most0.0001, layer weight1 and MoveStyle1 within0.0001, true `SheildBlock`, no transition, a matching base gait at the same weight tolerance, and no other active clip-bearing override layer. Mixed source gaits stay mixed. Positive clip weights are retained without rounding away smaller contributors. Seven synthetic tests pass, including simultaneous-Animator identity, missing base, mixed gait, other active override, normal block substitution, closure and current source-hash guards. Invalid, incomplete or changing captures produce no report. Reports always retain `runtime_pose_export_ready=false`.

The legacy observer's launch manifest records paths and launch bounds but no at-launch asset hash. The analyzer preserves that manifest hash and separately verifies the plan against the current installed asset. It states this provenance limitation explicitly. Active `wood_shield`/`wood_sword` transform paths, when present, are visual attachment evidence; they do not establish inventory instance identity, block attribute35 or actual damage mitigation.

## Pure-clone gate

The established `ReferenceAnimatorBake` creates only Transform+Animator clones, attaches **no** RuntimeAnimatorController and samples one clip. It cannot currently reproduce the mask. Installed Unity5.2 API documentation describes `AnimationMixerPlayable`, but no documented masked layer-mixer entry point was found. This is not evidence that an undocumented native method is safe or callable.

Attaching the original controller to an isolated hierarchy changes that isolation contract. It carries140 unique state-machine behaviour objects. Disabling `fireEvents` disables AnimationEvents, not state-machine callbacks; the draw/stow states themselves have callback references. Removing components after evaluation or muting unrelated layer weights is not proof that callbacks cannot run. The live Animator must remain untouched.

A future layered clone therefore needs either a verified callable mask-composition API using the **exact** serialized humanoid mask, or a separately built controller asset with audited callback removal before any Animator evaluation. Both need a fresh bounded probe, preserved source/mask provenance, event suppression, no gameplay components, inactive retained-graph cleanup, and simultaneous comparison against the live recording. The callback-free asset path now has offline preparation and a guarded three-frame smoke implementation described below. The later smoke06 result validates three simultaneous composed evaluations; a general runtime loop remains unqualified, and the tools never launch automatically. Keep block mechanics/source eligibility and masked pose availability separate; do not present an unmasked full-body clip as animation parity.

## October6 original observation attempts

The first two observer launches failed before arming, producing only header and human-trait records. Core inspection confirms the same earlier rendering-failure signature in both: `mov (%rsi),%edi` with `rsi=0xc`, a generated `/memfd:/.glXXXXXX (deleted)` mapping, and five identical NVIDIA glcore caller offsets. Live01 PID2194597 faulted on worker2194661; live02 PID2206672, launched with `-force-gfx-direct`, faulted on its main thread. Thus single-threaded rendering did not prevent this recurrence. The exact driver-versus-game cause remains unresolved; the unknown code address is not evidence of a managed JIT failure. Neither attempt supplies shield pose evidence. The bounded [crash comparison](../reference/shield-observer-crash-20261006.json) preserves registers, mappings, source logs, unchanged-save hashes and temporary-core cleanup.

Live03 used process-only `__GLX_VENDOR_LIBRARY_NAME=mesa`, `LIBGL_ALWAYS_SOFTWARE=1` and `-force-gfx-direct`. The preserved Player.log identifies `Mesa 26.2.2-arch1.1`, vendor Mesa and `llvmpipe (LLVM 22.1.8, 256 bits)`. It completed a closed `capture-limit` record with **187 frames over119.424317 observed real seconds**. The strict hierarchy/rig analysis passed with no missing source rig bones. This establishes a bounded software-rendered recording, not a permanent launch fix, performance parity or ordinary Steam/Proton compatibility.

**Live03 supplied zero shield-pose evidence.** All187 captured frames have `FirstPerson=1` and `SheildBlock=false`. MoveStyle is0 in118 frames and6 in69 frames. The left-arm layer remains `LArmOnly.DoNothing`; the first base pose is overwhelmingly NormalIdle, and the final base pose is overwhelmingly MineHold. There are zero steady Sword block frames, zero simultaneous Sword idle/walk/run+block frames, and no active exact named sword/shield attachment path. The root operator observed that the attempted equipment changes had left the shovel presentation with displayed ATK33/DEF12; this UI observation is separate from the recorded Animator parameters and does not prove an equipped instance ID.

The compact [software recording report](../reference/shield-observer-software-20261006.json) retains hashes, parameter counts, backend log excerpts and the scope limits. Full poses remain in the ignored/local capture, not documentation. All14 checked files in the preserved manual1/save0 and manual2/save1 checkpoints still match the pretest baseline. At handoff PID2213917 remains running and the user owns the desktop; **no exit or teardown success is claimed**. The next observation requires ordinary user/operator confirmation that Wood Sword1 and Wood Shield34 are actually equipped in third person before arming a fresh recording. No runtime shield animation asset is produced from live03.


## User-operated Zink capture: useful mixed block evidence

The subsequent [bounded Zink report](../reference/shield-observer-zink-20261006.json) records a complete `capture-limit` stream of3600 frames across74.367614 observed real seconds, SHA256 `25f1965e54ca2ebcb56479f1b2f72a68762736dc3f2b50c60573b269b340dc17`. The root's stop-file request came after automatic completion. Strict hierarchy/rig validation passed without missing source bones. Player.log confirms `zink Vulkan 1.4(Quadro K1200 (NVIDIA_PROPRIETARY))`, Mesa vendor/version26.2.2, with process-only Zink/Vulkan selection and `-force-gfx-direct`; llvmpipe was not used for this recording. The user reported playable responsiveness and supplied the ordinary equipment, third-person camera, movement, block and attack inputs. Capture cadence averages48.39 samples/s; that is not a controlled performance benchmark or proof of long-term stability.

The capture has732 frames with `SheildBlock=true` and451 **steady Sword block** frames under the analyzer's unchanged tolerance. All451 steady frames are third person. The source shield is observed under animated `Bone L Forearm`, and the source sword under `mountBack` and later `mountMain`. Five steady-block frames also retain another active override; excluding these leaves446 frames whose strongest base-layer contribution is AttackIdle156, SwordRF138, SwordWF80, shared NormalWalkTurnRight28/RunTurnRight25/WalkTurnLeft9/RunTurnLeft6, and SwordWB4. These are observed dominant contributions, not new purity thresholds or recovered backward loop assets.

The strict **0.9999-pure gait+block** count remains zero. This does not mean composed block evidence is absent. Representative frames with a steady shield layer and no other override are:

| Base contribution | Sample | Its weight | Relevant observed parameters |
|---|---:|---:|---|
| AttackIdle |1611|0.999358356|MoveStyle0.999995649; ForwardSpeed≈−4.35e−7; RightSpeed≈−3.31e−7|
| SwordWF |2157|0.9700903|ForwardSpeed2.03722119; RightSpeed0.00327215833|
| SwordRF |1686|0.9877594|ForwardSpeed4.966685; RightSpeed−0.00156909623|

The report retains every positive base/block clip contribution in these examples, including residual Normal and Rifle leaves. The source `Motion_Move_Human.UpdateAnimState` independently damps MoveStyle and MoveStyleH/V with `Lerp(...,5*Time.deltaTime)`. Sword style converges to `(cos20°,sin20°)≈(.9396926208,.3420201433)`, while the serialized style blend point is `(.9399999976,.3420000076)`. The live settled point `(.939692557,.3420201)` agrees with the trigonometric target, not the rounded authored point. Persistent small neighboring-style contributions are therefore consistent with the inspected source graph. Actual forward/side speeds and ground angle additionally create real gait/turn/slope mixtures. **The exact Unity blendType2 weight algorithm has not been reconstructed or numerically validated here.** No values were rounded into a pure state and no classifier tolerance changed.

Attack6→Attack7→Attack8 and three further Attack6 episodes appear on FullBody; the captured bool is false at each observed attack entry. Block returns after the first sequence at sample2828, and again at2890/2971 after subsequent Attack6 action endings. The visible attack tail can coexist with the returning block bool: sample2890 still reports Attack6 at normalized0.697980762, about0.7445 authored seconds, beyond the known Attack6 action-end event0.709125936s. Seventeen frames retain an attack clip and true block bool across these tails; this must not be mislabeled as proof that two gameplay actions are concurrently active. The observer does not record PEAction state or whether Ctrl remained continuously held. It records layer/parameter restoration, not exact input-to-action latency. Fourteen current-clip frames also show upper-body SwordPutOn; this is useful draw observation, not a complete draw/stow transition qualification.

All14 preserved manual1/save0 and manual2/save1 checkpoint files remain unchanged. After the user explicitly returned desktop control, root used SYSTEM→Quit Game→YES; PID2253813 exited with launcher137/rawSIGKILL, matching the known source self-kill pattern rather than exit0. The analysis agent performed no input, capture stop, process control or graphics launch; root supplied this separate lifecycle evidence.

### What the capture unlocks

These frames are now a source-backed comparison target for actual **mixed base + masked block** geometry, including the animated forearm mount and attack-tail restoration. Future qualification should compare the same recorded base contributions, independent base/block clocks and source layer mask; requiring an impossible pure base leaf would discard useful original behavior. The current strict pure-leaf counters remain unchanged diagnostics.

A bounded export can first produce an explicitly finite observed recording/preview with the matching body, attachment and defence matrices. That would preserve evaluated source composition and help validate alignment, but would not become a seamless authored gameplay loop merely by marking it looping. General moving block needs a callback-free exact-mask composition mechanism or another qualified representation of both independent phases, with controlled comparisons against these observations. The pure-clone safety gate above still applies. Full directional coverage, gameplay block eligibility/attributes, source reactions/IK, transition timing and the normal host's visible equipment integration remain separate work. No runtime shield asset was installed by this analysis.


## Callback-free controller composition groundwork

The installed Unity5.2.4f1 managed metadata was inspected with
[`inspect_unity_animation_api.c`](../../rebuild/tools/inspect_unity_animation_api.c),
loading metadata in a separate process and invoking no Unity native methods.
`AnimationMixerPlayable` exists, but the inspected assembly exposes no
`AvatarMask`, `AnimationLayerMixerPlayable`, `HumanPoseHandler`, or callable
masked-layer setter. This rules out using those later Unity APIs directly here;
it does not establish that undocumented native internals are safe.

[`export_shield_controller.py`](../../rebuild/tools/export_shield_controller.py)
now creates a separate local Unity5.2 bundle containing controller779 and its452
unique clip dependencies. It removes all140 referenced behaviour objects and
both behaviour range/index arrays **before any Unity load**, strips372 clip
events, and retains only454 serialized objects: controller, clips, bundle
metadata. External dependencies, scripts and unexpected object references fail
closed. The template is an installed Unity5.2 item container, not the older
Unity4.3 shield bundle. No source asset is modified.

The exporter preserves every clip byte preceding its terminal event vector,
including compressed motion/padding, and preserves the controller graph bytes.
Source blend tables contain NaNs, and `m_ControllerSize` describes native memory
rather than serialized length: neither normal float equality nor slicing by
that size field is a safe round-trip check. The tests use exact float bits and
parse the graph extent. A fresh `--check` rederives the sanitized objects from
the original and checks both decoded content and raw bytes.

The initial smoke deliberately uses `--disable-ik`. Source Base Layer has
`IKPass=true` and Base Move has `IKOnFeet=true`. Their graph bytes at offsets40
and48232 are changed from1 to0; the remaining graph bytes, including the exact
humanoid masks and blend trees, stay unchanged. This is an explicit clone
adapter, not a claim that the resulting pose contains source live IK.
`build/shield-composition/controller-noik-01/manifest.json` retains source,
template, source-graph, modified-graph and output hashes. Its bundle is8,140,097
bytes, SHA256 `4cbfd1e4929c6218b2357225334e08de789e93c24d50abb091aee8e63c138c91`.
The whole original asset hash also binds the removed events; originals remain
untouched. A separate default export can retain both source IK flags.

[`ReferenceShieldComposition.cs`](../../rebuild/tools/ReferenceShieldComposition.cs)
constructs only new Transform/Animator objects, borrows the female Avatar and
reads the source hierarchy, and loads only the hash-bound sanitized controller.
It never assigns or evaluates the original callback-bearing controller.
Events/root motion are off, speed is0, and every component and behaviour is
checked before/after evaluation. Failed native loads cannot be retried in the
same process. The loaded bundle/controller and inactive clones are strongly
retained until process exit: no graph Destroy or bundle Unload occurs.

The separate
[`reference_shield_composition.py`](../../rebuild/tools/reference_shield_composition.py)
launcher and
[`ReferenceShieldCompositionProbe.cs`](../../rebuild/tools/ReferenceShieldCompositionProbe.cs)
prepare **exactly three** recorded examples1611/1686/2157. Full closed-capture
validation runs first. All nontrigger Animator parameters and independent Base
Move/LArmOnly normalized clocks are replayed without rounding source blend
inputs into pure leaves. A fresh female tutorial supplies only the matching
Avatar/hierarchy. The root operator must explicitly launch and arm; no inputs,
items, live parameters or saves are changed by the probe.

Each seek occurs on the new clone at end of frame; readback follows a complete
player-loop frame later. Output records actual states, transitions, normalized
clocks, every layer's clip weights, parameters, and local/model matrices. Bounds
are3 samples,30 seconds after clone preparation,4MiB,512 transforms, and900
seconds waiting for the operator. After disabling the clone and closing output,
a120-update watchdog logs `[shield-composition] Post-cleanup alive120frames`.
The serialized object count is454. The initial smoke incorrectly expected
`LoadAllAssets` to enumerate the controller and all452 dependencies; its guard
rejected the native result before clone creation. The corrected loader resolves
`reference_shield_controller` by its exact container entry, logs actual
`LoadAllAssets` count/types/names and requires that enumeration to match the one
exported controller. It then checks452 distinct `controller.animationClips`
against a compiled source allowlist of names, lengths and frame rates, requiring
no events, nonlegacy clips and finite timing. Duplicate source clip names remain
distinct identities; the plan preserves each source path ID and raw clip SHA256.
The offline454-object closure and whole-bundle hash checks are unchanged.

Ten exporter/mutation tests pass with installed UnityPy/source assets, including
both raw-preserving and two-flag IK-disabled exports. Eleven replay/comparison/clip-allowlist
tests pass, including mixed contributors, independent clocks, unwanted layer
rejection, wrong parameters, actual clock mismatch, residual-weight loss, zero shield-layer weight, unwanted overrides and
bone-position errors. The helper/probe compile against the installed managed
assemblies. These are offline results, **not** a successful runtime load or
qualified moving-block animation. No production assets were installed.

The prepared smoke supports immutable reuse: `--reuse-prepared` verifies the
compiled DLL, shim, generated replay specification, plan, helper sources,
closed capture, rig and bundle hashes without compiling again. Root/operator
must arrange desktop ownership and use the bounded renderer environment from
the reference runtime audit. From the repository root, the prepared command is:

```sh
TMPDIR="$PWD/rebuild/build/compiler-tmp" \
rebuild/build/unity-tools/bin/python rebuild/tools/reference_shield_composition.py \
  --capture /home/admin/.local/share/planet-explorers-sandbox-reference/pose-captures/sword-shield-zink-01.jsonl \
  --bundle-dir rebuild/build/shield-composition/controller-noik-01 \
  --build-dir rebuild/build/shield-composition/smoke-06 \
  --reuse-prepared --single-threaded-rendering \
  --user-data /home/admin/.local/share/planet-explorers-shield-composition-reference \
  --output /home/admin/.local/share/planet-explorers-shield-composition-reference/pose-captures/shield-composition-04.jsonl \
  --launch
```

Without `--launch` the same command only verifies the prepared artifacts. The
launcher prints the fresh `.arm` and `.stop` paths. After a closed `complete`
record with three frames, compare actual readback with:

```sh
python3 rebuild/tools/analyze_shield_composition.py \
  /home/admin/.local/share/planet-explorers-shield-composition-reference/pose-captures/shield-composition-04.jsonl \
  --plan rebuild/build/shield-composition/smoke-06/replay-plan.json \
  --report rebuild/build/shield-composition/smoke-06/comparison.json
```

The comparator uses actual state hashes, normalized-time errors and clip-weight
errors, then reports every mapped human bone's model-space matrix/position
error. Requested clocks alone cannot pass. Live source matrices include IK;
the clone disables it, so disagreement must be diagnosed rather than tolerated
away. Three matching points would establish a useful runtime path, not qualify
independent phase coverage, transitions or a seamless moving-block asset.
Responsive ordinary gameplay after the120-frame watchdog and explicit quit
remain separate operator evidence.


### First runtime load guard: smoke03

Root launched the prepared smoke03 assembly in a fresh isolated female tutorial
(PID2346190, launcher session43500). After arming, the bundle loaded far enough
for the `LoadAllAssets` enumeration guard to reject `Unexpected loaded object
count`; **zero poses were sampled**. This does not establish the exact native
count, since that initial error omitted it. No arbitrary count is inferred from
the error alone. The120-update post-cleanup marker was observed; root then
acknowledged Andy, verified responsive Camera tips, and used Escape→Quit→YES.
Launcher137/rawSIGKILL matches the established original self-quit behavior.
No crash was observed. The frozen capture and `smoke-03/Player.log` remain
unchanged. Smoke04 adds explicit enumeration diagnostics and the named-container
plus exact dependency-allowlist validation described above. Its runtime outcome
is pending; no composed pose or production shield animation is qualified.


### Named-controller load succeeds: smoke04

The second runtime attempt (PID2355734, launcher session47757) records actual
`LoadAllAssets count=1`, the expected named RuntimeAnimatorController,
608 controller clip references, and452 validated distinct event-free clips.
The pure clone reached `cloneReady` with78 transforms and6 layers. Its first
pose serialization then rejected a nonfinite value before writing any frame.
The original error lacked a field name, so it does **not** identify which Unity
metadata field carried the value. No matrix or phase parity is inferred from
this zero-frame attempt. Root observed the120-update cleanup marker and
responsive Camera tips, then ordinary Quit→YES exited137. The bounded
[runtime smoke report](../reference/shield-composition-smoke-20261006.json)
keeps both attempts distinct; prior captures/build outputs remain unchanged.

Smoke05 introduces a separately tested numeric serializer. Active Base Layer0
and block layer3 clocks remain strictly finite. An undefined current-state
clock may serialize as `null` only on a disabled, nontransitioning, clipless
extra layer, with reason `inactive-clipless-layer`. A next-state clock is absent
when not transitioning and serializes as `null` with reason `not-transitioning`.
No zero is fabricated. All active clocks, clip weights/lengths, parameters and
matrix entries retain finite checks; failures now identify their exact layer,
state, parameter or bone/matrix element. The production numeric policy was
compiled and executed in a separate installed-Mono process with15 boundary
assertions and no Unity native calls. Both numeric tests and all11 existing
replay/allowlist/comparison tests pass. Smoke05's graphical result is pending.


### Frozen effective duration identified: smoke05

Smoke05 reached the same validated pure clone and localized the nonfinite value
to `layer[0]Base Layer state=-1436569151.length value=Infinity`. Its current
normalized time had passed finite validation earlier in serialization; this was
not a nonfinite authored clip clock or matrix. No pose frame was written. Root
observed the120-update cleanup marker. The frozen Animator has speed0, for which
Unity reports an infinite effective state duration; substituting a clip length
or zero would conflate different quantities.

Smoke06 therefore permits **only positive Infinity with Animator.speed exactly0**
for effective state-length metadata, recording `null` and the reason
`frozen-effective-duration`. NaN, negative Infinity, and nonzero/nonfinite Animator
speed still fail. It also records finite Animator.speed plus each state's
speed/speedMultiplier. Active normalized clocks, authored clip lengths,
contributions and matrices retain their strict finite checks. The pure numeric
suite executes23 boundary assertions through installed Mono, and12 comparator/
replay tests pass, including malformed duration reasons and nonfinite authored
clip lengths. The successful smoke06 evaluation and remaining geometric differences are recorded below; prior outputs stay frozen.


## Successful masked-controller evaluation: smoke06

The closed `shield-composition-04.jsonl` records all3 requested samples from the
callback-free, IK-disabled clone. Strict comparison reports zero error in both
current state hashes, normalized clocks, layer weights, every aggregated clip
contribution and all replayed parameters. There are no unexpected active
clip-bearing layers. This establishes actual simultaneous masked evaluation
with **independent base/block phases**, including the source's small neighboring
blend contributions; it does not merely assert that the requested times were
accepted.

The [bounded runtime report](../reference/shield-composition-smoke-20261006.json)
and [regional comparison](../reference/shield-composition-regions-20261006.json)
preserve the evidence. Source live capture SHA256 is
`25f1965e54ca2ebcb56479f1b2f72a68762736dc3f2b50c60573b269b340dc17`;
clone capture SHA256 is
`632847800fb07cb94a10291f94611a529bbdd1527896065acece0d3957d4da3c`;
replay plan SHA256 is
`d400ae16aa8303f01a4df5e9c11f7b897998b7d527d96b5c210c7d4e7584d4fa`;
strict comparison SHA256 is
`d42946942752ccfa22606b9bc63bc57c194116ccb51957b78d5ff3868d53d9e8`.
The regional JSON contains numeric errors and hashes, not large pose matrices.

Root observed120 post-cleanup updates, acknowledged Andy and verified responsive
Camera tips, then used Escape→SYSTEM→Quit→YES. PID2374148/session21876 exited137
(rawSIGKILL), consistent with the original's recorded self-quit behavior; no
crash was observed. The [responsive screenshot](../reference/shield-composition-responsive.png)
and frozen `smoke-06/Player.log` are separate lifecycle evidence. Desktop control
was returned to the user. The analysis agent performed no desktop actions.

### Geometry: shield side versus the rest of the body

Values below are maxima across the named region, in model space. The source
matrices include its live postprocessing; the pure clone has no gameplay/IK
components and explicitly disables the two built-in IK flags. Rotation errors
come from orthogonalized matrix bases with matching handedness, preserving the
skeleton's common reflected axes. They are not inferred from translation error.

| Region | Idle sample1611 | Run sample1686 | Walk sample2157 |
|---|---:|---:|---:|
| Shield arm: shoulder, upper/lower arm, hand |0.870mm /0.148°|16.900mm /3.299°|4.657mm /0.985°|
| Shield forearm alone |0.800mm /0.148°|9.718mm /2.148°|2.497mm /0.574°|
| Shield hand alone |0.292mm /0.148°|2.994mm /2.148°|0.997mm /0.574°|
| Legs, feet and toes |47.235mm /6.952°|57.691mm /8.715°|9.117mm /4.150°|
| Neck, head, left eye and jaw |1.050mm /0.128°|28.860mm /3.299°|7.785mm /0.985°|

The shield attaches to the animated left forearm, so its forearm error remains
material even though the hand is within3mm in these three examples. No shield
mesh/collision parity is inferred from the hand alone.

The head and left-hand **local matrices are exactly equal as recorded** in all3
samples. Their model-space discrepancies are inherited from ancestors; they do
not demonstrate a wrong head or hand clip. Spine local rotation differs by
0.110° idle,3.004° run and0.993° walk. Local leg-bend maxima are13.022°,16.964°
and4.118°. Left forearm local rotation differs by0.039°,3.868° and1.262°.
This local/model distinction makes a uniform whole-body correction inappropriate.

Source [`IKCmpt`](../../Assets/Scripts/PeEntity/Cmpt/IKCmpt.cs) binds
FullBodyBipedIK/GrounderFBBIK, controls arm-mapping weights and grounder spine
bend, while [`Motion_Move_Human`](../../Assets/Scripts/PeEntity/Cmpt/Motion/Motion_Move_Human.cs)
updates grounder foot-rotation limits from the ground angle. Those source paths
can affect the regions with larger differences. The clone omits them, and the
source internal foot-IK flag is also disabled there. This makes IK/postprocessing
a concrete candidate, **not proven causation**: no live solver/target snapshot
or paired IK-on/off clone comparison was recorded.

An overall character-proportion mismatch is also unproven. Maximum local
matrix-basis scale differences are below4.9e−7; maximum local translation errors
are0.068mm idle,1.120mm run and0.069mm walk. These measurements do not support
inventing a global scale adjustment. The inspected appearance builder morphs
meshes and preserves bind poses; it does not by itself prove identical Avatar
setup in two sessions. A pre-evaluation hierarchy/bind snapshot remains a useful
control. Large world-space source coordinates and float matrix inversion can
also affect submillimetre comparisons, but no measured error budget establishes
how much of these residuals they explain.

[`analyze_shield_composition_regions.py`](../../rebuild/tools/analyze_shield_composition_regions.py)
reproduces the local/model region measurements after the strict three-frame
readback gate. Its rotation calculation was checked against identity, a90°
rotation, matching reflected bases, mismatched reflection rejection and a
degenerate-basis rejection. No immutable probe, sampler or exporter was modified
for this regional analysis.

### Next bounded qualification

1. Repeat these same3 parameter/phase pairs on a fresh pure clone, in a changed
   order and with a repeated point, to test history/order dependence and preserve
   a pre-evaluation hierarchy snapshot. Keep the existing exact readback gate.
2. Compare a separately hash-bound, callback-free source-IK variant with the
   current two-flag IK-disabled variant. Change only those flags; neither variant
   gains live gameplay components or may touch the original Animator. This can
   distinguish built-in foot-IK effects from the still-unobserved external solver.
3. Once repeatability is established, sample a bounded4×4 Cartesian grid of base
   and block phase for each of the three observed parameter configurations
   (48 combinations), plus repeated endpoints and withheld intermediate points.
   Both axes must vary independently. Record actual clocks/contributions for
   every sample; do not turn a captured one-dimensional phase pairing into a loop.
4. Check loop seams and interpolation against the withheld source evaluations
   before selecting a runtime representation. Export matching body, sword/shield
   attachment and defence poses from the same composed sample. Keep ground IK,
   transient block entry/release, attacks and unobserved movement configurations
   outside that bounded claim until separately recovered.

These are next qualification steps, not completed captures or an authorization
for unattended desktop use. No masked loop was installed and normal native
blocking remains unavailable. The successful controller evaluation provides a
working route toward that export without substituting a full-body block clip or
an invented bone mask.

## Offline repeatability checkpoint

The next stage is prepared but **has not run in the original**. Successful
smoke06 probe/helper/exporter files remain unchanged. New
[`reference_shield_qualification.py`](../../rebuild/tools/reference_shield_qualification.py),
[`ReferenceShieldQualificationProbe.cs`](../../rebuild/tools/ReferenceShieldQualificationProbe.cs)
and [`ShieldQualificationClone.cs`](../../rebuild/tools/ShieldQualificationClone.cs)
prepare two fresh pure clones. The separate clone helper adds a snapshot hook
before any Animator exists, before controller assignment and before Rebind.
It records the actual source and copied hierarchy, relative paths, parents,
local/model matrices and Avatar identity. Source root placement is recorded
separately; the clone root is intentionally identity.

Eight evaluations visit configurations `[idle,run,walk,idle]` on clone0 and
`[walk,idle,run,walk]` on clone1, always using the same three observed parameter
sets and independent phase pairs. Each frame retains the actual readback fields
from smoke06 plus clone/config/repetition IDs. The old clone is disabled before
the second is created; both remain retained until process exit. Bounds are two
clones, eight samples,60 seconds after starting,16MiB,512 transforms and the
same120-update cleanup watchdog. No live controller or gameplay component is
attached to either clone.

The optional source-IK variant is separately hash-bound. Offline verification
checks both sanitized454-object closures, byte-identical clip payloads and
controller data outside the two flags. Source-IK/noIK graph differences must be
exactly offsets40/48232 changing1→0; masks and all remaining graph bytes must
match. Source-IK restores only those built-in flags, not external live IK
components. It requires a separate process/build and has not been evaluated.

`build/shield-qualification/repeatability-01` compiled successfully against the
installed managed assemblies. Its prepared plan and DLL support read-only,
hash-verified reuse:

```sh
TMPDIR="$PWD/rebuild/build/compiler-tmp" \
rebuild/build/unity-tools/bin/python rebuild/tools/reference_shield_qualification.py \
  --build-dir rebuild/build/shield-qualification/repeatability-01 \
  --reuse-prepared
```

This command does **not** launch the original. A future supervised capture would
add `--launch`, an isolated profile/fresh output and the agreed process-only
renderer options after desktop coordination. None was requested or performed
at this checkpoint.

Six focused preparation tests pass: independent phase/order preservation,
Cartesian-grid/seam/withheld layout, exact two-flag pair mutation guards, absent
repeatability rejection, raw-evidence reanalysis plus variant binding, and the
pre-Animator snapshot hook. The separate qualification analyzer has16 focused
tests and accepts all three **actual** smoke06 frames through its strict frame
validator. Its prepared-plan provenance check also passes. These checks establish
tooling readiness, not observed repeatability.

The offline grid description contains48 regular points,27 seam points and12
withheld points (87 total) across the three configurations. No grid capture or
runtime asset exists. Grid launch is gated on reanalyzing a completed
repeatability capture and plan for the same variant/bundle; an edited report
flag is insufficient. Exact repeated numeric local/model poses and active
readbacks are required, without a tolerance override. Interpolation, phase
coverage, source IK causality and production installation remain subsequent
work. The user requested a commit/push checkpoint and source-first audit, so
this turn stops at offline preparation.
