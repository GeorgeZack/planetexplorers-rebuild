# Shared actor alpha and culling

## Problem and source authority

The reported blonde resident is archived entity 20009, Dikeledi Sehgal. The
previous native renderer applied PNG alpha through raylib's default blended
shader to every actor part, while retaining depth writes. Hair drawn before the
head could therefore write depth even where its texture was transparent; the
head behind that fragment was rejected, exposing background. The eye material's
texture alpha was also incorrectly treated as opacity. A private native fixture
reproduces both defects without launching Unity or controlling the user desktop.

The correction is source-derived material selection, not a global culling or
depth workaround. Installed Unity material identities are recovered from each
existing `recording-manifest.json` and the original Steam serialized objects.
For entity 20009:

| Material | Active shader | Active source policy |
| --- | --- | --- |
| `w_hair_04` | `DoubleSideStdEntity` | `_Mode=1`, `_ALPHATEST_ON`, `_Cutoff=0.600000024`, both faces |
| `w_head_04A` | `AvatarStdEntity` | `_Mode=1`, `_ALPHATEST_ON`, `_Cutoff=0.5`, ordinary culling |
| `w_eye_02` | `AvatarStdEntity` | `_Mode=0`, opaque output; PNG alpha is not opacity |

The face also retains a stale `_AlphaCutOff=0.51923078` property. The current
shader uses **`_Cutoff`**, so the stale property must not drive native rendering.

Source files under `Assets/Scripts/WuYiqiu/Resources/Shaders/Avatar Standard`:

- `PEUnityStandardInput.cginc`: `Alpha` multiplies `_MainTex.a` by `_Color.a`.
- `PEUnityStandardCore.cginc` and `DSUnityStandardCore.cginc`: alpha-test
  variants execute `clip(alpha - _Cutoff)`; non-blend/non-premultiplied forward
  output uses `UNITY_OPAQUE_ALPHA`.
- `DoubleSideStdEntity.shader`: the forward pass uses `Cull Off`.
- `AvatarStdEntity.shader`: ordinary face culling remains enabled.

The installed tutorial bundles contain shell shaders. The exporter requires an
enabled attached `ReplaceShaderU3D` component and the exact material reference
on the exact recorded rig part's renderer whose Transform ancestry reaches that component's GameObject
before applying the source replacement table. An unrelated component in the
same asset file cannot authorize replacement. External hierarchy/material
pointers remain unqualified. `Assets/Scripts/ZhouXun/ReplaceU3DShader/ReplaceShaderU3D.cs`
replaces child-renderer material shaders in `Awake`, and
`Assets/Resources/shaderReplaceTbl.xml` maps `DoubleSideStdShell` to
`DoubleSideStdEntity`. The evidence records the original shader, effective
shader, replacement component identity/hash, renderer identity and Transform
ancestry. Tutorial Adisa 9004 is one
qualified cutout example. Merely encountering an unknown shader name does not
authorize alpha-test behavior.

## Runtime contract

`pe_actor_scene_manifest` owns the immutable material policy alongside the exact
actor/PNG snapshot. `pe_actor_scene_view` owns one optional shader per scene;
parts borrow that shader. `pe_actor_view` uses the same material helper for exact
and blended poses, so both hosts, residents and the player use one correction.

- Opaque materials ignore texture alpha for output opacity.
- Cutout materials discard only when `textureAlpha * sourceAlpha < cutoff`;
  passing fragments use opaque material output. Native tint alpha is retained.
- Only source-qualified two-sided parts temporarily disable culling. The
  previous culling enable state is restored after the draw; front/back selection
  is preserved, including the host's reflected projection.
- Depth testing and depth writes remain unchanged. Discarded fragments no longer
  occlude the head behind the hair.
- Shader creation/uniform failures reject the scene; they do not silently fall
  back to the broken transparency behavior.
- Gameplay, mesh topology, skeletons, animation clocks, camera and collision
  ownership are unchanged.

The optional `<scene>.materials` sidecar preserves the existing two-column
`PEAS1` grammar and all `PEAC`/PNG bytes. Existing binaries ignore it. Its format
is `PEAM1`, followed by one row per scene part in identical order:

```text
PEAM1
part.peac part.png cutout 0.600000024 1 1 0123456789abcdef
```

