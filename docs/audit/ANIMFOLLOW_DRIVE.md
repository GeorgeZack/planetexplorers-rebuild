# Installed female AnimFollow drive and rig contract

October 12, 2026. **Shipped-artifact-derived and implemented in disconnected C
requests.** `pe_animfollow_drive` now supplies numerical force/torque,
per-joint spring/damper, angular-limit and sync admission requests. No host
calls it. It does not provide ragdoll simulation, engine quaternion evaluation,
visible reactions, get-up or a solved physical pose.

## Evidence

The reverse-engineer-anything skill's managed workflow reused complete saved
REA inventory evidence
`ev_13b79477fb6033537478536807957a9437d4a82d8da7d9d87b9ef1ef27ee7243`.
No repeated inventory, target assembly execution or game launch was needed.
[The existing bounded excerpt](../reference/animfollow-lifecycle-il.json)
already retains every AnimFollow_AF method. The exporter binds those raw CIL
bodies and field layout to the installed Assembly-CSharp SHA/MVID, then binds
the complete female component and resources/mainData/sharedassets0 bytes.

| Method token | Recovered contract |
|---|---|
| 0x06000015 Awake | caches reciprocal fixed dt; resizes profiles; maps rigid/nonrigid pairs and joint space; initializes joint drive and disables angular limits |
| 0x06000017 DoAnimFollow | controller first; active drive cadence, errors, PD requests and joint target formula |
| 0x06000018 / 19 SetJointTorque | two-argument and one-argument overloads; index0/null joints skipped |
| 0x0600001a EnableJointLimits | angular X/Y/Z motion1 or2, leaving linear motion unchanged |
| 0x0600001b FixEuler | subtract360 once only when angle>180 |
| 0x0600001c PDControl | vector arithmetic and last-error write order |
| 0x0600001f SyncTransforms | copy world position then rotation only for present body/master pairs; exception wrapper |

`export_animfollow_drive.py` regenerates
[the rig metadata](../../rebuild/generated/pe_animfollow_drive_source.json)
and C constants. Component324352 contains77 paired master/slave transforms,
11 listed bodies and12 entries in each strength profile. Awake truncates each
profile to11, preserving its prefix. Traversal order agrees exactly with the
listed body order. The pelvis has no joint; the other10 bodies have one each.
The remaining66 nonrigid pairs retain their ordered identities. Each body has
one collider; its source geometry, material reference and component hash are
retained. Null materials still require the engine's default material; numeric
tags are retained without inventing tag-name mappings. Joint break infinities
are explicit strings in strict JSON.

| Index | Body | Rigidbody | ConfigurableJoint |
|---|---|---:|---:|
| 0 | Pelvis | 299397 | absent |
| 1 | L Thigh | 299393 | 309783 |
| 2 | L Calf | 299385 | 309780 |
| 3 | R Thigh | 299389 | 309776 |
| 4 | R Calf | 299381 | 309773 |
| 5 | Spine2 | 299377 | 309770 |
| 6 | L UpperArm | 299373 | 309767 |
| 7 | L Forearm | 299359 | 309764 |
| 8 | Head | 299345 | 309761 |
| 9 | R UpperArm | 299366 | 309758 |
| 10 | R Forearm | 299352 | 309755 |

## Numerical requests and ordering

PDControl is `P * (error + (D * (error - lastError)) * reciprocalDt)`, then
stores error as lastError. D is inside the P multiplication; Rigidbody
velocity is not substituted for the error difference. The reciprocal is
cached once in Awake. Installed PForce/DForce are27.200000762939453 and
0.00930000003427267; PTorque/DTorque are0.10100000351667404 and
0.0011599999852478504. The profiles multiply P and the final magnitude caps.
Bodies use their original per-index profiles; no mass factor is inserted.

The caller supplies ToAngleAxis for
`master.worldRotation * inverse(slave.worldRotation)`. Signed angle times
axis supplies torque error, preserving the axis literally. Exactly360 clears
the signal, skips total accumulation and preserves that body's last error.
Torque-enabled bodies issue AddTorque with literal force-mode2. Position
error is master minus slave. Weighted **vectors**, before clamping, accumulate
into totalForceError even when force is disabled. Opposing errors can cancel.
Enabled force updates history, clamps by its profile and issues
AddForceAtPosition at the slave transform's world position with mode2.
The mode integers are retained; no physical engine consumption is supplied.

As in [the lifecycle](ANIMFOLLOW_LIFECYCLE.md), DoRagdollControl reads the
previous drive's total error before the active check. An inactive/missing-
assignment call preserves the drive's counter, errors and history. Active
calls reset totals, issue body requests and increment the signed frame counter.
Native finite/overflow guards fail atomically; signed wrap is explicit.

A secondary tick refreshes nonrigid indices2 through64 only when
mimicNonRigids is true, and sets joint torque/damping. Every even call writes
those nonrigid rotations **regardless of the mimic flag**. When secondaryUpdate
is greater than2 it requests Quaternion.Lerp with factor2/secondaryUpdate;
otherwise it requests the stored target directly. The C owner emits cadence
requests; it does not maintain or solve those quaternion arrays.

Both joint-torque overloads reuse one cached JointDrive struct. The two-
argument overload replaces spring and damper per profile. The one-argument
overload replaces spring only: it preserves the **shared cached damper**, not
each joint's old damper. Awake's last source SlerpDrive has damper0 and a finite
maximum force near FLT_MAX; mode is changed to1. Angular limits are1 when
enabled,2 when disabled. Sync admission is the present body/master intersection,
including root0, with position before rotation.

Joint target math remains an explicit caller-owned recipe:
`inverse(localToJointSpace) * inverse(master.localRotation) * startLocalRotation`,
where `localToJointSpace = LookRotation(Cross(axis,secondaryAxis),secondaryAxis)`
and `startLocalRotation = slave.localRotation * localToJointSpace`.
No identity quaternion or pose fit substitutes for these engine evaluations.

## Verification and remaining boundary

The core suite checks actual installed identities/profiles, vector cap direction,
per-body history,360-degree preservation, disabled-channel behavior, vector
error cancellation, shared damper, null/root admission, independent nonrigid
cadence, signed wrap and atomic invalid/overflow failure. Its composition with
`pe_animfollow` confirms that the previous **unclamped** error can request a fall
even when the force request is small.

A bounded Python replay interprets only PDControl's pinned decoded instructions
with independently modelled float32 Vector3 operators.259 input vectors/777
components agree bit for bit with the C force requests, without clamping.
This comparison qualifies those adapters; it is not a Mono/JIT or engine
operator oracle. Native ClampMagnitude uses a double norm to avoid intermediate
overflow, which is explicitly an adapter. Installed exporter tests verify
11 bodies/10 joints/66 nonrigids, malformed/reordered rejection and regeneration.
Ordinary Python skips the three UnityPy-dependent cases; `check-sources` runs
all five using the available qualified environment.

```sh
python3 rebuild/tools/run_tests.py -k core:animfollow_drive
make -C rebuild animfollow-drive-source-check
```

Full regression, sanitizer and content/source results are recorded in
[PLAYTEST](../PLAYTEST.md). No graphics check is warranted for an owner with no
host consumer. Physical bodies, quaternion/target evaluation, renderer handoff,
friction/default material and actual Animator/PhysX cadence remain required
before integration. FinalIK remains separate. Live reactions/stun/knockback,
get-up, shield, pounce/aggression and Town Defense gates stay closed.
