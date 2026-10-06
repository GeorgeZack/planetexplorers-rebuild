# Installed Unity 5 scalar curve evaluator

Offline binary-derived evidence, October 6, 2026, followed by an isolated arithmetic replay. No game process, graphical context, live-process injection, or engine initialization was launched. This qualifies the scalar evaluator used by `AnimationCurve.Evaluate` in the installed Linux client; it is not a general claim about every Unity version or animation-graph curve representation.

## Provenance and method

Inspected `/home/admin/.local/share/Steam/steamapps/common/Planet Explorers/PE_Client.x86_64` with `objdump -d` and read-only Python byte inspection. SHA256: `1eb5ad6ff19dddb960a71453603ed7c67403403186c9dfeadc798a6021c4d2a6`. ELF Build ID: `651e01fe3d5be6ff003977729e62e91ebd3390e6`. The executable is stripped; the addresses below are virtual addresses, not recovered debug symbols. For the inspected text and read-only-data ranges, file offset equals virtual address minus `0x400000`.

The internal-call registration loop at `0xc4feb0` pairs names at `0x1702980` with function addresses at `0x1702680`. The literal `UnityEngine.AnimationCurve::Evaluate` selects wrapper `0xc4df70`, which follows the managed object's native pointer and jumps to `0x6e3ae0`. This identifies the evaluator independently of neighboring stripped symbol labels printed by objdump.

The source keys and action binding are recorded in [the generated playback metadata](../../rebuild/generated/pe_attack_playback_source.json). Its exporter now references this separate native-evaluator qualification. The arithmetic replay's metadata hash identifies the earlier metadata file before that explanatory wording changed; the keys, slopes and duration used in the replay are unchanged.

## Serialized infinity mode 2 clamps

The scalar-curve deserializer at `0x791420` loads the key array into native offset `0x38`, then passes native offsets `0x58` and `0x5c` to integer reader `0x75f810`, using the literal field names `m_PreInfinity` (`0x169f086`) and `m_PostInfinity` (`0x169f094`). The reader's direct branch at `0x75f8c8` copies four bytes into the supplied field; its endian branch only byte-swaps. There is no public enum conversion on this path. Serialized integer `2` therefore reaches the same native fields inspected by the evaluator.

Public property setters perform a different mapping: `0x6e1cb0` (pre) and `0x6e1cd0` (post) map public `2` to internal `1`, public `4` to internal `0`, and other values to internal `2`. The getters `0x6e1cf0` and `0x6e1d10` map internal `1` to public `2`, internal `0` to public `4`, and the remaining mode to public `8`. Casting serialized `2` directly to the public enum would change the behavior.

At `0x6e3b2f`, a finite requested time greater than or equal to the last key takes the post-infinity branch. At `0x6e3b78`, internal post mode `2` selects `0x6e3cc0`: the cached polynomial becomes constant, with all three nonconstant coefficients zero and its constant equal to the last key's value. Internal mode `1` instead takes the looping `floorf` branch. For times below the first key, internal pre mode `2` similarly selects the first-value constant branch at `0x6e3ce8`.

Consequently the sword response curve at normalized times **`[0.9995110630989075, 1)` evaluates to `0.9999892115592957`**, its final stored value. It does not wrap, extrapolate toward 1, or substitute 1. At elapsed time greater than or equal to the action's stored duration `0.6499999761581421`, `Action_Sword.UpdateAnimSpeed` stops evaluating the response curve and uses the separately calculated base speed. That action-level cutoff remains distinct from curve wrapping.

## Finite-tangent interpolation

Coefficient builder `0x6e2a00` reads 16-byte scalar keys: time at `+0`, value at `+4`, incoming slope at `+8`, outgoing slope at `+12`. For consecutive keys, the emitted arithmetic is the following single-precision sequence; parentheses indicate operation order, and operations are separate SSE arithmetic instructions rather than fused multiply-adds:

```text
h   = max(float(0.0001), t1 - t0)
inv = 1 / (h * h)
m0  = h * outgoing0
m1  = h * incoming1
dv  = value1 - value0
a   = ((((m0 + m1) - dv) - dv) * inv) / h
b   = (((((dv + dv) + dv) - m0) - m0) - m1) * inv
c   = outgoing0
d   = value0
x   = requested_time - t0
result = (((a * x) + b) * x + c) * x + d
```

This is unweighted cubic Hermite interpolation expressed in local time. The constants at `0x1683bdc`, `0x1686060`, and `0x1686050` are respectively float `1`, float `0.0001` (`9.999999747378752e-05`), and positive infinity. The builder also handles a positive-infinite endpoint tangent by making the segment constant; every tangent in the supported sword curve is finite, and every adjacent key gap is greater than the minimum interval. No editor tangent-mode field is read by this evaluator.

