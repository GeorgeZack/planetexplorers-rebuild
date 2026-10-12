# SprintSwordAttack source root qualification

October 12, 2026. **Source-derived extraction; later C integration and native observation are
recorded in [the sprint audit](SANDBOX_SPRINT_KNIFE.md). No original live
sprint root playback is claimed.** This supersedes the mandatory new sprint capture
in the earlier [reaction plan](REACTION_PRESENTATION.md). Claude's completed
25-clip pose bake includes clip401; its root-removed poses do not establish
root displacement.

## Evidence

`export_sprint_root_motion.py` reuses the unchanged
[player root proof](PLAYER_ROOT_CHANNELS.md), including its pinned installed
asset/ELF, static branch evidence and independent sequential running408 oracle.
It then checks the actual female SprintSwordAttack401 against the same bounded
predicate: nonloop/unmirrored humanoid, exact importer flags, no Motion0–6,
complete Root/goal/muscle binding closure, native scalar index table, dense
30Hz clock, unit start scale and finite normalized starting quaternion.

Sprint401 passes that predicate. Its 56 dense frames bracket the authored
1.8000068664550781-second endpoint. Avatar777 supplies human scale
0.9932681322097778. Its own starting quaternion is replayed through the pinned
call-free projection/product/normalization/rotation slices, rather than
borrowing clip408's arithmetic result. The resulting baked basis is identity;
the three diagnostic vectors have zero measured residual in this case.
This arithmetic child does not initialize the engine or Animator.

The source-derived formula is
`(RootT.xz(t) - RootT.xz(0)) * humanScale`, with Y=0 and no raw body-quaternion
rotation. At the exact EndAction event, 1.1924036741256714 seconds, displacement
is approximately (-0.052491, 0, 2.698236) metres. At the clip endpoint it is
(-0.082027, 0, 2.686301) metres. These are unblended source samples, not observed
sprint distances or native collision outcomes. Double interpolation remains
an adapter; transition/layer aggregation, motor and physical acceptance remain
outside the proof. [Bounded result and input hashes](../reference/sprint-root-source.json)
retain the exact case and output digest.

## Staging and checks

From the repository root:

```sh
rebuild/build/unity-tools/bin/python rebuild/tools/export_sprint_root_motion.py \
  --output rebuild/build/sprint-source-motion-01
rebuild/build/unity-tools/bin/python rebuild/tools/export_sprint_root_motion.py \
  --output rebuild/build/sprint-source-motion-01 --check
```

Export requires a fresh directory. The check regenerates and byte-compares both
files. `check-sources` includes this staged group. `sprint-root.pesr` is a
952-byte **one-record PESR1** with raw dense XZ doubles, source clip401 and
Avatar777. Keep it separate from the installed four-record ordinary group and
observed PERM1 running curve. The existing ordinary runtime loader rejects
this one-record profile; no runtime reader or host readiness is added here.
The group is uninstalled and recovered outputs remain ignored build artifacts.

Five synthetic exporter tests cover source preservation, exact profile and
finite knot validation, event/identity/flag mutations, independent arithmetic
case admission and incomplete base evidence. Synthetic fixtures establish
these guards only. Real installed inputs regenerate exactly. Full validation
is recorded in [PLAYTEST](../PLAYTEST.md).

The [separate presentation fixture](REACTION_PREVIEW.md) now renders the staged
reaction body/attachments/defence and sprint pose.

## Reader and host routing

**Implemented in C; supported host admission is qualified separately in
[the sprint audit](SANDBOX_SPRINT_KNIFE.md).**
[`pe_sprint_root_motion`](../../rebuild/src/pe_sprint_root_motion.h) is the
distinct owner of this one-record profile. It admits only the exact Avatar777,
scale, clip401, 56-knot, duration, start and 30Hz header, finite bounded knots
and the exact 952-byte length; the ordinary four-record PESR1 and PERM1 fail.
Sampling is the same double linear interpolation adapter, relative to authored
time 0, with no extrapolation past the clip endpoint. Its world delta accepts
only a valid female third-person sprint stage0 interval and rotates once by
body yaw, as the ordinary owner does.

`sandbox_melee_delta` in `pe_sandbox_view` now sends a retained outgoing sprint
stage0 interval to that owner and fails closed when it is absent; sprint
stages1–3 remain the ordinary Attack7–9 continuations. The host now loads its distinct installed curve and sets Knife readiness only
after complete matching presentation/defence validation. Missing optional
assets still reject the press before stamina debit.

`make -C rebuild sandbox-sword-motion-host-test` reads the staged file
(`PE_SPRINT_ROOT_MOTION` selects another) and checks: sprint stage0 differs
from both408 and ordinary stage0, the routed request equals the owner's delta
through the shared request cap, the unrotated EndAction delta reproduces the
source figures above to 1e-5m, a queued sprint combo routes its retained
stage0 interval and then Attack7 from 0, and a missing owner or a query past
the clip fails without touching the output. A missing staged file is a skip,
not a pass. That target is not part of `check`; before this change it no
longer compiled (missing `pe_sword_motion.h` include), which is fixed here.
The core suite `sprint_root_motion` covers the reader with synthetic bytes.

The complete matching group is now installed with preserved backups; see
[installation and shared collision/contact checks](SANDBOX_SPRINT_KNIFE.md).
A new original sprint root capture is optional corroboration. Live reactions,
get-up, stun/knockback and shield gameplay remain closed.
