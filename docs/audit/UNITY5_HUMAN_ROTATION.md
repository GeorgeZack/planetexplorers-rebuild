# Installed Unity 5 humanoid rotation arithmetic

October 6, 2026. Offline binary inspection and isolated arithmetic replay qualify the installed `Avatar.GetZYRoll` helper. **The complete muscle-to-skeleton retargeter is not qualified by this result.** No game process, ELF initialization, Avatar/Animator object, graphics context or live-process injection was used.

## Native entrypoints and axis records

The inspected Linux executable is the same SHA-pinned binary as the [scalar curve audit](UNITY5_CURVE_EVALUATOR.md): `PE_Client.x86_64`, SHA256 `1eb5ad6ff19dddb960a71453603ed7c67403403186c9dfeadc798a6021c4d2a6`, ELF Build ID `651e01fe3d5be6ff003977729e62e91ebd3390e6`. Addresses below are virtual addresses in that exact binary. The relevant text/data file offsets are virtual addresses minus `0x400000`. Stripped objdump labels are not function names.

The internal-call registration loop at `0xc55f00` pairs the literal-name table at `0x1705320` with function pointers at `0x1705260`:

| Registered Avatar API | Wrapper | Native implementation |
| --- | --- | --- |
| GetAxisLength | `0xc55b40` | `0xbaf1e0` |
| GetLimitSign | `0xc55c30` | `0xbaf230` |
| GetPostRotation | `0xc55ba0` | `0xbaf2c0` |
| GetPreRotation | `0xc55b70` | `0xbaf390` |
| INTERNAL_CALL_GetZYRoll | `0xc55c00` | `0xbaf460` |
| INTERNAL_CALL_GetZYPostQ | `0xc55bd0` | `0xbaf770` |

The wrappers resolve the managed Avatar's native object and call the implementations. These object-facing routines were **not executed**. `0xbaf0e0` maps the public bone index into the Avatar human skeleton. Main human bones use indices0–23; two15-entry finger maps cover24–38 and39–53. `HumanTrait.get_MuscleCount` at `0xc559b0` returns92, and `get_BoneCount` at `0xc559c0` returns54. The associated `MuscleFromBone` and `BoneFromMuscle` routines begin at `0xbaf000` and `0xbaf070`.

Following the getters establishes a96-byte native axis record with these offsets. These are native aligned offsets, not an instruction to read serialized asset bytes with this structure:

| Native axis offset | Serialized field / meaning |
| --- | --- |
| `+0` | `m_PreQ`, quaternion XYZW |
| `+16` | `m_PostQ`, quaternion XYZW |
| `+32` | `m_Sgn`, float4; GetLimitSign returns XYZ |
| `+48` | `m_Limit.m_Min`, float4 |
| `+64` | `m_Limit.m_Max`, float4 |
| `+80` | `m_Length`, float |
| `+84` | `m_Type`, integer |

GetPreRotation and GetPostRotation normalize the stored quaternion with Unity's approximate SIMD normalization. GetZYRoll reads the limit and sign vectors, but does **not** apply PreQ/PostQ, axis length, axis type, hierarchy, limb distribution or IK. Its output is a quaternion in the axis frame. Its use of sign-vector XYZ is distinct from `m_Sgn.w`: the fourth SIMD lane is discarded before quaternion construction, so this helper's result does not depend on that fourth sign. Do not invent a fourth-sign multiplication to correct a full-pose discrepancy. Signed local scale must independently be preserved when composing matrices; a quaternion cannot represent a reflection.

## Exact bounded helper arithmetic

For each normalized muscle component `m`, minimum `lo`, maximum `hi`, and sign `sgn`, the native code first computes these branches. Limits are in radians; the ordinary case is negative minimum and positive maximum. Inputs outside±1 are not clamped to±1.

```text
negative = lo < 0 ? (-m)*lo : lo > 0 ? m : 0
positive = hi > 0 ? m*hi    : hi < 0 ? m : 0
angle = m < 0 ? negative : positive
shifted = abs(angle) + float(pi)
wrapped = (shifted - trunc(shifted / float(2*pi))*float(2*pi)) - float(pi)
wrapped ^= signbit(angle)
half = clamp(0.5*wrapped, -1.5707954168319702, 1.5707954168319702)
u = half * 0.15915493667125702
a = 0.25 - abs(u - round_to_nearest_even(u))
v = u - 0.25
b = 0.25 - abs(v - round_to_nearest_even(v))
P(p) = p * (76.03546142578125*p^4 + (-41.28318405151367*p^2 + float(2*pi)))
tangent = P(b) / P(a)
tangent ^= signbit(sgn)
```

Here `^= signbit` means toggling the float sign bit, not integer XOR of the numeric value. The parentheses and individually rounded float operations in [human_rotation_math.c](../../rebuild/tools/human_rotation_math.c) preserve the instruction sequence. This is Unity's rational approximation to the half-angle tangent; substituting ordinary `tanf` changes the recovered result.

For the resulting three components `tx,ty,tz`, the native quaternion before normalization is:

```text
q = (tx, ty + tx*tz, tz - tx*ty, 1)
```

It is normalized twice on this API path. Algebraically this corresponds to YZ swing followed by X twist in quaternion multiplication order `swing * twist`. It is not ordinary XYZ Euler-angle composition.

