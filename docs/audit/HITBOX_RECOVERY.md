# Recovered hare capsule geometry

The native core can load and pose the installed hare's actual defence and forearm attack capsules, and test instantaneous capsule contacts without graphics. This provides geometry for future combat integration; it does not apply damage, dispatch skill events or complete the combat lesson. The actor preview has an optional capsule overlay. Graphical validation of that new overlay was still pending when this document was written.

## Sources and conversion

The installed source is `AssetBundles/Monster/m_lepus_hare.unity3d`, Unity 5.2.4f1, 1,551,933 bytes, SHA-256 `94409ad1bf9570048a52c42671986907e2a5ca131f55f73737e5b7d7cff76c35`. Recovery uses the normal actor renderer `6629980832213904343` and model root `-8365773678112902782`, with the same 48 skin joints as [actor recovery](ACTOR_RECOVERY.md). It excludes the separate ragdoll hierarchy.

From the repository root:

```sh
/tmp/pe-asset-tools/bin/python rebuild/tools/export_reference_actor.py
/tmp/pe-asset-tools/bin/python rebuild/tools/export_reference_hitboxes.py
```

The isolated environment requires UnityPy 1.25.4. The hitbox exporter accepts `--game-dir`, `--actor-dir` and `--output`. Its defaults use the installed Steam game and ignored `rebuild/build/reference-assets/`. It requires an existing `hare.peac` and `hare-manifest.json`, verifies bundle provenance, skin-joint order and exact inverse-bind data, then writes only `hare.pehb` and `hare-hitboxes.json`. Conversion finishes in a staging directory before replacing those outputs. A source or validation failure leaves previous outputs intact; output inside the original installation is rejected. No Unity editor or graphical process is required.

The JSON manifest preserves source component and transform IDs, raw capsule fields, groups, damage scales, disabled records, unsupported triggers, source-script hashes and reconstruction measurements. The binary is a bounded native conversion artifact, not a replacement Unity bundle.

## Source capsule rules

[PECapsuleTrigger.cs](../../Assets/Scripts/Attack/PECapsuleTrigger.cs) defines six axes: X, Y, Z, negative X, negative Y and negative Z. For source direction `axis`, `ResetInfo` computes the two sphere-center endpoints as:

```text
endpoint A = offset + radius × axis
endpoint B = offset + (heigh − radius) × axis
```

The offset is not the capsule midpoint. Runtime initialization does not clamp `heigh` to twice the radius, so short authored capsules retain reversed endpoint ordering. World endpoints use the referenced transform; world radius is `abs(radius × transform.lossyScale.x)`, regardless of the capsule's axis. Recovery consequently exports a local-X radius vector as well as the endpoints.

The source's immediate contact check is strict: squared distance between the closest segment points must be **less than** the squared sum of radii. Exact tangency does not hit. Contact position divides the line between those closest points according to the two radii. [PEDefenceTrigger.cs](../../Assets/Scripts/Attack/PEDefenceTrigger.cs) initializes a serialized zero damage scale to one; the exporter retains that runtime interpretation.

## Recovered records and known omissions

The file contains 19 records: 15 serialized defence parts and four forearm attack parts. Only 12 defence parts are enabled.

| Defence records, in source order | Joint index / state | Damage scale |
| --- | --- | ---: |
| Head1 | 2 | 2 |
| Body1 | 4 | 1 |
| LUpper, RUpper | 7, 31 | 1 |
| LForearm, RForearm | 13, 35 | 1 |
| LForearm_M, RForearm_M | Null transform; disabled | 1 |
| LThigh, RThigh | 0, 30 | 1 |
| LCalf, RCalf | 10, 32 | 1 |
| Tail1, Tail2 | 8, 18 | 1 |
| Tail3 | Null transform; disabled | 1 |

These disabled parts are present in the source, not missing recovery work. They remain named records with joint `UINT32_MAX` and zero points; posing them fails without changing the caller's output. All recovered defence types and materials are zero (flesh). The defence component ID is `-1768499269866288629`.