The evaluator's cached interval is start-inclusive and end-exclusive (`0x6e3af8`–`0x6e3b0a`). The float Horner sequence is at `0x6e3c20`; first evaluation after building coefficients joins the same sequence at `0x6e3cac`. The call-free segment selector at `0x6e2b10` similarly selects the following segment at an interior key's exact time. No initialized-engine output comparison or cross-platform bitwise-equivalence test is claimed.

## Isolated native arithmetic replay

[qualify_unity5_curve_arithmetic.py](../../rebuild/tools/qualify_unity5_curve_arithmetic.py) reads the SHA-pinned installed executable and copies only three bounded, call-free ranges into private memory: coefficient builder `[0x6e2a00,0x6e2b02)`, segment selector `[0x6e2b10,0x6e2c9b)`, and Horner arithmetic `[0x6e3c24,0x6e3c44)`. It does not dynamically load the ELF or execute its entrypoint, constructors, registration functions, allocation routines, or engine objects. Keys and a 96-byte scratch curve/cache layout are ordinary local buffers.

Four coefficient RIP-relative constant reads are verified and relocated to copies of the original float `1`, `0.0001`, and positive infinity. The other two ranges need no external address relocation. A small ABI shim preserves RBX, supplies the Horner arguments and returns the float result. Memory transitions from read/write to read/execute before any call; it is never mapped simultaneously writable and executable. Execution is restricted to Linux x86-64 SysV and a subprocess with a 15-second supervisor timeout. The source tool distributes no installed engine bytes.

The [recorded arithmetic result](../reference/unity5-curve-arithmetic.json) includes 29 selected golden points, routine-range hashes and a digest of all 4,109 little-endian float input/output pairs: a 4,097-point uniform `[0,1]` grid plus distinct exact keys and adjacent floats. Interior samples execute the copied original selector, coefficient and Horner instructions. Tail dispatch uses the separately qualified static clamp rule and is labeled `static-clamp`; the tool does not claim to have executed the engine's clamp dispatch or whole evaluator.

Comparison against the native C rebuild initially found exactly one mismatch: at key time bits `3dcccccc` (`0.09999999403953552`), selecting the preceding segment produced value bits `3c23d780` instead of the engine arithmetic's stored-key value `3c23d70a`. The C selector was corrected from `t <= next_key.time` to `t < next_key.time`. After this correction, all **4,109 points matched bit for bit**, with maximum absolute error zero. Root repeated the comparison after the metadata qualification wording and clock-lifecycle changes; the committed result now hashes that final metadata, with identical numerical results. The C wrapper was compiled with `cc -O2 -std=c11 -fPIC -shared -fvisibility=hidden -ffunction-sections -fdata-sections -Wl,--gc-sections ... -lm`, exposing only a `compare_curve(float,float*)` wrapper around `pe_attack_playback_curve`. The local wrapper/library remain ignored build artifacts.

```sh
python3 rebuild/tools/qualify_unity5_curve_arithmetic.py \
  --binary '/home/admin/.local/share/Steam/steamapps/common/Planet Explorers/PE_Client.x86_64' \
  --c-library rebuild/build/attack-playback-audit/compare_curve.so
```

Omit `--c-library` to replay the installed arithmetic independently. An incorrect binary was separately checked and rejected by its SHA before executable mapping; Python syntax compilation passed. These results establish the tested finite-key arithmetic on this CPU/ABI, not contact timing, graph playback, general malformed-curve semantics, or another engine build.

## Reproduction and implementation limits

Representative read-only commands, substituting the installed binary path for `BINARY`:

```sh
sha256sum BINARY
objdump -d --start-address=0xc4feb0 --stop-address=0xc4ff30 BINARY
objdump -d --start-address=0xc4df70 --stop-address=0xc4dfa0 BINARY
objdump -d --start-address=0x791420 --stop-address=0x7914b0 BINARY
objdump -d --start-address=0x75f810 --stop-address=0x75f8d4 BINARY
objdump -d --start-address=0x6e1cb0 --stop-address=0x6e1d30 BINARY
objdump -d --start-address=0x6e2a00 --stop-address=0x6e2b10 BINARY
objdump -d --start-address=0x6e3ae0 --stop-address=0x6e3d18 BINARY
```

A bounded native implementation can support these five finite keys and explicit raw-mode-2 clamping without inventing an endpoint policy. Tests should cover every key, interior samples, the exact final key, its successor float, a point before normalized 1, and the separate action-duration cutoff. Preserve float operation order when claiming agreement with this binary; a mathematically equivalent double-precision Hermite basis need not produce identical rounded results. This evidence does not qualify contact notification order, broadphase ownership, simulation scheduling, or the full Sword action lifecycle; those remain separate integration contracts.