Normalization uses `RSQRTPS`, multiplies the seed by `0.9999998211860657`, caps it at `FLT_MAX`, then performs the biased Newton refinement with `3.000000238418579` and `0.5`. The square-sum grouping is lane-dependent: adjacent squared components are added before the opposite pair. The experimental C helper preserves this arithmetic, using an SSE reciprocal-square-root seed when available. Its portable `1/sqrtf` fallback is separately labeled and measured; it is not asserted bit-identical.

## Isolated oracle and results

[qualify_unity5_human_rotation.py](../../rebuild/tools/qualify_unity5_human_rotation.py) first requires the exact executable SHA and Linux x86-64 SysV ABI. It copies only two straight-line ranges: `[0xbaf4d6,0xbaf6d1)` and `[0xbaf703,0xbaf734)`. `objdump --insn-width=16` verifies complete instruction coverage and absence of control-transfer instructions. Every RIP-relative constant address is checked against the original bytes and relocated to a private copy. A small new ABI shim supplies local muscle and aligned axis arrays and returns four floats. No Avatar lookup, native object method, allocator or engine registration executes. Memory changes from RW to RX before use. The CLI supervises a child with a45-second limit; the source tool distributes no engine instruction bytes.

The experimental C API is declared in [human_rotation_math.h](../../rebuild/tools/human_rotation_math.h):

```c
bool pe_human_rotation(const float muscles[3], const float minimum[3],
    const float maximum[3], const float sign[3], float out[4]);
```

Its current explicit domain is finite muscles within±16, ordered finite limits within±100 radians, signs exactly±1, and round-to-nearest-even floating-point mode. Invalid input leaves output unchanged. This bounded API is under `tools`, outside runtime CORE/FRONTEND; it does not activate any gameplay or rendering path.

The [recorded oracle result](../reference/unity5-human-rotation-arithmetic.json) compares **272,217 samples**: each of73 axis/sign configurations uses3,729 distinct muscle triples. This includes the complete15-value grid `[-16,-8,-4.1,-2,-1.5,-1,-0.5,0,0.5,1,1.5,2,4.1,8,16]^3`, plus each actual Attack6 muscle minimum/maximum independently applied to every axis. All92 source muscle channels are included; their actual extrema are `-4.0788655281066895` and `2.0431206226348877`. Configurations include32 synthetic cases covering asymmetric/wide/zero/one-sided limits and all eight sign combinations, plus all41 axis records in the recovered female starter Avatar. The native SSE-seed C implementation has **zero bitwise mismatches and zero maximum component error**. The native result itself has maximum norm-squared deviation `3.7268636532417077e-07` because the original normalization is approximate; it is deliberately not replaced by an exact unit-length result. The result binds the C library, helper source/header, rig and actual clip by SHA256.

The same corpus with `PE_HUMAN_PORTABLE_SQRT` has138,434 bitwise mismatches and maximum quaternion-component error `1.7881393432617188e-07`. This is an observed error on the inspected machine/compiler, not a universal portability bound. [Focused C tests](../../rebuild/tests/test_human_rotation_math.c) cover atomic rejection of nonfinite/out-of-range muscles and limits, reversed limits, invalid signs, null pointers and unsupported floating-point rounding modes; they also exercise the accepted16/100 boundaries and aliased input/output. Optimized and ASan/UBSan with leak detection both pass with zero failures. The library was built with `cc -O2 -std=c11 -Wall -Wextra -Werror -fPIC -shared ... -lm`; both libraries and the portable result remain ignored under `rebuild/build/humanoid-proof`.

```sh
TMPDIR="$PWD/rebuild/build/compiler-tmp" cc -O2 -std=c11 -Wall -Wextra -Werror \
  -fPIC -shared rebuild/tools/human_rotation_math.c -lm \
  -o rebuild/build/humanoid-proof/human_rotation_math.so
python3 rebuild/tools/qualify_unity5_human_rotation.py \
  --binary '/home/admin/.local/share/Steam/steamapps/common/Planet Explorers/PE_Client.x86_64' \
  --c-library rebuild/build/humanoid-proof/human_rotation_math.so \
  --rig rebuild/build/reference-player-starter/player-female-starter-rig.json \
  --clip rebuild/build/player-animation-clips/clip-231.json
```

## Remaining full-retarget proof

The public GetZYRoll helper is now independently reproducible. Its only direct call found in the executable's disassembly is the managed internal-call wrapper; this inspection alone does not prove that every animation job invokes this exact helper rather than an equivalent inlined or specialized implementation. GetZYPostQ is a larger routine with axis-type branches, quaternion-relative calculations and `atanf` calls; it was inspected only as a lead and was not copied or executed.

The next proof must compare sampled source muscle channels, actual Avatar PreQ/PostQ, signed local scale and this helper against existing unlayered baked local matrices at matched authored times. Report **all** bones and residuals, including limbs, rather than accepting only the easy bones. Any surviving limb differences require source-backed twist distribution, goal solving, body/root reconstruction or other identified stages. Matching this primitive does not qualify a complete humanoid retargeter, Animator layer masks, controller transitions, foot IK, hand IK, root-motion extraction or shield composition.


The subsequent [697-pose comparison](HUMANOID_ROTATION_PROOF.md) carries out that local-basis test across Normal/Attack/Sword clips and adds a separately traced scalar loop correction. It preserves all12 substantial limb discrepancies and does not qualify full retargeting.
