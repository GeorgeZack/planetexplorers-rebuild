# Source-first extraction and reference testing

October 6, 2026. Read-only audit against checkpoint `25877e7f` and the existing
local captures. This document proposes a cheaper evidence workflow; it does not
add features, launch Unity, capture the desktop or qualify new assets.

Most remaining rules and content should be recovered from source before another
gameplay session. Unity remains useful as an evaluation oracle for its native
animation machinery and as the final visual reference. Those are different jobs:
an authored pose batch usually needs a loaded Avatar, not a player repeatedly
performing the action or progressing through a quest.

File/line references below identify the audited revision. Source-derived rules,
implemented extraction, observed runtime results and proposed work are separated.

## Choose the cheapest sufficient evidence

| Category | Concrete authority and existing implementation | What still needs observation |
|---|---|---|
| Direct C# logic port | Shield eligibility, action masks, buff lifetime and movement style are explicit in [Action_Sheild.cs](../../Assets/Scripts/PeEntity/Cmpt/Motion/PEAction/Action_Sheild.cs#L21), lines 21–64. Item use/cooldown routing is in [UseItemCmpt.cs](../../Assets/Scripts/PeEntity/Cmpt/UseItemCmpt.cs#L356), lines 356–406. Port bounded state transitions and test transactions before reproducing them by hand. | Integration ordering, unsupported action combinations and final presentation. A screenshot cannot determine exact cooldown, damage or identity ownership. |
| Database, XML, prefab and archive extraction | [export_content.py](../../rebuild/tools/export_content.py#L77), lines 77–95, reads deterministic rows through read-only SQLite. [export_starter_sword_shield.py](../../rebuild/tools/export_starter_sword_shield.py#L73), lines 73–115 and 154–260, ties source action code, prefab fields, buffs and item metadata to provenance. [export_adventure_hunt.py](../../rebuild/tools/export_adventure_hunt.py#L33), lines 33–67, recovers the actual archived random target and return anchor. | A generated target or actor instance absent from both static data and preserved archives may require one bounded observation/archive capture. Static quest text and prices do not. |
| Generic transform animation, offline | The hare pipeline already decodes dense/constant TRS channels, resolves hierarchy bindings and builds skin palettes: [export_reference_actor.py](../../rebuild/tools/export_reference_actor.py#L74), lines 74–118 and 302–344. This is implemented without running Unity. | Streamed curves and unsupported binding semantics need a decoder or a qualified engine fallback; they are not automatically humanoid retargeting. |
| Humanoid retargeting, masks and authored root deltas | Source assets provide muscles, Avatar/controller data and graph metadata, but the current rebuild does not reproduce Unity's complete native evaluation. [ReferenceAnimatorBake.cs](../../rebuild/tools/ReferenceAnimatorBake.cs#L219) evaluates isolated leaf clips; [ReferenceRootMotionBake.cs](../../rebuild/tools/ReferenceRootMotionBake.cs#L116) measures sequential root deltas. The shield path preserves a sanitized original mask/controller. | Actual evaluated poses, independent layer phase composition and root-motion integration need engine qualification with the current toolchain. This is an extraction-time dependency, not a proposed Unity dependency in the C game. |
| Engine evaluation that can be batched | [reference_animator_bake.py](../../rebuild/tools/reference_animator_bake.py#L34), lines 34–80, validates exact clip identities and computes the sample budget before launch. One pure-clone run can cover up to 32 verified leaves and 3,600 samples. [export_directional_player.py](../../rebuild/tools/export_directional_player.py#L65), lines 65–100, reuses that capture for body, held/stowed tools, Torch and defence. | One-time clone/binding/lifecycle qualification, then bounded batch execution. Equipping each item and manually acting out every clip is unnecessary for this existing pipeline. |
| Native renderer, UI and camera integration | UI atlases, fonts, borders and icon mappings are already recovered by [export_training_ui.py](../../rebuild/tools/export_training_ui.py#L33), lines 33–113. Terrain material/UV rules are guarded in [export_terrain_assets.py](../../rebuild/tools/export_terrain_assets.py#L88), lines 88–107. CameraForge has readable graph deserializers and C# nodes. | Final visual comparison, responsiveness, projection/collision behavior and real input ownership still require appropriate native and original checks. Asset extraction alone does not establish visual parity. |

“Direct port” does not mean copying an entire Unity component and its implicit
dependencies into C. Extract its state/data contract, preserve original IDs and
event clocks, and keep one authoritative owner in the native simulation.

## Animation: recover more without more ordinary gameplay

The existing humanoid leaf sampler creates only transforms and an Animator,
borrows the verified Avatar, assigns no source controller, disables events and
root-motion application, and samples a verified clip through a playable. See
[ReferenceAnimatorBake.cs](../../rebuild/tools/ReferenceAnimatorBake.cs#L202),
lines 202–261. Its retained-object cleanup is part of the qualified path, not an
optional optimization. [Torch/Sword recovery](TORCH_SWORD_RECOVERY.md) explicitly
records that buying/equipping a Torch was unnecessary to recover those leaves.

Prepare the union of missing styles, directions, action leaves and auxiliary
mount/defence transforms offline, with exact IDs and total samples. Split that
union into bounded batches only when required. Reuse existing qualified captures;
do not recapture Normal locomotion because a newly supported item needs the same
body clock. The unified starter-pair exporter already appends compatible source
leaves to the shared presentation groups
([export_starter_pair_unified.py](../../rebuild/tools/export_starter_pair_unified.py#L120),
lines 120–198). Separate historical increments reflected expanding requirements;
this is a batching improvement for future work, not evidence that all earlier
captures were redundant.

The generic wolf/rhino path has a different blocker. The
[creature inventory](CREATURE_SAMPLER_PREREQUISITES.md) finds all 23 wolf clips
streamed; rhino idle/attacks are streamed while some movement is dense. It also
finds an Avatar/model-child path-root distinction, unresolved transform hashes,
and class95/customType8 channels outside the hare exporter's TRS contract.
[inventory_creature_sampler.py](../../rebuild/tools/inventory_creature_sampler.py#L50),
lines 50–116, inventories those facts offline. Merely removing the rejection at
[export_reference_actor.py](../../rebuild/tools/export_reference_actor.py#L310)
would silently lose channels.

Two concrete paths remain: implement and test the streamed-curve/binding decoder,
or qualify one isolated generic clone and then batch its exact controller leaves.
First resolve semantics and auxiliary transforms, including the wolf pounce bone
outside its skin palette. Neither path requires repeatedly fighting a live wolf
to obtain every authored attack frame. Live action eligibility and attack-contact
timing are separate source/observation questions.

Root motion cannot be replaced with a sequence of arbitrary static seeks. The
qualified running-Knife curve came from sequential engine evaluation at two
speeds and strict cross-rate agreement. Captures04/05 failed;06 passed without
weakening the 5 mm gate. See [root-motion evidence](RUNNING_KNIFE_ROOT_MOTION.md)
and [analyzer](../../rebuild/tools/analyze_reference_root_motion.py#L126),
lines 126–191. Those retries tested a real numerical uncertainty. Future plans
can calculate sample budgets and prepare multiple explicit density trials before
launch; they must still reject inadequate sampling and preserve raw curves.

## Shield composition: stop searching for an artificial pure pose

The observed mixed base gait is useful evidence. Source Sword style converges to
`(cos20°, sin20°)`, while the authored blend point is approximately `(.94,.342)`.
[Motion_Move_Human.cs](../../Assets/Scripts/PeEntity/Cmpt/Motion/Motion_Move_Human.cs#L1074),
lines 1074–1091, supplies the target and interpolation. The
[Zink capture report](../reference/shield-observer-zink-20261006.json) retains real
small neighboring contributions. Repeating ordinary play until a 0.9999-pure
gait appears would not resolve this source mismatch. The exact native blend
weight algorithm is still unported; no tolerance was relaxed to fabricate purity.

The installed Unity5.2 metadata does not expose the later AvatarMask,
AnimationLayerMixerPlayable or HumanPoseHandler APIs used by many modern examples.
The working route instead sanitizes the original controller's behaviours/events
offline, preserves its mask/clip graph and evaluates a pure clone. The
[shield composition audit](SHIELD_LAYER_CAPTURE.md) records exact source hashes,
guards and limitations. Smoke06 reproduced all three requested states, clocks,
parameters and layer/clip weights exactly, but did **not** establish geometric
parity: maximum model-position differences remained approximately47 mm idle,
58 mm run and9 mm walk. Hand differences were smaller; source IK causality remains
unproven. [Regional evidence](../reference/shield-composition-regions-20261006.json)
distinguishes copied local transforms from inherited ancestor differences.

The prepared next step is two fresh clones, eight reordered/repeated points and
exact repeatability checks, not another manual idle/walk/run recording.
[reference_shield_qualification.py](../../rebuild/tools/reference_shield_qualification.py#L62),
lines 62–96, describes this sequence and an offline87-point phase grid. Grid
execution remains gated on completed, reanalyzed repeatability evidence for the
same bundle. The optional source-IK/noIK pair differs only at the two verified
graph flags; external live IK components are not attached. No repeatability/grid
capture has run at this checkpoint, and no production block loop is installed.

## Camera, rendering and UI: separate math from visual acceptance

CameraForge is available locally, including its binary graph readers:
[ControllerAsset.cs](../../Assets/Camera%20Forge/Scripts/Assets/ControllerAsset.cs#L59),
lines 59–105, and [ModifierAsset.cs](../../Assets/Camera%20Forge/Scripts/Assets/ModifierAsset.cs#L52),
lines 52–93. Decode the selected source controller/modifier graph and inventory its
nodes before tuning offsets from screenshots. Pure numerical helpers can be
checked without launching the game: [reference_smoothdamp.c](../../rebuild/tools/reference_smoothdamp.c#L1)
loads the installed managed method with explicit time steps and no graphics,
game code or input. The current [camera audit](SANDBOX_CAMERA.md) correctly
separates that verified scalar from the remaining graph and native adapters.

Likewise, source texture IDs, atlas borders, UV equations and material constants
are offline facts. Native lighting is still an approximation: the shader in
[pe_terrain_material_view.c](../../rebuild/src/pe_terrain_material_view.c#L15),
lines 15–42, is not the complete original rendering pipeline. Visual tests should
compare the remaining material/lighting behavior after extraction checks pass,
not rediscover which texture or sprite the source references.

UI response order, transaction boundaries and modal ownership can be tested
headlessly. [pe_sandbox_ui_input.c](../../rebuild/src/pe_sandbox_ui_input.c#L28),
lines 28–36 and 204–205, distinguishes whole-world ownership from action-only
consumable ownership. A final desktop check remains valuable for actual keyboard,
pointer, layout and camera interactions; it does not replace those regression
tests or require replaying every quest to check a window.

## What repeated launches actually taught us

| Existing loop | Necessary evidence versus avoidable repetition | Reusable improvement |
|---|---|---|
| Shield observation with the wrong equipped/view state | Software capture03 closed normally but recorded FirstPerson1, no active shield paths and zero `SheildBlock` frames; it could not qualify blocking. [Report](../reference/shield-observer-software-20261006.json). | Verify actual equipment, view and action state before arming the expensive full-pose window. Preserve failure evidence rather than interpreting an attempted input as a successful equip. |
| Shield clone smoke03–06 | First runtime container enumeration was genuinely unknown. Subsequent failures concerned absent/frozen state metadata, not missing gameplay. [Smoke report](../reference/shield-composition-smoke-20261006.json). | Keep a minimal three-frame preflight, contextual field diagnostics and regression fixtures for actual sentinels. Run the complete offline mutation/schema tests before another engine launch. Do not relax active clock or asset-closure guards. |
| Searching for pure Sword gait/block | Residual mixing follows the inspected source target versus serialized blend point; existing mixed frames already provided the three source parameter/phase pairs. | Reuse those frames for exact independent-phase clone evaluation and compare actual contributions. |
| Recovering attachments for each new item | Body, tool mounts and defence share sampled hierarchy clocks. | Export every required attachment/auxiliary palette from one qualified pose batch, preserving exact source mount transforms. |
| Rechecking static quests, item values or UI art by gameplay | Database/XML/prefab and existing archives already answer these questions precisely. | Regenerate/check source tables and use transactional/headless tests. Reserve gameplay for integration, genuinely generated state and visual acceptance. |

A failed graphics driver launch is not new gameplay evidence. Preserve renderer,
launch and source provenance, and use the separately documented stable-enough
process configuration for a planned bounded test; do not infer that a successful
probe establishes long-term renderer stability.

## What a 30-minute normal play session can establish

The full-pose observer currently captures at most **3,600 display frames or120
seconds**, whichever arrives first, with a **256 MiB** output bound. Defaults are
only600 frames/20 seconds. `--wait-seconds` allows up to1,800 seconds **before
arming/capture**, not30 minutes of recorded play. See
[reference_pose.py](../../rebuild/tools/reference_pose.py#L80), lines 80–82, and
[ReferencePoseProbe.cs](../../rebuild/tools/ReferencePoseProbe.cs#L20), lines 20–21,
44–46 and68–79. The byte guard fails closed at lines185–198. A normal
`capture-limit` end can be a valid closed capture; it does not establish that all
requested actions occurred. A byte-limit error is not successful qualification.

At60 sampled frames/s the frame cap is about60 seconds; at30 it is about120.
The actual [Zink recording](../reference/shield-observer-zink-20261006.json)
contains3,600 frames over74.37 real seconds. The earlier software recording
contains187 frames over119.42 seconds. These are observed capture cadences, not
controlled performance benchmarks. Frame/time caps therefore cannot promise
the same useful coverage on different renderer paths.

The probe records model hierarchy, local/world matrices, Animator states,
parameters, contributions and equipment transform paths
([ReferencePoseProbe.cs](../../rebuild/tools/ReferencePoseProbe.cs#L80),
lines 80–104 and149–172). It does not provide a synchronized keyboard/mouse log,
all skill/action ownership, damage transactions, inventory history or external
IK solver internals. Specialized combat/root probes record different fields and
have their own bounds; do not assume this observer answers every gameplay question.

A user's30-minute session can expose bad feel, useful action combinations,
quest order, camera behavior, equipment appearance and missing features. Selected
short armed windows can establish the actual mixed animation/action states;
preserved save checkpoints can expose generated content afterward. It cannot
automatically recover exhaustive assets, all phase combinations, unseen actions,
precise causality or continuous30-minute full-pose data with the current tool.

There is no implemented rolling-window/session recorder. The probe finishes and
destroys its host; `OnDestroy` clears the static instance, so a future explicit
reinstall is technically a separate design option, not an existing automatic
workflow (lines60–66 and204). A lightweight event log plus separately triggered
pose windows would be sensible future tooling, but requires its own bounded
implementation and tests. Do not ask the user to repeat a long session on the
assumption that this capability already exists.

## Prioritized follow-up

1. **Reuse the evidence already collected.** Keep the successful mixed shield
   frames and source graph as the next composition input. Finish the prepared
   repeatability stage before any phase-grid capture or production claim.
2. **Create each capture manifest offline first.** Enumerate missing exact source
   leaves, styles, mounts, auxiliary hitbox bones and expected consumers. Calculate
   sample/time/byte limits; reuse existing captures and batch only the remainder.
3. **Resolve generic creature bindings before gameplay.** Investigate streamed
   curves, root-path semantics and class95 channels offline; choose a tested
   decoder or a single qualified pure-clone fallback, then batch. Keep unresolved
   wolf/rhino channels explicit.
4. **Recover camera/render/UI contracts before visual tuning.** Inventory the
   source camera graph and shader/node math; extract assets once; add numerical
   and transaction tests, then target the remaining visual/input differences.
5. **Make a future long session selective.** Decide the concrete unanswered
   gameplay questions, source preconditions and required observer fields first.
   Separate ordinary user feedback from short high-detail captures. A new rolling
   recorder, if wanted, is a tool task rather than a reason to replay quests now.

This audit changed documentation only. No source/runtime assets, samplers,
generated tables, user saves or desktop state were modified; no new runtime or
visual qualification is claimed.


## Follow-up: partial humanoid computation without engine startup

[The offline rotation proof](HUMANOID_ROTATION_PROOF.md) now reproduces the pinned joint arithmetic and source muscle loop correction, tested against697 previously captured poses. It narrows the remaining computation to body/goal/limb solving and later masked composition; all12 limb discrepancies remain visible in the report. No new original session was required. The prepared engine-bake route remains available if the remaining retargeter proves more costly than a bounded batch.
