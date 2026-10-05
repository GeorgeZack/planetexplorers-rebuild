# Training752→753: build mode and receiver puzzle

The implemented headless boundary accepts a successful build-mode opening, presents the source completion dialogue, evaluates the actual50 receiver rays and advances to mission 756 only after the computed geometric success. It does not substitute a block count or keyboard event for completion. The 15,000-frame graphical construction run passed with exit 0 at mission 756; no original-runtime receiver success has yet been observed.

## Mission and dialogue order

Read-only `Assets/Resources/DataBase/localData.bytes` records establish:

| Mission | Acceptance and objective | Completion |
| --- | --- | --- |
|752 | Get_MissionItem268×9000; TalkOP4074,300244; Plot437→300245; target4105 | Actual `onBuildMode(true)`, then TalkED300246,4075,4076→753 |
|753 | TalkOP300249; Plot438→300247,300248; target4106 | Receiver score positive; TalkED0→756 |
|756 | TalkOP4074,300254,300255; Plot441→300256,300257,4077 | Next unsupported lesson boundary |

The current chain **does not enter754 or755**. Their old save/export callbacks remain in source, but they are not successors of753 in this database. `TrainingTaskManager.InitMission` registers the build-mode callback for752 and calls `EmitlineTask.InitScene` for753. `EmitlineTask.OnInBuildMode` accepts only true and unsubscribes immediately. A close or duplicate open cannot complete twice.

The C bridge requires the current visible dialogue to be acknowledged, then `pe_training_set_build_mode(true)` stages both the ledger build-mode event and tutorial objective. It displays the three752 completion lines before accepting753. Its source NPC TalkOP and plot hints are shown in one deterministic modal sequence; original asynchronous UI ordering is not claimed. After753's three introduction lines, `pe_training_check_receivers` runs the evaluator directly. There is no public completion Boolean. It transitions to756 without an invented completion talk or new item grant. Entry side effects of756 are deliberately not implemented.

## Exact ray and collider data

`EmitlineRayCaster.cs:29` calls `Physics.Raycast(P - right*15, right, 20, 1<<Layer.VFVoxelTerrain)`. Layer is12, mask4096. Only the nearest hit's transform is compared to the two receiver collider transforms. Terrain/building hits and misses both count as successful shielding; room/player colliders on other layers do not participate.

`TrainingManager_New.prefab` contains50 emitters:

- Upright25: P=(10.5+0.5i, 2.5+0.5j, 13.5), right=(0,0,−1), i,j=0..4. Query origin is z28.5.
- Horizontal25: P=(17.5+0.5i, 5.5, 10.5+0.5j), right=(0,−1,0), i,j=0..4. Query origin is y20.5.

The independent collider transforms are enabled, nontrigger BoxColliders on layer12. Upright receiver center=(11.5,3.5,9.5), world size=(3,3,.2). Horizontal center=(18.5,1.5,11.5), size=(3,.2,3). Unobstructed source rays hit the near receiver surface at18.9m. Installed standard BoxCollider fields match the source. All50 emitter and receiver render/collider hierarchy transforms match installed resource root120048 within1e−5; the custom task MonoBehaviour fields have not been separately decoded from the installed build. The durable exporter manifest is `rebuild/build/reference-assets/building/manifest.json`; the broader asset comparison and visuals are documented in [blocks](BLOCKS.md).

`EmitreceiverAppear` scales the separate visual `orgreceiver`; it does not resize the independent `colreceiver` box during appearance. Collision therefore uses the authored slab dimensions, not the fading visual size. The new collision-only mesh exposes these exact two boxes,72 triangle vertices, to the host so player movement is physically blocked as well.

## Success and cleanup

Every ray calls `UpdateLine`: receiver hit subtracts100; anything else adds1. `EmitlineTask.Update` resets the score and `LateUpdate` completes when score>0 and not already complete. With all50 active rays, score=50−101×receiver_hits; thus **all50 must avoid receiver hits**. One remaining receiver produces−51, not partial success. A miss is a source success even though the ordinary intact setup supplies a receiver on every ray.

The C evaluator performs all50 queries in a deterministic pass and validates each nearest-hit result before publishing output. A callback failure, nonfinite/negative/out-of-range distance, unknown receiver identity or inconsistent miss leaves output unchanged. The host supplies nearest layer12 geometry and explicit equal-distance tie ordering because PhysX tie ordering is not established. `pe_training_receiver_boxes` provides the exact receiver slab intersections; terrain and actual Block45 shape hits are merged by the caller. The evaluator never reads a decorative beam endpoint or arbitrary placement count.

The source initialization also writes32 density255/material1 terrain anchors: x17..20,z10..13 at y1, and x10..13,y2..5 at z9. Those anchors are solid backing geometry behind the receiving faces, not replacements for the receiver collider. `TrainingTaskManager` completion cleanup invokes both `CloseMission_buildPoint` and `CloseMission_buildCreateISO`: hide the rays/receiver group, clear every recorded Block45 write, clear authored anchors and end Build action. The C training bridge closes build mode at756; the host owns the matching terrain/cell/visual cleanup. Resource accounting stays in the existing shared gathering ledger and transactional build module, with no refund for lesson cleanup.

## Tests and limits

[pe_training_receiver.c](../../rebuild/src/pe_training_receiver.c) and its tests verify all50 source origins and18.9m slab distances, all-uncovered score−5000,49-covered score−51, all-covered50, actual cube occlusion, behind-receiver failure, source miss behavior, invalid callback rollback, and collision mesh agreement with ray queries/capsule overlap. Training tests cover early/duplicate build opens, dead/modal rejection, ordered752 completion acknowledgement,753 introduction gating, computed completion, repeat rejection, unchanged dirt and a separate dynamic build scene stopping movement.

The evaluator and training boundary pass strict C11, AddressSanitizer, UndefinedBehaviorSanitizer and leak checks. The compiled frontend uses actual source receiver geometry, procedural beam lines and recovered success/failure sprites. It allows source cube placement/removal against real terrain/Block45 picking, merges source slabs with dynamic player collision, and clears anchors/lesson cells on success. Its full 15,000-frame graphical route passed:19 placements, one removal, receiver score 50, zero remaining cells and zero terrain triangles. Dirt 8,987 correctly reflects 9,006 minus 19 one-cell costs, with no one-cell removal or cleanup refund. Wood 11, two shovels (3528/3553 and 3553/3553 durability), nine juices and player HP 500/500 remain conserved. The complete route also recorded eight attack starts, 24 contacts and hare HP 0/212. [Construction frame](../reference/rebuilt-training-building.png) and [completed lesson](../reference/rebuilt-training-building-complete.png); the retained run log is `/tmp/pe-training-building-15000-02.log`. This is C implementation evidence, not an observed original-client construction success. Newly activated source slabs also participate in bounded upward overlap recovery: the controller prefers a walkable support, but accepts the first clear raised position airborne when the rounded capsule only meets a steep slab edge, leaving subsequent gravity/slide to resolve contact. Tests reproduce an upright slab overlap atz9.85 that the terrain backing alone misses, plus blocked-ceiling rollback. This recovery preservesx/z and is an explicitly new safety behavior, not source Unity depenetration parity. Original hologram shader/particle appearance, the complete construction-tool palette, save/exportISO workflows and mission 756 remain unimplemented. Generic Block45 raycasts return an exit when starting inside a cell; receiver hosts must account for Unity's origin-inside-collider exclusion when supporting construction around the distant query origins. Tie ordering is host-defined, not claimed source parity.
