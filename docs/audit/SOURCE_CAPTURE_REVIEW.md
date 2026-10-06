# Source extraction and a thirty-minute reference session

October 6, 2026. Fresh read-only review of the current exporters, generated manifests, recording tools and closed captures. No game, graphics, input, capture or asset installation was performed for this review. This supplements [the earlier extraction audit](SOURCE_FIRST_EXTRACTION.md) and [source-first work order](SOURCE_FIRST_REBUILD.md); historical statements in those documents are not a claim that their proposed increments remain unfinished.

**Thirty minutes of user-operated gameplay can replace many separate broad observation sessions. It cannot replace static extraction, precise engine evaluation or native acceptance tests.** The useful recording is ordinary game-window video with timestamped notes and reusable save checkpoints. The existing full-pose observer is a separate, short diagnostic tool; it cannot record a continuous half hour.

## What we can recover without another play session

| Data or behavior | Existing route and current coverage | Concrete next use |
|---|---|---|
| Items, recipes, translations and prices | [export_content.py](../../rebuild/tools/export_content.py#L115) reads source databases. The current [manifest](../../rebuild/generated/content_manifest.json) selects320 items and95 formulas from1743 valid item rows and589 valid recipe rows; it records52 construction materials and128 terrain-resource rows. Both localization sources contain12990 unique entries. These are extraction counts, not supported-gameplay counts. | Expand a requested feature's dependency closure, component rules and tests. Do not ask the user to open every catalog page or infer values from a screenshot. Bulk exposure of all database rows would advertise unsupported products. |
| Mission text, rewards and realized NPCs | Generated [Adventure dialogue](../../rebuild/generated/pe_adventure_dialogue_text.json) contains27 lines/six missions; [tutorial text](../../rebuild/generated/pe_training_ui_text.json) contains19 missions/24 objectives. [Population](../../rebuild/generated/pe_adventure_population.json) retains25 archived residents; [shops](../../rebuild/generated/pe_town_offers.json) retain284 offers. Dedicated hunt/medicine/defense exporters recover source IDs and scripts. | Export the next quest graph and explicit prerequisites before asking for a quest replay. Recover realized identities and targets from preserved archives first. A missing generated result may still need a new checkpoint. |
| Action, damage, buff and equipment rules | Original C# plus guarded exporters now underpin passive recovery, stamina/comfort, outfit wear, shield admission, capsule history and contact-responsive playback. See [recovery](PASSIVE_RECOVERY.md), [stamina](STAMINA_RECOVERY.md), [contacts](MELEE_CAPSULE_HISTORY.md), [shield action](SHIELD_ACTION.md) and [playback](ATTACK_PLAYBACK.md). | Port transactions, clocks and boundary conditions directly. Use headless differential tests or an offline numerical oracle where practical. A long combat video cannot identify exact frame gates, ownership or failed-transaction rollback. |
| Meshes, materials, UI and colliders | Existing reference-asset/player/terrain/UI/hitbox exporters decode installed bundles and source prefabs. Recovered starter appearances, attachments, UI atlas/text and collision metadata already exist. | Reuse those outputs and source manifests. New item presentation usually needs a matching mount/clock export, not another purchase/equip session merely to discover the mesh. Final appearance still needs comparison. |
| Generic TRS animation | [export_reference_actor.py](../../rebuild/tools/export_reference_actor.py#L74) maps source Transform bindings and dense/constant data; the hare pipeline already exports posed geometry without running Unity. | Reuse the decoder where its binding contract applies. Streamed curves are explicitly rejected at [line310](../../rebuild/tools/export_reference_actor.py#L310); do not remove the guard to produce incomplete motion. |
| Humanoid channels and a substantial rotation primitive | [packed_channels](../../rebuild/tools/export_reference_player.py#L94) already recovers dense/constant data. The [latest proof](HUMANOID_ROTATION_PROOF.md) compares697 old captured poses, implements the pinned GetZYRoll arithmetic and tests serialized muscle-loop correction offline. | Continue source-derived body/goal/limb solving against existing failed matrices. There is no reason to recapture the same Normal/Sword leaves merely to test the next mathematical candidate. |
| Camera and rendering parameters | CameraForge graph readers, node implementations, prefabs, material definitions and UI layouts are local. [The extraction audit](SOURCE_FIRST_EXTRACTION.md#camera-rendering-and-ui-separate-math-from-visual-acceptance) identifies their readers. | Decode the actual graph and port its math before another broad camera survey. A video helps judge the result but cannot recover hidden collision masks or precise damping constants by itself. |

Generated content and reference geometry remain bounded selections, not general procedural Adventure replication. The installed route and residents represent archived state. Worlds, NPC populations, random quest destinations and defense spawns need their original seeds/settings/realized state or a source-derived generator; walking for thirty minutes does not export every world chunk automatically.

## Evidence that should be reused

The strongest current humanoid comparison set consists of the existing closed female-clips-06 (345 poses/ten leaves), female-sword-01 (304/ten) and female-sword-backward-02 (48/two). Their697 poses yield27880 comparisons of40 non-root local bases. The28 non-limb bones match closely after the bounded loop correction, while12 limb bones still disagree substantially; maximum local-basis component error reaches1.35407. Hips/root, goals, limb solving and complete layer composition remain outside that result. These are matrix errors, not meters or a whole-character accuracy score. See [the hash-bound comparison](../reference/humanoid-local-rotation-comparison.json).

Other already useful evidence includes:

- [Directional recovery](DIRECTIONAL_PLAYER.md): body, tool, mount and defence exports can share qualified body clocks; do not perform a separate manual locomotion recording for every held item.
- [Running root06](RUNNING_KNIFE_ROOT_MOTION.md): a qualified sequential root-displacement curve. Static pose seeks or a video cannot replace this curve, but it does not need recapture for each consumer.
- [Shield live Zink capture](../reference/shield-observer-zink-20261006.json): actual mixed gait/block/attack behavior and source states. Searching for a perfectly pure gait is unnecessary when source target parameters themselves produce neighboring contributions.
- [Shield smoke06 and regional comparison](SHIELD_LAYER_CAPTURE.md): three exact graph-readback matches from a sanitized controller, with remaining positional discrepancies. This is a useful composition baseline, not a qualified moving block loop.
- Existing source/reference archives, including later defense checkpoints, should be inspected for missing realized data before replaying earlier progression. Use current checkpoint metadata; historical slot numbers alone are not a safe selection rule.

## Remaining engine work versus one prepared bake

Full humanoid evaluation is not unavailable in principle offline. Its remaining work is an algorithm-recovery and validation project: source root/goal coordinate transforms, human scale, limb solve/twist distribution, then complete local/world poses and masked composition. The current partial rotation match does not make those remaining operations trivial. No defensible completion-time estimate follows from the number of matched bones.

For the immediate moving-shield feature, a bounded engine batch remains an alternative to finishing the general solver. The [leaf launcher](../../rebuild/tools/reference_animator_bake.py#L34) permits at most32 allowlisted source clips and3600 samples, calculates the budget offline, and can reuse one Avatar evaluation for body/attachments/defence. It does **not** recover a two-layer shield pose by treating block as a full-body leaf.

The separate [shield qualification tool](../../rebuild/tools/reference_shield_qualification.py#L90) prepares two fresh pure clones and eight reordered/repeated points, then reanalyzes completed exact-repeatability evidence before permitting a same-bundle grid launch. Its offline [87-point plan](../../rebuild/tools/reference_shield_qualification.py#L74) contains three configurations, a4×4 independent base/block phase grid, seams and withheld points. Repeatability and the production grid have not yet run. A source-IK/noIK pair differs only at two checked graph flags; neither attaches external live player components. One successful future launch could batch prepared work, but it is not honest to promise that one unvalidated bake closes all geometric or interpolation questions.

Practical choice: finish offline preflight, reuse the existing comparator, and make one explicit decision between the remaining solver work and a bounded engine qualification batch. Do not alternate between speculative code and repeated manual idle/walk/run sessions. A new session should answer a named question absent from source and existing captures, or accept a finished native change.

## Actual recording capabilities and limits

[reference_pose.py](../../rebuild/tools/reference_pose.py#L75) and [ReferencePoseProbe.Configure](../../rebuild/tools/ReferencePoseProbe.cs#L40) agree:

| Setting | Default | Hard limit / meaning |
|---|---:|---|
| Pose frames |600|3600 display frames|
| Active recording |20 seconds|120 seconds; closes when either time or frame limit is reached|
| Wait for arming/player |900 seconds|1800 seconds; this is **not recording duration**|
| Output |bounded JSONL|256 MiB; excess is an error, not automatic rotation to another file|

The observer records at end-of-frame, with Animator states/parameters/contributions, hierarchy transforms, renderer bone mappings and timestamps. It does not log synchronized user input, every inventory transaction, all combat/action owners or external IK internals. Its host finishes and is destroyed after a window; an automatic rolling or rearm workflow is **not implemented**. Specialized hunt/root/defense observers have different schemas and limits and are not interchangeable.

The actual Zink shield trace reached3600 frames over74.367614 seconds and used about226.2 MiB. The earlier software-rendered trace reached only187 frames in119.42 seconds. These demonstrate variable observation cadence, not fixed-rate sampling guarantees. Extending the same uncompressed Zink density to thirty minutes would be roughly5.35 GiB, an estimate rather than a supported mode. Raising limits alone would not solve recorder overhead, event attribution or missing fields.

[reference_input.py](../../rebuild/tools/reference_input.py) provides bounded keys, pointer actions and still screenshots; it is not a continuous video recorder. No current repository tool supplies an integrated thirty-minute game-video-plus-event recording session. A proposed user video therefore needs an ordinary recorder configured separately before play, with a checked output path and short playback check. That setup is not performed by this audit. A future lightweight event recorder with optional short pose windows would be new tooling, not a capability to assume today.

## A practical thirty-minute user session

Prepare a **short question list**, not an obligation to replay all known content. The user remains sole input operator. Preserve the starting checkpoint and use a new slot for the end state; do not overwrite manual references. Record game-window video and elapsed-time notes, with no need for microphone or unrelated desktop content. Confirm the current process-local launch setup from [REFERENCE_RUNTIME](../REFERENCE_RUNTIME.md); the playable Zink observation was followed by a separate startup crash, so a crash-free half hour is not established.

| Approximate time | User activity, when naturally available | Evidence retained |
|---|---|---|
|0–3 min|Show game mode/difficulty, character stats, worn/held equipment, consumable counts and current mission.|Baseline needed to interpret damage, animation and progression.|
|3–9 min|Move normally: forward/side/back walk/run, camera modes, draw/stow, transitions into attack/block and release.|Control behavior, visible transitions and useful combinations; mark any jarring moment.|
|9–20 min|Ordinary combat with retreat/recovery and consumables; block when equipped. If practical, save to a new slot and reload after partial progress.|Usability, survival behavior, HP/stamina display and actual partial-progress persistence; no demand to survive or finish.|
|20–27 min|Follow the current reachable quest, report it or use shops/crafting as naturally needed.|Dialogue order, rewards, UI layout, interaction cadence and missing features.|
|27–30 min|Show final equipment wear, counts and mission state; save to a new slot and identify useful timestamps.|Reusable end checkpoint and a compact comparison checklist.|

The segments are optional priorities; death or inaccessible content is useful evidence, not a failed recording. If one exact animation-state question genuinely remains, prepare **one short armed pose window** around that action during the session, choosing its output and bounds in advance. Do not promise repeated windows in the same launch with the current one-shot probe. Most of the session should remain ordinary play, not instructions to pause every few seconds.

Afterward, review the video by questions/timestamps, extract only useful stills or short sequences, inspect the new checkpoint offline, and map findings to source IDs before implementing. Video establishes visible behavior and ordering; it does not establish precise simulation-time constants, hidden entity IDs, complete asset coverage, all independent phase combinations or the cause of a discrepancy. Compare the rebuilt game separately: original footage does not prove native input ownership, collision integration or save correctness.

## Recommended next work

1. Continue source-backed gameplay/progression and exporter closure work while reusing existing captures. The latest recovery, stamina, capsule-history and playback increments deserve focused native acceptance, not another original-game search for their already recovered constants.
2. Keep humanoid solver experiments offline against the697-pose baseline. Retain failed limbs explicitly; do not fit corrective offsets or install partial poses as exact blocking.
3. If moving shield remains the next presentation blocker, finish a single coherent repeatability/grid preflight and decide whether to spend a bounded engine session. Do not request another long manual shield recording by default.
4. Use the proposed thirty-minute video for broad feel, transition and progression coverage. It can substantially reduce separate observation sessions, with short future checks reserved for genuinely missing state or acceptance of changed native behavior.

Validation for this document: read current tool implementations and generated manifest counts, checked existing evidence and local links. No new gameplay or runtime qualification is claimed.
