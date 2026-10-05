#!/usr/bin/env python3
"""Accumulate actual loaded LOD0 terrain while an operator walks the original.

Default is an offline build. --launch uses the isolated original launch path.
Create the printed .arm file after loading the intended archive. The probe
observes only existing chunks; it never generates terrain, moves the player,
injects input, edits voxels or saves. Each chunk must match two consecutive
frames before its first capture. Files in OUTPUT.chunks survive incomplete
coverage and record independent capture frames: this is not a simultaneous
whole-world snapshot. A complete rectangle is emitted only with full coverage.
"""
import argparse
import os
from pathlib import Path
import subprocess
import sys
import reference_pose

BASE_BUILD = reference_pose.build


def build(game_dir, build_dir=None):
    build_dir = build_dir or reference_pose.TOOLS.parent / 'build' / 'terrain-route-probe'
    compiler_tmp = reference_pose.TOOLS.parent / 'build' / 'compiler-tmp'
    compiler_tmp.mkdir(parents=True, exist_ok=True)
    os.environ['TMPDIR'] = str(compiler_tmp.resolve())
    mono, shim, _ = BASE_BUILD(game_dir, build_dir)
    managed = game_dir / 'PE_Client_Data/Managed'
    assembly = (build_dir / 'ReferenceTerrainRouteProbe.dll').resolve()
    subprocess.run([str(build_dir / 'compiler'), str(mono), str(managed), '-noconfig', '-nostdlib',
                    '-target:library', f'-r:{managed / "mscorlib.dll"}',
                    f'-r:{managed / "UnityEngine.dll"}', f'-r:{managed / "Assembly-CSharp.dll"}',
                    f'-out:{assembly}', str(reference_pose.TOOLS / 'ReferenceTerrainRouteProbe.cs')], check=True)
    return mono, shim, assembly


def main(argv=None):
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument('--origin', type=int, nargs=3, default=(-640, 0, 64))
    parser.add_argument('--size', type=int, nargs=3, default=(416, 64, 416))
    parser.add_argument('--capture-seconds', type=int, default=1800)
    args, remaining = parser.parse_known_args(argv)
    if any(s < 2 or s > 512 for s in args.size) or args.size[0]*args.size[1]*args.size[2] > 16777216:
        parser.error('size must be 2..512 per axis and at most 16,777,216 samples')
    if any(abs(n) > 1000000 for n in args.origin):
        parser.error('origin must be within +/-1,000,000')
    if not 1 <= args.capture_seconds <= 1800:
        parser.error('capture-seconds must be 1..1800')
    os.environ['PE_TERRAIN_ORIGIN'] = ','.join(map(str, args.origin))
    os.environ['PE_TERRAIN_SIZE'] = ','.join(map(str, args.size))
    os.environ['PE_TERRAIN_SECONDS'] = str(args.capture_seconds)
    reference_pose.build = build
    reference_pose.__doc__ = __doc__ + '\nRoute flags: --origin X Y Z --size X Y Z --capture-seconds 1..1800.'
    return reference_pose.main(remaining)


if __name__ == '__main__':
    sys.exit(main())
