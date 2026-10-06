#!/usr/bin/env python3
"""Replay only SHA-pinned, call-free Unity scalar-curve arithmetic on Linux x86-64.

Never loads the ELF as a library or executes its entrypoint. Native bytes are read
from the user's installed binary, not distributed. A private mapping transitions
RW -> RX before execution. The CLI runs the bounded worker in a subprocess.
"""
import argparse
import ctypes as C
import hashlib
import json
import mmap
from pathlib import Path
import platform
import struct
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
SHA = '1eb5ad6ff19dddb960a71453603ed7c67403403186c9dfeadc798a6021c4d2a6'
BASE = 0x400000


def f32(x):
    return struct.unpack('<f', struct.pack('<f', x))[0]


def bits(x):
    return struct.unpack('<I', struct.pack('<f', x))[0]


def from_bits(x):
    return struct.unpack('<f', struct.pack('<I', x))[0]


def worker(args):
    if sys.platform != 'linux' or platform.machine() != 'x86_64' or C.sizeof(C.c_void_p) != 8:
        raise ValueError('This pinned arithmetic replay requires Linux x86-64 SysV ABI')
    data = args.binary.read_bytes()
    if hashlib.sha256(data).hexdigest() != SHA:
        raise ValueError('Installed binary SHA mismatch; refusing executable mapping')
    meta_bytes = args.metadata.read_bytes()
    keys = json.loads(meta_bytes)['binding']['curve']['m_Curve']
    expected_times = [0.0, 0.09999999403953552, 0.25, 0.75, 0.9995110630989075]
    if [k['time'] for k in keys] != expected_times:
        raise ValueError('Replay is bounded to the inspected five-key sword curve')
    code = bytearray(mmap.PAGESIZE)

    def copy(address, end, offset):
        body = data[address-BASE:end-BASE]
        code[offset:offset+len(body)] = body
        return {'virtual_start': hex(address), 'virtual_end_exclusive': hex(end),
                'sha256': hashlib.sha256(body).hexdigest(), 'bytes': len(body)}

    provenance = {'coefficients': copy(0x6e2a00, 0x6e2b02, 0),
                  'segment_selector': copy(0x6e2b10, 0x6e2c9b, 0x200)}
    # All four RIP-relative reads in the coefficient routine. No external calls.
    relocations = [(0x6e2a18, 0x1683bdc, 0x800),
                   (0x6e2a3c, 0x1686060, 0x804),
                   (0x6e2ac5, 0x1686050, 0x808),
                   (0x6e2af8, 0x1686050, 0x808)]
    for displacement, source, destination in relocations:
        old = struct.unpack_from('<i', data, displacement-BASE)[0]
        if displacement + 4 + old != source:
            raise ValueError('Unexpected coefficient relocation')
        relative = displacement - 0x6e2a00
        struct.pack_into('<i', code, relative, destination - (relative + 4))
        code[destination:destination+4] = data[source-BASE:source-BASE+4]
    # ABI shim: preserve RBX; cache=RDI -> RBX, local time=XMM0 -> XMM2.
    # Original call-free Horner bytes; return XMM1 via SysV float result XMM0.
    prologue = bytes.fromhex('53 48 89 fb 0f 28 d0')
    code[0x400:0x400+len(prologue)] = prologue
    provenance['horner'] = copy(0x6e3c24, 0x6e3c44, 0x400+len(prologue))
    end = 0x400+len(prologue)+0x20
    code[end:end+5] = bytes.fromhex('0f 28 c1 5b c3')
    memory = mmap.mmap(-1, len(code), prot=mmap.PROT_READ | mmap.PROT_WRITE)
    memory.write(code)
    address = C.addressof(C.c_char.from_buffer(memory))
    libc = C.CDLL(None, use_errno=True)
    libc.mprotect.argtypes = [C.c_void_p, C.c_size_t, C.c_int]
    libc.mprotect.restype = C.c_int
    if libc.mprotect(address, len(code), mmap.PROT_READ | mmap.PROT_EXEC):
        raise OSError(C.get_errno(), 'mprotect RX failed')
    coefficient = C.CFUNCTYPE(None, C.c_void_p, C.c_void_p, C.c_int, C.c_int, C.c_float)(address)
    selector = C.CFUNCTYPE(None, C.c_void_p, C.c_void_p, C.c_void_p, C.c_void_p, C.c_float)(address+0x200)
    horner = C.CFUNCTYPE(C.c_float, C.c_void_p, C.c_float)(address+0x400)
    packed = b''.join(struct.pack('<ffff', k['time'], k['value'], k['inSlope'], k['outSlope']) for k in keys)
    key_storage = C.create_string_buffer(packed)
    state = C.create_string_buffer(0x60)
    struct.pack_into('<Q', state, 0x38, C.addressof(key_storage))
    struct.pack_into('<Q', state, 0x48, len(keys))
    struct.pack_into('<ii', state, 0x58, 2, 2)

    def native(t):
        # Clamp dispatch is from static disassembly; only interior arithmetic
        # and segment selection execute original bytes in this bounded replay.
        if t >= keys[-1]['time']:
            return keys[-1]['value'], None
        struct.pack_into('<i', state, 0, -1)  # Fresh selector cache each query.
        start, finish = C.c_int(-1), C.c_int(-1)
        selector(state, state, C.byref(start), C.byref(finish), t)
        if not (0 <= start.value < finish.value < len(keys)):
            raise ValueError('Native segment selection escaped bounded input')
        coefficient(state, state, start, finish, 0.0)
        return horner(state, f32(t-keys[start.value]['time'])), start.value

    compare = None
    if args.c_library:
        library = C.CDLL(str(args.c_library.resolve()))
        compare = library.compare_curve
        compare.argtypes = [C.c_float, C.POINTER(C.c_float)]
        compare.restype = C.c_bool
    golden = {f32(i/16) for i in range(17)}
    for key in keys:
        b = bits(key['time'])
        golden.add(key['time'])
        if b:
            golden.add(from_bits(b-1))
        golden.add(from_bits(b+1))
    golden.add(from_bits(bits(1.0)-1))
    points = sorted(golden | {f32(i/4096) for i in range(4097)})
    digest, records, mismatches = hashlib.sha256(), [], []
    max_error = 0.0
    for t in points:
        value, segment = native(t)
        digest.update(struct.pack('<ff', t, value))
        if t in golden:
            records.append({'time': t, 'time_bits': f'{bits(t):08x}', 'value': value,
                            'value_bits': f'{bits(value):08x}', 'segment': segment,
                            'method': 'static-clamp' if segment is None else 'original-arithmetic'})
        if compare:
            actual = C.c_float()
            if not compare(t, C.byref(actual)):
                raise ValueError(f'C evaluator rejected bounded point {t}')
            max_error = max(max_error, abs(actual.value-value))
            if bits(actual.value) != bits(value):
                mismatches.append({'time': t, 'native': value, 'c': actual.value,
                                   'native_bits': f'{bits(value):08x}', 'c_bits': f'{bits(actual.value):08x}'})
    result = {'schema': 'pe-unity5-curve-arithmetic-1', 'binary_sha256': SHA,
              'metadata_sha256': hashlib.sha256(meta_bytes).hexdigest(),
              'executed': 'Only copied call-free selector/coefficient/Horner routines; no ELF or engine initialization',
              'tail': 'Static-disassembly clamp dispatch, not executed native clamp code',
              'routines': provenance, 'samples': len(points), 'sample_pairs_sha256': digest.hexdigest(),
              'golden': records, 'comparison': None if not compare else {
                  'bitwise_equal': not mismatches, 'mismatch_count': len(mismatches),
                  'maximum_absolute_error': max_error, 'first_mismatches': mismatches[:16]}}
    print(json.dumps(result, indent=2))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', type=Path, required=True)
    parser.add_argument('--metadata', type=Path, default=ROOT/'rebuild/generated/pe_attack_playback_source.json')
    parser.add_argument('--c-library', type=Path, help='Optional library exporting bool compare_curve(float,float*)')
    parser.add_argument('--worker', action='store_true', help=argparse.SUPPRESS)
    args = parser.parse_args()
    if args.worker:
        worker(args)
    else:
        result = subprocess.run([sys.executable, __file__, *sys.argv[1:], '--worker'],
                                capture_output=True, text=True, timeout=15)
        sys.stdout.write(result.stdout)
        sys.stderr.write(result.stderr)
        raise SystemExit(result.returncode)


if __name__ == '__main__':
    main()
