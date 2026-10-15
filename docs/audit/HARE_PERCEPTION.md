# Hare sensors and bounded target collection

`pe_hare_perception.[ch]` implements the source sensor tests and update cadence for one explicitly identified player. It produces a new-target collection candidate, not a complete enemy-selection AI. Host integration must still supply real poses, source permissions and lifecycle gates. These results are source-derived and headless-tested; they do not establish native visual/combat parity or a measured original sensor timing trace.

## Sensor membership and cadence

`PEVision.Update` increments its own frame counter, then refreshes its entity list every20 updates. `PEHearing.Update` independently refreshes every16 updates. Their old membership remains cached between refreshes. `TargetCmpt.OnUpdate` increments another counter and performs collection every15 updates. There is no common 60Hz clock or fixed-seconds expiry for these lists. The module exposes separate vision, hearing and collect calls, each representing one enabled component update. The host determines their actual execution order; at coincident cadence boundaries this can affect whether target collection sees the old or new sensor list. A gameplay catch-up tick must not advance these component counters again.

Both sensors require a present entity with a view and use inclusive XZ range:

`horizontal_distance(entity.position, sensor.position) <= sensor.radius + entity.maxRadius`

Vision additionally compares the full3D angle between `entity.centerPos - sensor.position` and the sensor's world-transformed configured axis, with **strict angle < configured angle**. Its ray goes from the sensor to `entity.centerPos`. It then excludes `IsSnake`; hearing has no angle or Snake condition. Sensors themselves do not filter death; target collection does. Hearing skips its ray only when source `isIgnoreBlock` is true. Zero-length vectors follow Unity's zero-angle behavior; a zero-length ray is clear.

The required block callback receives the exact source layer mask: SceneStatic11, VFVoxelTerrain12, Building13, TreeStatic14, Unwalkable16 and NearTreePhysics21. It must query actual terrain/buildings and the relevant live tree blockers. The existing camera/geometry ray excludes trees and is insufficient. The module supplies no invented geometry or dummy always-clear callback. Callback failure leaves sensor state/counter unchanged.

## Collection and permission boundaries

Collection consumes cached vision/hearing membership only on its15-update cadence. It requires the host's source view/controller/scan/search/alive gates, a present living target, carrier eligibility, and absence from the existing enemy list. The helper models a new `CollectEnemies` / `CanSelectEntity` candidate. It does not remove an existing target merely because a new sensor sample loses visibility.

Source `PEUtil.CanAttack` / `CanSelectEntity` use both directional reputation checks, force conflict, and `abs(initialThreat) > Linux PEMath.Epsilon`. Reputation at Neutral4 or higher blocks new attack acquisition. Negative initial threat still passes this collection test and can feed source escape behavior; it must not be turned into unconditional aggression.

`CanDamage` is distinct: force conflict and a nonzero DamageID table value are required, but Neutral reputation allows damage. In Adventure, the inspected repository's `CanDamageReputation` also allows higher reputation; that does not change the stricter new-acquisition rule. `PeHarePermissionInput` makes both directional reputation observations and the actual conflict/threat/damage lookups explicit. It does not infer permissions just from proximity, camps or hardcoded faction IDs. The known initialized hare/player contract (creature player4/camp45 versus player1/camp1, positive initial threat5) can be supplied by the host after validating that evidence.

The output `collection_candidate` is not permission to apply damage or a substitute for `Enemy.canAttacked`, hatred sorting, `CanDelete`, target retention, escape, serious injury, daytime behavior, carrier/vehicle logic, attack cooldowns or skill gates. Existing enemy memory and final `SelectEnemy` remain outside this bounded kernel. No fixed aggro range/timer or special automatic retaliation is introduced.

## Recovered installed sensor mount