The right attack component is `-3642635455585807656`; the left is `7786295801131662817`. Each owns two capsules. The first has radius approximately .15, height .63, offset `(0,0,.08)`, negative-X axis and a **clavicle** transform reference. The second has radius .28, height 1.26, offset `(.47,0,0)`, negative-X axis and the corresponding forearm reference. Recovery follows those references rather than assuming every part uses its owner's forearm. Right joint indices are 22 and 35; left indices are 6 and 13. Their original attack layer mask is 34082816 and attack form is zero.

Group bits are `RIGHT=1`, `LEFT=2`, matching [pe_hare_attack.h](../../rebuild/src/pe_hare_attack.h). Both parts on an arm share its activation group. The independent timing helper preserves the source's inclusive ordinary windows and exclusive EX end boundaries; see [training enemy](TRAINING_ENEMY.md) and the actor event tables.

The separate pounce/body trigger, component `1448081807291899685`, has radius .70, height 2.20, offset `(-.13,0,-.83)`, Z axis and transform `6639384226732744091` (`Bone05/Bip01`). **That transform is not among the 48 skin joints.** Its raw data and omission reason are retained in JSON `unexported_triggers`, but it has no PEHB record. Posing it requires an additional exported transform palette; attaching it to a nearby skin joint would produce unverified geometry. Pounce contact is therefore not implemented.

## Bind-space reconstruction and interpolation

For every enabled record, four bone-local points are retained: both segment endpoints, `(0,0,0)`, and `(radius,0,0)`. Each is converted to mesh bind space using the inverse of that bone's inverse-bind matrix:

```text
bind point = inverse(mesh_inverse_bind[joint]) × bone_local_point
posed point = sampled_skin_palette[joint] × bind point
```

The actor palette already includes inverse bind. Applying it again would be incorrect. The exporter checks the result against direct source hierarchy composition for all four points of every enabled record. The current fixture's maximum reconstruction error is approximately `3.89e-16` meters, below its `1e-5` rejection threshold. This is an authored-hierarchy reconstruction check, not proof of equality at every interpolated animation time.

Native posing uses exactly the actor's neighboring sampled palettes and linear point interpolation. This keeps the recovered capsules and rendered skin on the same sampled timeline, including clip-duration looping and nonlooping clamping. It is not Unity's original quaternion/TRS interpolation. Between samples, bone rotation and radius can therefore differ from the original. Radius is measured between the two posed radius points, then multiplied by the caller's positive uniform actor scale. Root translation and positive-Y yaw are applied to the endpoints separately.

## PEHB version 1 and actor compatibility

All integers and floats are little endian. The header is 32 bytes:

| Field | Type |
| --- | --- |
| Magic `PEHB` | 4 bytes |
| Version, record count, joint count | Three u32; version is 1 |
| Actor compatibility fingerprint | u64 |
| Reserved, reserved | Two u32, both zero |

Each 104-byte record contains a NUL-padded ASCII name[32]; five u32 fields `kind`, `group`, `joint`, `form`, `material`; f32 `damage_scale`; then four f32[3] bind-space points. Kind is 0 for defence or 1 for attack. Defence group is zero; attack group is right 1 or left 2. The current 19-record file is 2,008 bytes.

The loader rejects more than 1,024 records, mismatched actors, duplicate or malformed names, invalid enums/joints, nonfinite or excessive values, degenerate enabled radius vectors, truncation and trailing bytes. Disabled records must have zero points. It exposes no partially loaded object on failure.

`pe_actor_skeleton_hash` computes FNV-1a 64 over PEAC bytes 8 through 19 and bytes 24 through the end of inverse binds: vertex/index/joint counts, all vertex geometry, normals, UVs, skin weights and joint indices, triangle indices, and inverse-bind matrices. It excludes clip count and all clip data, so appending a clip does not invalidate recovered geometry. This deliberately binds to the actual mesh and skin order, rather than accepting an unrelated 48-joint skeleton with similar bind matrices. The current fingerprint is `73c285d0b40445c8`. It is a compatibility check, not cryptographic authentication. PEAC remains version 1.

