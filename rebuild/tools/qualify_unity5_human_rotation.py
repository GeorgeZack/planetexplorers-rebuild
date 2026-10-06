#!/usr/bin/env python3
"""SHA-pinned arithmetic-only Avatar.GetZYRoll oracle, Linux x86-64 only.

Never loads/initializes the ELF, Avatar, Animator or game. Copied arithmetic is
read from the installed binary; no engine bytes are distributed. CLI execution
uses a bounded child. This qualifies a helper, not complete humanoid retargeting.
"""
import argparse
import ctypes as C
import hashlib
import itertools
import json
import math
import mmap
from pathlib import Path
import platform
import re
import struct
import subprocess
import sys

SHA = '1eb5ad6ff19dddb960a71453603ed7c67403403186c9dfeadc798a6021c4d2a6'
BASE = 0x400000
RANGES = [(0xbaf4d6, 0xbaf6d1), (0xbaf703, 0xbaf734)]


class NativeRotation:
    def __init__(self, binary):
        if sys.platform != 'linux' or platform.machine() != 'x86_64' or C.sizeof(C.c_void_p) != 8:
            raise ValueError('Pinned oracle requires Linux x86-64 SysV ABI')
        binary = Path(binary)
        data = binary.read_bytes()
        if hashlib.sha256(data).hexdigest() != SHA:
            raise ValueError('Binary SHA mismatch: refusing executable mapping')
        code = bytearray(mmap.PAGESIZE)
        # RAX=axes, XMM3=muscles, XMM1=min; XMM2/XMM4=zero. RDX=out.
        prologue = bytes.fromhex('48 89 f8 0f 10 1e 0f 10 48 30 0f 57 d2 0f 57 e4')
        code[:len(prologue)] = prologue
        position, const_position, constants = len(prologue), 0x800, {}
        self.provenance = []
        for start, end in RANGES:
            body = data[start-BASE:end-BASE]
            code[position:position+len(body)] = body
            assembly = subprocess.run(['objdump', '-d', '--insn-width=16',
                f'--start-address={start}', f'--stop-address={end}', str(binary)],
                check=True, capture_output=True, text=True, timeout=5).stdout
            relocations = []
            covered = 0
            for line in assembly.splitlines():
                match = re.match(r'^\s*([0-9a-f]+):\s+((?:[0-9a-f]{2}\s+)+)\s*(.*)$', line)
                if not match:
                    continue
                pc, raw, text = int(match[1], 16), bytes.fromhex(match[2]), match[3]
                if pc != start+covered or raw != data[pc-BASE:pc-BASE+len(raw)]:
                    raise ValueError('Disassembler byte coverage mismatch')
                covered += len(raw)
                if re.match(r'(?:call|j\w+|ret|syscall|int)\b', text):
                    raise ValueError('Arithmetic range unexpectedly contains control transfer')
                if '(%rip)' in text:
                    target = int(re.search(r'#\s+([0-9a-f]+)', text)[1], 16)
                    if pc+len(raw)+struct.unpack('<i', raw[-4:])[0] != target:
                        raise ValueError('Unexpected RIP displacement position')
                    if target not in constants:
                        constants[target] = const_position
                        code[const_position:const_position+16] = data[target-BASE:target-BASE+16]
                        const_position += 16
                    new_pc = position+pc-start
                    struct.pack_into('<i', code, new_pc+len(raw)-4,
                                     constants[target]-(new_pc+len(raw)))
                    relocations.append({'instruction': hex(pc), 'constant': hex(target),
                                        'constant_hex': data[target-BASE:target-BASE+16].hex()})
            if covered != end-start or const_position > len(code):
                raise ValueError('Incomplete or oversized arithmetic mapping')
            self.provenance.append({'start': hex(start), 'end_exclusive': hex(end),
                'sha256': hashlib.sha256(body).hexdigest(), 'relocations': relocations})
            position += len(body)
        code[position:position+4] = bytes.fromhex('0f 11 02 c3') # XMM0 -> out; ret.
        self.memory = mmap.mmap(-1, len(code), prot=mmap.PROT_READ | mmap.PROT_WRITE)
        self.memory.write(code)
        address = C.addressof(C.c_char.from_buffer(self.memory))
        libc = C.CDLL(None, use_errno=True)
        libc.mprotect.argtypes = [C.c_void_p, C.c_size_t, C.c_int]
        libc.mprotect.restype = C.c_int
        if libc.mprotect(address, len(code), mmap.PROT_READ | mmap.PROT_EXEC):
            raise OSError(C.get_errno(), 'mprotect RX failed')
        self.function = C.CFUNCTYPE(None, C.c_void_p, C.c_void_p, C.c_void_p)(address)

    def rotation(self, muscles, minimum, maximum, sign):
        vectors = [list(v) for v in [muscles, minimum, maximum, sign]]
        if any(len(v) != 3 or any(not math.isfinite(x) for x in v) for v in vectors):
            raise ValueError('Expected four finite float3 inputs')
        muscles, minimum, maximum, sign = vectors
        if (any(abs(x)>16 for x in muscles) or
            any(abs(x)>100 for x in minimum+maximum) or
            any(a>b for a,b in zip(minimum,maximum)) or any(x not in [-1,1] for x in sign)):
            raise ValueError('Input outside bounded helper contract')
        axes = C.create_string_buffer(96+15)
        aligned = (C.addressof(axes)+15)&~15
        C.memmove(aligned+32, struct.pack('<4f', *sign, 1), 16)
        C.memmove(aligned+48, struct.pack('<4f', *minimum, 0), 16)
        C.memmove(aligned+64, struct.pack('<4f', *maximum, 0), 16)
        values, out = (C.c_float*4)(*muscles, 0), (C.c_float*4)()
        self.function(aligned, values, out)
        if not all(math.isfinite(x) for x in out):
            raise ValueError('Nonfinite native arithmetic result')
        return list(out)


