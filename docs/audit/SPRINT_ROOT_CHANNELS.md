# SprintSwordAttack source root qualification

October 12, 2026. **Source-derived, staged; no observed sprint root playback
or C runtime integration.** This supersedes the mandatory new sprint capture
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
reaction body/attachments/defence and sprint pose. Next is an explicit sprint reader,
outgoing stage routing and shared collision/contact integration. Use the
qualified private display for presentation checks. A new original sprint
root capture is optional corroboration, not needed to extract this bounded
source curve. Live reactions, get-up, stun/knockback and shield gameplay remain
closed; isolated leaf poses do not qualify their blending or physics.
