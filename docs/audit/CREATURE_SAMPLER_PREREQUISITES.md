# Generic creature sampler prerequisites

October6,2026. **Offline installed-data validation only.** The [reusable inventory](../../rebuild/tools/inventory_creature_sampler.py) fills gaps in the earlier [species feasibility audit](ADVENTURE_TOWN_DEFENSE.md#installed-species-feasibility-read-only-october6-inspection): exact Avatar/controller references, hierarchy-relative bindings, clip leaves, material groups and the auxiliary wolf pounce transform. It does not duplicate mesh/texture extraction, decode curves, sample poses or install runtime assets. No original game was launched, and the user's native desktop session was untouched.

[The compact report](../reference/creature-sampler-prerequisites.json) is generated from the two hash-pinned installed bundles. Full hierarchy/rest-TRS and controller-tree metadata can be reproduced into an ignored build report; no vertex arrays, streamed curve payloads or captured animation frames are embedded in the compact report. Original bundle hashes are checked before and after inspection. Existing output files are never overwritten; `--check` is read-only.

## New binding findings

| Dependency | Wolf7 | Rhino4 |
|---|---|---|
|Live Animator ID|9221401832128272006|-1210783088705582868|
|Avatar ID/name|5695114379777007197 / `lupus_wolfAvatar`|5281976513122940807 / `coelodonta_rhinoAvatar`|
|Controller ID|6856608753594384806|5289878430810792808|
|Serialized Animator owner transform|-5710197759955247117 (`mlupus_wolf_model`)|705351890664988904 (`mcoelodonta_rhino_model`)|
|Child with exact Avatar path map|5699745300516145509 (`lupus_wolf_model`)|-3143692575000355903 (`coelodonta_rhino_model`)|
|Avatar skeleton nodes / child hierarchy nodes|63 /64|51 /54|
|Serialized human-index count|0|0|
|Root-motion path|`Bone037`|`Bone_coelodonta_rhino`|
|Source clips / controller states|23 /47|37 /47|
|Missing clip transform-path hashes|4|8|
|Live skin joints / material groups|45 /1|43 /4|

Both installed live Animators reference the intended Avatar/controller and retain transform hierarchies. The wolf bundle also contains a ragdoll Animator with no controller; selecting the first Animator or renderer would choose the wrong branch. Both live Animator prefabs have `applyRootMotion=false`; that serialized setting does not establish how source motion code uses Animator deltas.

Avatar path CRCs and parent relationships match all imported model-child transforms. At the serialized Animator owner,62 wolf paths and50 rhino paths fail the direct relative-path lookup because of the additional imported-model child prefix. This is an **offline path-root distinction**, not proof that the original Unity Animator binds incorrectly: engine generic Avatar binding has not been observed by this tool. A future sampler must preserve and qualify the distinction explicitly, rather than moving the Animator or stripping a prefix silently. Empty serialized human-index arrays also do not replace a live `Avatar.isValid`/generic-binding check.

All23 wolf clips use streamed curves. Rhino idle/attacks use streamed curves; some movement clips contain dense curves. The inventory resolves exact controller leaf indices into clip object IDs/names, including repeated clip uses, without guessing from filenames. Each clip's start/end/sample rate and stream/dense counts remain explicit.

Even after using the exact Avatar/model-child root, clip TRS bindings reference additional path hashes absent from that entire hierarchy:

- Wolf:1308757902,3145981584,3779843825,4227423909.
- Rhino:565701865,1035510781,1468317193,1618488525,2057306712,3035435900,3221266501,3950674502.

The inventory deliberately leaves these unresolved. It does not guess bone names, add fake transforms or declare these channels harmless. Each species' clips also include class95/customType8 attributes7–13 on path0, outside the current offline actor exporter's TRS-only contract. Their presence is another reason a generic sampler must let the actual engine evaluate a qualified clone, or a future curve decoder must explicitly support the source binding semantics. Merely bypassing the streamed-curve rejection is insufficient.

The wolf pounce transform1664068957102402253 is exactly `Bone037/Bip01`, parent1228958024332566035, inside the validated hierarchy but outside its45 skin joints. Its movement therefore needs an auxiliary sampled transform palette; substituting a nearby skin joint would move the attack shape incorrectly. The source capsule/skill references remain in the earlier species audit.

Rhino mesh7236384840505367644 has four triangle/material groups, preserving material order03,01,02,04. Their source index counts are5316,7440,3174,72 and first byte offsets0,10632,25512,31860. Wolf mesh-7480331273908971926 has one group/11802 indices. These mappings establish material partition metadata, not a rendered-model validation or permission to flatten rhino into the hare exporter's one-material format.

## Validation and commands

The tool bounds bundles to64MiB, object tables to4096, hierarchy/skeleton/controller nodes to256 and bindings per clip to2048. It checks source hashes, local object types/references, finite nonsingular TRS, parent/child consistency, duplicate/hash-colliding paths, Avatar parent/path order, unique binding-root candidate, valid root-motion index, controller leaf references, skin/bind-pose counts, material/submesh counts and the auxiliary transform's exact membership. Unsupported clip bindings are reported rather than discarded. `runtime_sampler_qualified` remains false in every report.

Ten [offline tests](../../rebuild/tests/test_creature_sampler_inventory.py) pass under the installed UnityPy environment, with no skips. They cover malformed hierarchy/TRS/Avatar data, exact leaf identity, external pointers, unresolved bindings and changed bundle hashes. The installed-data test inventories each species twice and compares complete results, then verifies compact reporting. The ordinary Python interpreter passes eight tests and skips the two UnityPy-dependent tests; those skips are not passes. The root integration registers this suite in `make test`; the ordinary interpreter retains those two explicit optional skips.

```sh
# From the repository root; use the existing optional asset interpreter.
rebuild/build/unity-tools/bin/python rebuild/tests/test_creature_sampler_inventory.py

# Verify the committed compact evidence without changing it.
rebuild/build/unity-tools/bin/python rebuild/tools/inventory_creature_sampler.py \
  --summary --check --output docs/reference/creature-sampler-prerequisites.json

# Optional complete metadata report: choose a genuinely fresh build filename.
rebuild/build/unity-tools/bin/python rebuild/tools/inventory_creature_sampler.py \
  --output rebuild/build/creature-sampler-next-inventory.json
```

`--species wolf` or `--species rhino` narrows an independent fresh report. No new package installation, graphics context, engine process or desktop interaction is required. The full scratch inspection initially lived at `rebuild/build/creature-sampler-prerequisites.json`; only the final compact evidence is governed by the command above.

## Smallest next sampler qualification

1. Prepare a **separate generic-creature sampler**, retaining existing humanoid sampler guards. Select the actual live entity/model by verified prototype, Avatar/controller and transform identity. Observe its current hierarchy/binding root without altering it. Do not assume that the serialized prefab hierarchy equals the final live hierarchy.
2. Construct a pure transform/Animator clone only, copying the complete verified hierarchy and auxiliary transforms. Do not instantiate the live prefab with AI, skills, colliders, rigidbodies or other scripts. Disable events and do not apply sampled root displacement to gameplay. Retain clone/graph lifetimes until callbacks have unwound, following the qualified player sampler's cleanup constraints.
3. Qualify one wolf idle or mouth-attack clip first: record live metadata and isolated sampled clocks/poses, finite skin and auxiliary palettes, nontrivial expected bone movement, Avatar validity and exact clip identity. A static bind pose or unchanged sample is a failed qualification. Resolve how generic engine binding handles the wrapper and absent paths before broad capture; do not introduce invented channels to force a match.
4. Only then capture the bounded species clip set and export independent multipart actor/Defence/attack assets. Root-motion gameplay, rhino collision, pounce movement, perception/skill timing, NPC participation and two-wave cohort capture remain separate gates. Neither this inventory nor a future pose capture enables native9077 by itself.

The [Town Defense observer](ADVENTURE_TOWN_DEFENSE_OBSERVER.md) can capture beacon membership independently of this animation work. It needs no Animator access, so this newly identified binding uncertainty is not a reason to weaken or delay that observer's existing read-only contract.
