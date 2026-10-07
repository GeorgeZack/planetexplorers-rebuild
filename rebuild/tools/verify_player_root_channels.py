#!/usr/bin/env python3
"""Offline root-channel evidence against the preserved running408 engine oracle.

Does not start Unity, create a motion asset, or claim observed ordinary playback.
Qualifies bounded source extraction through static branches, call-free arithmetic
replay in a limited child, and the preserved independent running engine oracle.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import struct
import subprocess
import sys

from export_reference_player import packed_channels
from analyze_reference_root_motion import analyze

ROOT = Path(__file__).resolve().parents[2]
ASSET_SHA = 'd9c65f820a2186116d9eb73d815fe07d9f46924ddfd103ee9510bf254f9a3d67'
ELF_SHA = '1eb5ad6ff19dddb960a71453603ed7c67403403186c9dfeadc798a6021c4d2a6'
CAPTURE_SHA = 'b6ccd0199470a14b10182943d18db566409a03d27029a65bea657a06e63a43b4'
CLIPS = {231: 'Attack6', 240: 'Attack7', 247: 'Attack8', 254: 'Attack9', 408: 'RunSwordAttack'}
FLAGS = dict(m_LoopTime=False, m_LoopBlend=False, m_LoopBlendOrientation=True,
             m_LoopBlendPositionY=True, m_LoopBlendPositionXZ=False,
             m_KeepOriginalOrientation=True, m_KeepOriginalPositionY=True,
             m_KeepOriginalPositionXZ=False, m_HeightFromFeet=False, m_Mirror=False)
# Named serializer, humanoid root orientation/Y/XZ masks, humanScale getter,
# and the human-scale multiplication before final transform composition.
RANGES = [(0xb998c6, 0xb998da), (0xb99b53, 0xb99be5), (0xc1ffd0, 0xc200da),
          (0xc227e0, 0xc228bd), (0xc237f3, 0xc23801),
          (0xc25678, 0xc256a4), (0xc27310, 0xc27387),
          (0xc25378, 0xc254c8), (0xc24f1d, 0xc25064),
          (0xc27260, 0xc272f5), (0xc24c18, 0xc24d2d),
          (0xc24b3b, 0xc24c14), (0xc1f830, 0xc1f843),
          (0xbc6e05, 0xbc7000), (0xbc7c70, 0xbc7c8c),
          (0xc51ec0, 0xc51ef5), (0xbc4920, 0xbc4939),
          (0xc251d3, 0xc252cd),
          (0xc54680, 0xc546af), (0xc51e60, 0xc51e86),
          (0xbc96f0, 0xbc9721), (0xbae650, 0xbae68d),
          (0xc04470, 0xc044dd)]


def digest(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def finite(value):
    return type(value) in (int, float) and math.isfinite(value)


def algebraic_basis_delta(start_yaw, previous, current, common_offset):
    """Exact-real algebra model of the inspected branches, not engine arithmetic.

    KeepOriginalOrientation: adjustment = identity * inverse(start_yaw).
    LoopBlendOrientation: every root Q = adjustment * start_yaw = identity.
    Common root offsets cancel; PositionY is baked. Actor yaw is downstream.
    """
    if (len(start_yaw) != 4 or len(previous) != 3 or len(current) != 3 or
            len(common_offset) != 3 or any(not finite(v) for v in
            list(start_yaw)+list(previous)+list(current)+list(common_offset)) or
            abs(sum(v*v for v in start_yaw)-1) > 1e-6 or
            start_yaw[0] != 0 or start_yaw[2] != 0):
        raise ValueError('Expected normalized planar quaternion and finite float3 values')
    # Explicit cancellation is intentional: avoids large-offset cancellation error
    # in this real-algebra illustration. Native floating-point replay is separate.
    return [current[0]-previous[0], 0, current[2]-previous[2]]


class NativeRootArithmetic:
    """Four reviewed call-free arithmetic slices; no ELF loader or engine state.

    Used only by a CPU/time-bounded child process. Every copied instruction and
    RIP constant is checked against the pinned ELF before a private RX mapping.
    """
    SLICES = {'project': (0xc1dc20, 0xc1dd5b, b'', 2),
              'product': (0xc1a7a0, 0xc1a7fb, b'', 3),
              'rotate': (0xc1a740, 0xc1a798, b'', 3),
              # The slice inherits XMM2=source normalization coefficient3.
              'normalize': (0xc1dd1a, 0xc1dd5b,
                            bytes.fromhex('0f 28 06 0f 28 15')+struct.pack('<i', 2048-10), 2)}

    def __init__(self, binary):
        import ctypes as C
        import mmap
        import platform
        import re
        if sys.platform != 'linux' or platform.machine() != 'x86_64' or C.sizeof(C.c_void_p) != 8:
            raise ValueError('Arithmetic replay requires Linux x86-64 SysV')
        libc = C.CDLL(None, use_errno=True)
        libc.fegetround.restype = C.c_int
        if libc.fegetround() != 0:
            raise ValueError('Arithmetic replay requires round-to-nearest')
        raw = Path(binary).read_bytes()
        if hashlib.sha256(raw).hexdigest() != ELF_SHA:
            raise ValueError('Binary SHA mismatch: no executable mapping created')
        self.functions, self.mappings, self.provenance = {}, [], []
        for name, (start, end, prefix, argc) in self.SLICES.items():
            code = bytearray(mmap.PAGESIZE)
            body = raw[start-0x400000:end-0x400000]
            code[:len(prefix)] = prefix
            code[len(prefix):len(prefix)+len(body)] = body
            if name == 'normalize':
                code[2048:2064] = raw[0x16fb440-0x400000:0x16fb450-0x400000]
            assembly = subprocess.run(['objdump', '-d', '--insn-width=16',
                f'--start-address={start}', f'--stop-address={end}', str(binary)],
                check=True, capture_output=True, text=True, timeout=5).stdout
            covered, cursor, constants, relocations = 0, 1024, {}, []
            for line in assembly.splitlines():
                match = re.match(r'^\s*([0-9a-f]+):\s+((?:[0-9a-f]{2}\s+)+)\s*(.*)$', line)
                if not match:
                    continue
                pc, instruction, text = int(match[1], 16), bytes.fromhex(match[2]), match[3]
                if pc != start+covered or instruction != raw[pc-0x400000:pc-0x400000+len(instruction)]:
                    raise ValueError('Arithmetic disassembly coverage differs')
                covered += len(instruction)
                if re.match(r'(call|j\w+|syscall|int)\b', text) or (
                        text.startswith('ret') and covered != end-start):
                    raise ValueError('Unexpected arithmetic control transfer')
                if '(%rip)' in text:
                    target = int(re.search(r'#\s+([0-9a-f]+)', text)[1], 16)
                    # pshufd carries an immediate byte after its RIP displacement.
                    displacement = len(instruction)-(5 if text.startswith('pshufd') else 4)
                    if pc+len(instruction)+struct.unpack_from('<i', instruction, displacement)[0] != target:
                        raise ValueError('Unexpected RIP operand layout')
                    if not 0x1680000 <= target < 0x1700000:
                        raise ValueError('Arithmetic must read only inspected rodata')
                    if target not in constants:
                        constants[target] = cursor
                        code[cursor:cursor+16] = raw[target-0x400000:target-0x400000+16]
                        cursor += 16
                    at = len(prefix)+pc-start
                    struct.pack_into('<i', code, at+displacement,
                                     constants[target]-(at+len(instruction)))
                    relocations.append(dict(instruction=hex(pc), constant=hex(target),
                        sha256=hashlib.sha256(raw[target-0x400000:target-0x400000+16]).hexdigest()))
            if covered != end-start or body[-1] != 0xc3 or cursor > len(code):
                raise ValueError('Invalid bounded arithmetic range')
            memory = mmap.mmap(-1, len(code), prot=mmap.PROT_READ | mmap.PROT_WRITE)
            memory.write(code)
            address = C.addressof(C.c_char.from_buffer(memory))
            libc.mprotect.argtypes = [C.c_void_p, C.c_size_t, C.c_int]
            libc.mprotect.restype = C.c_int
            if libc.mprotect(address, len(code), mmap.PROT_READ | mmap.PROT_EXEC):
                memory.close()
                raise OSError(C.get_errno(), 'mprotect RX failed')
            self.mappings.append(memory)
            self.functions[name] = C.CFUNCTYPE(None, *([C.c_void_p]*argc))(address)
            self.provenance.append(dict(name=name, start=hex(start), end_exclusive=hex(end),
                sha256=hashlib.sha256(body).hexdigest(), relocations=relocations,
                prefix_inherited_constant=None if name != 'normalize' else dict(address='0x16fb440',
                    sha256=hashlib.sha256(raw[0x16fb440-0x400000:0x16fb450-0x400000]).hexdigest())))

    def call(self, name, *inputs):
        import ctypes as C
        if any(len(v) != 4 or any(not finite(x) or abs(x)>100 for x in v) for v in inputs):
            raise ValueError('Arithmetic input outside finite bounded float4 contract')
        buffers = [C.create_string_buffer(31) for _ in range(len(inputs)+1)]
        addresses = [(C.addressof(b)+15)&~15 for b in buffers]
        for address, values in zip(addresses[1:], inputs):
            C.memmove(address, struct.pack('<4f', *values), 16)
        self.functions[name](*addresses)
        result = list(struct.unpack('<4f', C.string_at(addresses[0], 16)))
        if not all(finite(v) for v in result):
            raise ValueError('Nonfinite arithmetic replay result')
        return result


def arithmetic_worker(binary, quaternions):
    import resource
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    resource.setrlimit(resource.RLIMIT_CPU, (5, 5))
    if not isinstance(quaternions, list) or len(quaternions) > 32:
        raise ValueError('At most32 arithmetic cases')
    oracle = NativeRootArithmetic(binary)
    # Exact integer-valued cases verify operand order and rotation handedness.
    product = oracle.call('product', [1,0,0,0], [0,1,0,0])
    rotated = oracle.call('rotate', [0,1,0,0], [1,0,2,0])
    normalized = oracle.call('normalize', [0,0,0,2])
    if product != [0,0,1,0] or rotated != [-1,0,-2,0] or normalized != [0,0,0,1]:
        raise ValueError('Native arithmetic operand order differs from traced algebra')
    results = []
    for q in quaternions:
        if len(q) != 4 or any(not finite(v) for v in q) or abs(sum(v*v for v in q)-1)>1e-4:
            raise ValueError('Expected normalized source quaternion')
        projected = oracle.call('project', q)
        inverse = [-projected[0], -projected[1], -projected[2], projected[3]]
        adjustment = oracle.call('normalize', oracle.call('product', [0,0,0,1], inverse))
        basis = oracle.call('normalize', oracle.call('product', adjustment, projected))
        conjugate = [-basis[0], -basis[1], -basis[2], basis[3]]
        vectors = [list(struct.unpack('<4f', struct.pack('<4f', *v))) for v in
                   [[1,0,0,0], [0,0,1,0], [-.35,0,1.4,0]]]
        rotated = [oracle.call('rotate', conjugate, v) for v in vectors]
        results.append(dict(start_q=q, projected_q=projected, baked_basis_q=basis,
            maximum_planar_vector_error=max(math.dist(a,b) for a,b in zip(vectors,rotated)),
            rotated_vectors=rotated))
    return dict(scope='Call-free arithmetic only: source projection/product/normalize/rotate; no Animator or ELF initialization',
                routines=oracle.provenance,
                operand_order_check=dict(product=product, rotation=rotated, normalization=normalized),
                cases=results)


def replay_arithmetic(binary, quaternions):
    result = subprocess.run([sys.executable, __file__, '--arithmetic-worker', str(binary)],
        input=json.dumps(quaternions, allow_nan=False), capture_output=True, text=True, timeout=15)
    if result.returncode:
        raise ValueError('Bounded arithmetic child failed: '+result.stderr[-2000:])
    return json.loads(result.stdout)


def decode(clip, avatar):
    """Bounded scalar decoder; deliberately rejects other importer branches."""
    m = clip['m_MuscleClip']
    if any(type(m.get(k)) is not bool or m[k] != v for k, v in FLAGS.items()):
        raise ValueError('Unsupported root-extraction flags')
    if any(not finite(m.get(k)) or m[k] != 0 for k in
           ('m_StartTime', 'm_CycleOffset', 'm_OrientationOffsetY', 'm_Level')):
        raise ValueError('Unsupported source offsets')
    if clip.get('m_Legacy') is not False or clip.get('m_Compressed') is not False:
        raise ValueError('Unsupported clip encoding')
    if any(clip.get(k) for k in ('m_PositionCurves', 'm_RotationCurves',
                               'm_CompressedRotationCurves', 'm_ScaleCurves', 'm_PPtrCurves')):
        raise ValueError('Unexpected ordinary transform curves')
    start = m['m_StartX']
    if (any(not finite(start[key].get(axis)) for key in ('t', 'q', 's') for axis in 'xyzw') or
            any(start['s'][axis] != 1 for axis in 'xyzw') or start['t']['w'] != 0 or
            abs(sum(start['q'][axis]**2 for axis in 'xyzw')-1) > 1e-4):
        raise ValueError('Unsupported start transform: require finite normalized quaternion and unit scale')
    ch = packed_channels(clip, avatar)
    human = [b for b in ch['bindings'] if b['kind'] == 'humanoid_scalar_unretargeted']
    attrs = [b['binding']['attribute'] for b in human]
    if (len(attrs) != 127 or set(attrs) != set(range(7, 134)) or
            any(b['kind'] != 'humanoid_scalar_unretargeted' and
                (b['kind'] != 'direct_transform' or not b['binding']['path']) for b in ch['bindings'])):
        raise ValueError('Require exact Root/goal/muscle binding closure, no Motion channels')
    indices = m['m_IndexArray']
    if (not isinstance(indices, list) or len(indices) != 155 or
            any(type(v) is not int for v in indices) or indices[:7] != [-1]*7 or
            any(indices[b['binding']['attribute']] != b['scalar_offset'] for b in human)):
        raise ValueError('Native index table does not match decoded scalar binding layout')
    if (not finite(ch['sample_rate']) or ch['sample_rate'] != 30 or
            not finite(ch['begin_time']) or abs(ch['begin_time']) > 1e-6 or
            not finite(ch['stop_time']) or not 0 < ch['stop_time'] <= 10 or
            not 2 <= ch['frame_count'] <= 302 or
            ch['begin_time']+(ch['frame_count']-1)/ch['sample_rate'] < ch['stop_time']):
        raise ValueError('Unsupported dense clock or missing terminal bracket')
    scale = avatar['m_Avatar']['m_Human']['data']['m_Scale']
    if not finite(scale) or not 0 < scale <= 10:
        raise ValueError('Invalid Avatar human scale')
    roots = {b['binding']['attribute']: [v[0] for v in b['samples']]
             for b in human if b['binding']['attribute'] < 14}
    return dict(name=clip['m_Name'], duration=ch['stop_time'], rate=ch['sample_rate'],
                begin=ch['begin_time'], count=ch['frame_count'], scale=scale, roots=roots,
                start_transform=start, root_scalar_indices=indices[7:14])


def scalar(curve, attribute, seconds):
    if not finite(seconds) or not 0 <= seconds <= curve['duration']:
        raise ValueError('Authored time outside clip')
    values = curve['roots'][attribute]
    if len(values) == 1:
        return values[0]
    phase = min(max((seconds-curve['begin'])*curve['rate'], 0), curve['count']-1)
    i = int(phase)
    j = min(i+1, curve['count']-1)
    return values[i]*(1-(phase-i))+values[j]*(phase-i)


def candidate(curve, seconds):
    # Source RootT body input, projected XZ; no raw RootQ rotation is applied.
    # This is NOT a bone/palette-derived trajectory or an integrated seek delta.
    return [(scalar(curve, k, seconds)-scalar(curve, k, 0))*curve['scale'] if k else 0
            for k in (7, 0, 9)]


def compare(curve, samples):
    if len(samples) < 2:
        raise ValueError('Missing sequential samples')
    previous = -1
    errors = []
    for s in samples:
        t, pos = s['playableTime'], s['position']
        if (not finite(t) or t < 0 or t <= previous or len(pos) != 3 or
                any(not finite(x) for x in pos)):
            raise ValueError('Invalid sample clock/position')
        previous = t
        if t <= curve['duration']:
            predicted = candidate(curve, t)
            errors.append((math.dist(predicted, pos), t, predicted, pos))
    if samples[0]['playableTime'] != 0 or samples[-1]['playableTime'] <= curve['duration']:
        raise ValueError('Missing origin or measured terminal crossing')
    maximum = max(errors)
    before, after = next((a, b) for a, b in zip(samples, samples[1:])
                         if a['playableTime'] <= curve['duration'] < b['playableTime'])
    fraction = ((curve['duration']-before['playableTime']) /
                (after['playableTime']-before['playableTime']))
    endpoint = [a+(b-a)*fraction for a, b in zip(before['position'], after['position'])]
    predicted_end = candidate(curve, curve['duration'])
    return dict(compared_samples=len(errors), maximum_error_m=maximum[0],
                at_authored_seconds=maximum[1], candidate_xyz=maximum[2], observed_xyz=maximum[3],
                endpoint_interpolation_error_m=math.dist(predicted_end, endpoint),
                candidate_endpoint_xyz=predicted_end, interpolated_observed_endpoint_xyz=endpoint,
                endpoint_bracket_seconds=[before['playableTime'], after['playableTime']])


def native_evidence(binary):
    if digest(binary) != ELF_SHA:
        raise ValueError('Installed ELF differs from inspected branch evidence')
    raw = Path(binary).read_bytes()
    names = [(0x77b, 0x16fbbfc, 'm_LoopBlendOrientation'),
             (0x77c, 0x16fbc13, 'm_LoopBlendPositionY'),
             (0x77d, 0x16fbc28, 'm_LoopBlendPositionXZ'),
             (0x77e, 0x16fbc3e, 'm_KeepOriginalOrientation'),
             (0x77f, 0x16fbc58, 'm_KeepOriginalPositionY'),
             (0x780, 0x16fbc70, 'm_KeepOriginalPositionXZ')]
    for _, address, name in names:
        if raw[address-0x400000:address-0x400000+len(name)+1] != name.encode()+b'\0':
            raise ValueError('Native serialized field-name evidence differs')
    return dict(elf_sha256=ELF_SHA,
                ranges=[dict(begin=hex(a), end_exclusive=hex(b),
                             sha256=hashlib.sha256(raw[a-0x400000:b-0x400000]).hexdigest()) for a, b in RANGES],
                clip_flag_offsets={name: hex(offset) for offset, _, name in names},
                y_selection_mask=list(struct.unpack_from('<4I', raw, 0x16fc7b0-0x400000)),
                root_translation_indices=[dict(attribute=i, index_offset=hex(0x4f8+4*i)) for i in (7,8,9)],
                evidence='Previous RootT is sampled at c22887 and current RootT read at c27310. The no-Motion path cancels StartX yaw under KeepOriginalOrientation, bakes orientation/Y, retains XZ, and c24ccb subtracts previous from current. c24b7b divides displacement by its interval; c1f830 reconstructs displacement. bc6e10..bc6e48 applies Human+0x1c0 and actor scale; bc6eef..bc6fff applies actor basis once and stores Animator+0x1c0..0x1c8. Registered deltaPosition getter c51ec0/bc4920 returns those fields.',
                bounded_scope='One unblended, unmirrored humanoid clip, no Motion0–6, exact listed importer flags, unit clip/actor scales, nonloop playback. Controller layer aggregation, transitions, matching/warping and motor/collision are outside this proof.',
                remaining_static_link=None)


def verify(game, capture):
    asset = game/'PE_Client_Data/sharedassets1.assets'
    if digest(asset) != ASSET_SHA or digest(capture) != CAPTURE_SHA:
        raise ValueError('Require pinned installed asset and complete original root06 oracle')
    native = native_evidence(game/'PE_Client.x86_64')
    oracle = analyze(capture, require_motion=True)
    if not oracle.get('clone_motion_qualified') or not oracle.get('all_authored_gaps_within_60hz'):
        raise ValueError('Root oracle did not pass unchanged original qualification')
    import UnityPy
    env = UnityPy.load(str(asset))
    file = next(iter(env.files.values()))
    avatar = file.objects[777].read_typetree()
    if avatar['m_Name'] != 'FemaleAvatar':
        raise ValueError('Avatar identity changed')
    curves, clips = {}, []
    for id, name in CLIPS.items():
        clip = file.objects[id].read_typetree()
        if clip['m_Name'] != name:
            raise ValueError('Clip identity changed')
        curve = curves[id] = decode(clip, avatar)
        events = [e['time'] for e in clip['m_Events'] if e['data'] == ('EndAction' if id == 408 else 'TP_EndAction')]
        if len(events) != 1:
            raise ValueError('Expected one source action-end event')
        clips.append(dict(id=id, name=name, duration=curve['duration'], dense_frames=curve['count'],
                          rate=curve['rate'], begin=curve['begin'], flags=FLAGS,
                          start_transform=curve['start_transform'],
                          root_scalar_indices=curve['root_scalar_indices'],
                          end_action=events[0], candidate_at_action_end=candidate(curve, events[0]),
                          candidate_at_clip_end=candidate(curve, curve['duration']),
                          raw_root_ranges={k: [min(v), max(v)] for k, v in curve['roots'].items()}))
    rows = [json.loads(line) for line in Path(capture).read_text().splitlines()]
    comparisons = [dict(trial=i, **compare(curves[408], [r for r in rows if r.get('type') == 'sample' and r['trial'] == i])) for i in (0, 1)]
    source_quaternions = [[curves[i]['start_transform']['q'][k] for k in 'xyzw'] for i in CLIPS]
    synthetic_quaternions = [[0,math.sin(a/2),0,math.cos(a/2)] for a in (-3,-2,-1,0,1,2,3)]
    arithmetic = replay_arithmetic(game/'PE_Client.x86_64', source_quaternions+synthetic_quaternions)
    arithmetic['source_case_clip_ids'] = list(CLIPS)
    arithmetic['synthetic_case_yaw_radians'] = [-3,-2,-1,0,1,2,3]
    arithmetic['maximum_measured_basis_residual_m'] = max(c['maximum_planar_vector_error'] for c in arithmetic['cases'])
    arithmetic['interpretation'] = ('Real-algebra cancellation is established by the static branch chain; '
        'float normalization/product can leave a measured residual, reported without a tunable tolerance. '
        'The source-channel formula is not asserted bit-identical to those operations.')
    return dict(schema='pe-player-root-channel-verification-1', source_asset=str(asset),
                asset_sha256=ASSET_SHA, capture=str(capture), capture_sha256=CAPTURE_SHA,
                verifier_sha256=digest(__file__),
                dependency_sha256={name: digest(Path(__file__).with_name(name)) for name in
                    ('export_reference_player.py', 'analyze_reference_root_motion.py')}, native=native, avatar_id=777,
                human_scale=curves[408]['scale'], clips=clips, running_comparisons=comparisons,
                isolated_arithmetic=arithmetic,
                running_oracle_qualified=True, ordinary_clips_observed_sequentially=False,
                ordinary_source_extraction_qualified=True,
                ordinary_runtime_motion_qualified=False,
                formula='(linearly_sampled_RootT.xz(t)-RootT.xz(0))*Avatar.Human.m_Scale; Y=0',
                scope='All four ordinary clips pass the exact bounded source-data predicate and static unblended source-to-delta chain. Starting-basis cancellation is independently replayed with call-free native arithmetic. This qualifies source extraction, not observed ordinary playback or runtime integration, and creates no runtime asset.',
                limitations=['No layer/state-transition/root blend or source motor/collision evaluation.',
                             'Double-precision dense interpolation is diagnostic, not bit-identical Unity arithmetic.',
                             'Raw RootQ is a humanoid body input. Same sampled root quaternion is selected by bake-orientation branch; do not apply body yaw a second time.',
                             'No motion is inferred from root-removed meshes or paused-seek deltaPosition.'])


def main():
    if len(sys.argv) == 3 and sys.argv[1] == '--arithmetic-worker':
        request = sys.stdin.read(16385)
        if len(request) > 16384:
            raise ValueError('Oversized arithmetic request')
        print(json.dumps(arithmetic_worker(Path(sys.argv[2]), json.loads(request)), allow_nan=False))
        return 0
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--game-dir', type=Path, default=Path.home()/'.local/share/Steam/steamapps/common/Planet Explorers')
    p.add_argument('--capture', type=Path, default=Path.home()/'.local/share/planet-explorers-strafe-reference/pose-captures/female-running-root-06.jsonl')
    p.add_argument('--output', type=Path, required=True)
    args = p.parse_args()
    game, output = args.game_dir.resolve(), args.output.resolve()
    if output.exists() or output == game or game in output.parents or ROOT/'rebuild/build/reference-assets' in output.parents:
        p.error('Output must be fresh and outside installed game and runtime assets')
    try:
        report = verify(game, args.capture.resolve())
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(json.dumps(report, indent=2, allow_nan=False)+'\n')
    except (OSError, ValueError, KeyError, ImportError) as e:
        p.error(str(e))
    print(output)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