`tools/export_hare_perception.py` reads the installed hare bundle and the recovered `hare.peac` / manifest. It exports `generated/pe_hare_perception_source.h` and `.json`, preserving source bundle/actor/manifest/class hashes, source component IDs, exact bone path, skeleton fingerprint and inverse-bind-derived points. It never launches or instruments the original game.

Both source components are enabled on `Bip01 Head`, transform−9012346613887982781, PEAC joint2, path `Bone05/Bip01/Bip01 Pelvis/Bip01 Spine/Bip01 Spine1/Bip01 Neck/Bip01 Head`:

| Component | Source ID | Serialized settings |
|---|---:|---|
| PEVision | 315025741553887200 | radius20, angle60°, local axis(−.5,1,0) |
| PEHearing | 3190093343559429235 | radius15, ignoreBlock=false |

These are installed serialized prefab values, not the database's different18/10.8/75 settings and not yet a live-setting observation. Scripts or buffs could alter them. The live combat probe is separate evidence.

The exported bind-space origin and axis point are evaluated with `pe_actor_pose_joint_point` using the exact currently rendered hare clip/time. The host transforms the origin by its root/yaw/uniform scale and uses the normalized difference between the posed points for the world axis. Radius stays an unscaled world distance. The exporter verifies the matching skeleton/renderer/inverse binds and exact hierarchy path; an unrelated actor or a torso-only player rig is not compatible.

Unity `TransformDirection` ignores scale. The exporter rejects nonuniform/sheared sampled head transforms, and the current recovered samples have maximum conformality error2.38e−7. Thus the point-derived normalized axis agrees with the sampled rotation to that numerical precision. Between samples, the native actor's existing affine palette interpolation is still an approximation to Unity rotation interpolation; this export does not recover Animator transitions, blend trees or procedural head motion.

Reproduce and verify with the optional local UnityPy environment:

```sh
rebuild/build/unity-tools/bin/python rebuild/tools/export_hare_perception.py
make -C rebuild hare-perception-assets-check ASSET_PYTHON="$PWD/rebuild/build/unity-tools/bin/python"
```

## Validation

Six exporter tests pass with UnityPy, including actual installed sensor settings, inverse-bind point reconstruction, conformal sampled matrices, regeneration, wrong joint/path rejection and actor-hash rejection. `--check` passes. Standard Python runs the two dependency-free contract tests and explicitly skips the four UnityPy-dependent checks when that optional dependency is absent.

See [HARE_MOVEMENT.md](HARE_MOVEMENT.md), [HARE_DAMAGE.md](HARE_DAMAGE.md), [TRAINING_ENEMY.md](TRAINING_ENEMY.md), and [PLAYTEST.md](../PLAYTEST.md) for separate movement, contact and physical evidence.

## Tree-inclusive native obstruction query

`pe_tree_collision_ray` now evaluates a true zero-radius finite ray against the source upright capsule's cylinder and exposed hemispheres. It normalizes direction, accepts exact ray tangency and the segment endpoint, and ignores an exit when the origin is strictly inside a tree collider. This mirrors the existing source-collider policy used by NPC picking; it does not reuse a moving capsule with an artificial radius. A surface-origin inward/tangent entry can be at distance0, while an outward ray does not report the opposite side as an entry.

`pe_sandbox_collision_ray_perception` selects the nearest existing real-geometry or live-tree hit in the last committed collision snapshot. The existing geometry/camera ray still excludes trees. Synthetic crop walls and NPC blockers are not added. Real-geometry triangle behavior remains the existing sandbox adapter; the analytic primitive's interior-origin exclusion is specifically established for trees.

Focused collision tests cover cylinder, cap and spherical-degenerate shapes; exact ray tangent versus adjacent miss; normalized directions and finite segment boundaries; cap/cylinder interior origins; nearest tree versus real box/terrain; immutable snapshot updates and tree removal; absence of synthetic crop-wall hits; and actual tree rays suppressing both vision/hearing caches until the tree is removed. Existing capsule movement/sweep tests remain intact.