Columns are actor basename, PNG basename, `legacy|opaque|cutout`, cutoff,
source alpha multiplier, two-sided flag, and FNV-1a64 of the exact retained PNG
bytes. The sample hash is illustrative, not an installed asset hash. The
export evidence additionally records SHA-256 identities for sources, original
serialized material/shader bytes, scene/recording manifests and textures.

The parser requires matching row count/order/names/hash, bounded finite policy
values and canonical policy combinations. Missing metadata retains the prior
diffuse adapter; malformed **present** metadata rejects the entire scene.
Policies are snapshotted during preparation and never reread during drawing.
FNV binding catches stale texture metadata; it is not a cryptographic trust
boundary. A source shader/material change requires regenerating metadata even
when PNG bytes do not change.

## Export and installation

`rebuild/tools/export_actor_materials.py` is the owner of metadata recovery.
Run it after replacing an actor group. It resolves recorded source materials,
validates the recorded texture SHA-256 and qualifies active shader mode and
keywords. Modes 2/3, unqualified shaders and synthetic attachment materials
explicitly retain `legacy`; invalid recognized source modes/keyword contracts
fail. Metadata must not be inferred from PNG alpha statistics or hair names.

```sh
rebuild/build/unity-tools/bin/python rebuild/tools/export_actor_materials.py \
  --output rebuild/build/actor-materials-FRESH-NAME --install
rebuild/build/unity-tools/bin/python rebuild/tools/export_actor_materials.py --check
```

`--output` must be fresh. All scenes are qualified before installation begins;
staged sidecars and `material-evidence.json` are written first. Installation
backs up original PEAS manifests and any existing sidecars, then atomically
replaces each sidecar. It never overwrites actor, texture, clip or original Unity
files. Installation is atomic per file, not a multi-file filesystem transaction.
Missing/bad assets abort preflight before any installed sidecar changes.

The October 7 installation qualifies all **55 installed actor scenes**. Final
evidence/staging/backups are under
`rebuild/build/actor-materials-20261007-v3/`; the initial pre-correction PEAS
backups are also retained in `actor-materials-20261007/`. Intermediate
`actor-materials-20261007-v2/` evidence/backups are retained as well. This includes player body/attachments, all
archived residents and tutorial actor groups without changing their clip sets.
The currently running older process does not acquire the shader update until
the rebuilt binary is restarted.

## Verification and limits

Implemented and checked in C on October 7:

- `make -C rebuild actor-material-test`: six source/exporter tests pass in the
  ordinary Python interpreter, without optional UnityPy skips. They cover active
  cutoff selection, opaque gloss, explicit legacy branches, current source guards,
  malformed policy values, exact texture/row binding and exclusion of unrelated
  renderer hierarchies from shader replacement authority. Base material cache
  entries do not cache per-renderer replacement authority.
- `test_actor_scene.c`: optional sidecar snapshot lifetime and malformed
  policy/name/hash/truncation cases pass. The standalone parser run passes
  ASan/UBSan with leak detection enabled and exit 0.
- `make -C rebuild actor-material-render-capture`: passes on owned private Xvfb
  `:115`, NVIDIA Quadro K1200/OpenGL 3.3, exit 0. No user desktop input. The
  synthetic GPU test reproduces legacy transparent-fragment depth holes, then
  asserts opaque alpha, two distinct source cutoffs and culling-state restoration
  from initially enabled and disabled states.
- The same renderer captures source-corrected and legacy portraits of NPC
  20009, the installed player and tutorial 9004 at three angles. Inspected
  before/after images show opaque eyes and correctly cut out hair with underlying
  skin visible. Captures are in `rebuild/build/actor-material-captures/`.
- The source export `--check` completed with all 55 v2 sidecars matching source.
  The final v3 export adds exact renderer ancestry qualification and independently
  recomputes all 55 policies. Every resulting sidecar is byte-identical to v2 and
  to the installed copy, so the existing GPU captures exercise the final policies.
  Original PEAS/PNG SHA-256 values and backed-up PEAS bytes also match. The final
  six pure exporter tests pass after the hierarchy/cache regression additions.

This is a bounded alpha/culling correction. Source PBR lighting, normal/specular
maps, dynamic alpha animators/fades, full transparency sorting and complete
material parity remain unported. Unqualified synthetic weapon attachments, two
legacy quiver materials and actual fade-mode materials retain the old adapter.
The private portraits establish the native defect and fix; they are not a new
original-game visual comparison or an in-session user acceptance check. No
Unity launch was needed to establish this material contract.