## Native APIs and preview

[pe_actor.h](../../rebuild/src/pe_actor.h) adds allocation-free `pe_actor_pose_joint_point(actor, clip, seconds, joint, bind_point, output)` and the compatibility hash getter. Joint-point posing supports input/output aliasing, validates inputs and leaves output unchanged on failure. The bind-pose selector treats mesh bind points as identity; it does not reapply inverse binds.

[pe_hitbox.h](../../rebuild/src/pe_hitbox.h) exposes bounded load/free/count/get operations, `pe_hitbox_pose` for a capsule at a specified clip/time/root/yaw/scale, and `pe_hitbox_contact` for an instantaneous capsule pair. Sphere-center endpoints and positive radius define a capsule. Contact math uses double precision internally and handles parallel, skew and zero-length sphere segments. Optional contact output includes closest points, their distance, the source radius-weighted contact position and a geometric normal; coincident closest points yield a zero normal. That normal is not the original motion-derived attack direction.

The standalone actor preview automatically loads an adjacent `hare.pehb` when present and rejects a present invalid file. H toggles wire capsules: green defence, faint gold inactive attacks, orange-red arms within their recovered animation windows. Disabled records are omitted. A smoke run of at least 180 frames cycles all available clips and enables the overlay when present; shorter runs retain idle/run coverage. The duration allocated to each clip determines which active windows a smoke run reaches. This overlay visualizes geometry/timing only and does not execute combat.

## Validation and remaining work

```sh
python3 rebuild/tests/test_reference_hitboxes.py
/tmp/pe-asset-tools/bin/python rebuild/tests/test_reference_hitboxes.py
make -C rebuild test
make -C rebuild sanitize
```

The standard Python run passes five tests and skips the optional installed-source test; the isolated UnityPy run passes all six. Coverage includes all six axes, noncentral offsets, short capsules, local-X radius under nonuniform scale, inverse-bind reconstruction, fingerprint sensitivity and clip-count stability, exact source counts/groups/disabled parts, repeatable binary output and preservation of previous output after source failure.

Native actor and hitbox tests have passed strict compiler checks and AddressSanitizer/UndefinedBehaviorSanitizer, including the actual exported hare. Fixtures cover joint-point skinning, aliasing and atomic invalid inputs, root transforms and scaled radius, strict tangency, parallel/skew/crossing/endpoint/sphere contacts, contact symmetry, every truncated file prefix, malformed headers/names/groups/joints/floats, actor mismatch and trailing bytes. The installed fixture checks 12 enabled defence parts, three disabled parts and four posed attack parts.

The original capsule code also samples motion from previous endpoints to current endpoints and corrects those samples around a parent center. That swept-history behavior is not implemented; fast motions can pass between instantaneous tests. Remaining combat integration must also supply broadphase selection, source collision-layer and faction permissions, authoritative skill-guide timing, per-target deduplication, actual damage processing and death/mission transitions. Neither a capsule overlap nor a highlighted attack window independently authorizes damage. The current training scene still runs only the limited [leisure movement controller](HARE_MOVEMENT.md), with no hostile target supplied.

## Contact resolver integration

The independent [stationary knife contact resolver](TRAINING_COMBAT.md#headless-stationary-knife-contact-resolver) now consumes posed PEHB defence capsules plus a caller-supplied world-space knife capsule. It follows original serialized defence order: successful source capsule results all have `distance=0`, so `PEAttackTrigger.CheckHit` retains the first intersection, not the deepest one. Geometry still supplies no damage permissions by itself. The resolver requires host eligibility, an open `PeCombat` window and an unused guide-target entry before atomically updating HP. Its death result reflects only a real positive-to-zero transition and does not alter tutorials. Swept weapon history and full trigger dispatch remain explicit integration requirements.