def worker(args):
    oracle = NativeRotation(args.binary)
    library = C.CDLL(str(args.c_library.resolve())) if args.c_library else None
    if library:
        fn = library.pe_human_rotation
        fn.argtypes = [C.POINTER(C.c_float)]*5
        fn.restype = C.c_bool
        library.pe_human_rotation_native_rsqrt.restype = C.c_bool
    axes = [('asymmetric', [-.4,-1.2,-2.4], [.8,1.7,2.8]),
            ('wide', [-4,-3,-2], [4,3,2]),
            ('zero', [0,0,0], [0,0,0]),
            ('one-sided', [.3,-.8,0], [.9,-.2,1])]
    cases = [(name, lo, hi, list(sign)) for name,lo,hi in axes
             for sign in itertools.product([-1,1], repeat=3)]
    rig_provenance = []
    rig = None
    for path in args.rig:
        raw = path.read_bytes()
        rig = json.loads(raw)
        entries = rig['avatar']['m_Avatar']['m_Human']['data']['m_Skeleton']['data']['m_AxesArray']
        for i,a in enumerate(entries):
            cases.append((f'{path.name}:axes{i}', [a['m_Limit']['m_Min'][k] for k in 'xyz'],
                          [a['m_Limit']['m_Max'][k] for k in 'xyz'], [a['m_Sgn'][k] for k in 'xyz']))
        rig_provenance.append({'name':path.name, 'sha256':hashlib.sha256(raw).hexdigest(), 'axes':len(entries)})
    clip_provenance, extrema_points = [], set()
    if args.clip and rig is None:
        raise ValueError('--clip requires an offline --rig for scalar decoding')
    if args.clip:
        from export_reference_player import packed_channels
        for path in args.clip:
            raw = path.read_bytes()
            channels = packed_channels(json.loads(raw), rig['avatar'])
            extrema = []
            for row in channels['bindings']:
                if row['kind']!='humanoid_scalar_unretargeted' or not 42<=row['binding']['attribute']<134:
                    continue
                values = [sample[0] for sample in row['samples']]
                lo, hi = min(values), max(values)
                if not math.isfinite(lo) or not math.isfinite(hi) or lo < -16 or hi > 16:
                    raise ValueError('Actual clip muscle exceeds bounded helper domain')
                extrema.append({'attribute':row['binding']['attribute'],'minimum':lo,'maximum':hi})
                for value in (lo,hi):
                    for axis in range(3):
                        point = [0.,0.,0.];point[axis] = value
                        extrema_points.add(tuple(point))
            clip_provenance.append({'name':path.name,'sha256':hashlib.sha256(raw).hexdigest(),'muscles':extrema})
    digest, count, mismatches, max_error, norm_error = hashlib.sha256(), 0, 0, 0., 0.
    golden, first = [], []
    grid = [-16,-8,-4.1,-2,-1.5,-1,-.5,0,.5,1,1.5,2,4.1,8,16]
    points = sorted(set(itertools.product(grid,repeat=3)) | extrema_points)
    for name,lo,hi,sign in cases:
        for m in points:
            m = list(m)
            expected = oracle.rotation(m,lo,hi,sign)
            count += 1
            digest.update(struct.pack('<16f', *m,*lo,*hi,*sign,*expected))
            norm_error = max(norm_error, abs(sum(v*v for v in expected)-1))
            if name=='asymmetric' and sign==[1,1,1] and m in [[0,0,0],[1,0,0],[0,1,0],[0,0,1],[-1,0,0],[0,-1,0],[0,0,-1],[2,-2,1.5]]:
                golden.append({'muscles':m,'min':lo,'max':hi,'sign':sign,'rotation':expected})
            if library:
                out = (C.c_float*4)(99,99,99,99)
                inputs = [(C.c_float*3)(*v) for v in [m,lo,hi,sign]]
                if not fn(*inputs,out):
                    raise ValueError(f'C helper rejected {name}:{m}')
                error = max(abs(a-b) for a,b in zip(expected,out))
                max_error = max(max_error,error)
                if struct.pack('<4f',*expected)!=bytes(out):
                    mismatches += 1
                    if len(first)<4:
                        first.append({'case':name,'muscles':m,'native':expected,'c':list(out)})
    print(json.dumps({'schema':'pe-unity5-human-rotation-1','binary_sha256':SHA,
        'scope':'GetZYRoll arithmetic only; no full retarget qualification or engine initialization',
        'routines':oracle.provenance,'rigs':rig_provenance,'clips':clip_provenance,
        'muscle_grid':grid,'distinct_muscle_triples':len(points),'cases':len(cases),'samples':count,
        'sample_sha256':digest.hexdigest(),'maximum_native_norm_squared_error':norm_error,
        'golden':golden,'comparison':None if not library else {
            'library_sha256':hashlib.sha256(args.c_library.read_bytes()).hexdigest(),
            'c_source_sha256':hashlib.sha256(Path(__file__).with_name('human_rotation_math.c').read_bytes()).hexdigest(),
            'c_header_sha256':hashlib.sha256(Path(__file__).with_name('human_rotation_math.h').read_bytes()).hexdigest(),
            'native_rsqrt':bool(library.pe_human_rotation_native_rsqrt()),
            'bitwise_mismatches':mismatches,'maximum_component_error':max_error,'first_mismatches':first}},indent=2))


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--binary',type=Path,required=True)
    p.add_argument('--c-library',type=Path)
    p.add_argument('--rig',type=Path,action='append',default=[])
    p.add_argument('--clip',type=Path,action='append',default=[])
    p.add_argument('--worker',action='store_true',help=argparse.SUPPRESS)
    args = p.parse_args()
    if len(args.rig)>2:
        p.error('At most two offline rig fixtures')
    if len(args.clip)>2:
        p.error('At most two offline clip fixtures')
    if args.worker:
        worker(args)
    else:
        result = subprocess.run([sys.executable,__file__,*sys.argv[1:],'--worker'],
                                capture_output=True,text=True,timeout=45)
        sys.stdout.write(result.stdout);sys.stderr.write(result.stderr)
        raise SystemExit(result.returncode)


if __name__=='__main__':
    main()
