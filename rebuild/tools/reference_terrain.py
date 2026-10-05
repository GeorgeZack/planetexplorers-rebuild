#!/usr/bin/env python3
"""Capture one loaded original terrain crop, read-only, through the pose shim.

Default builds only. --launch explicitly starts an isolated reference profile;
all launch flags match reference_pose.py. --output should end in .pevx.
Navigate to the archived Adventure settlement, wait for terrain, then create
printed .arm. Requires every LOD0 chunk for (-627,16,323),64x32x128 loaded and
identical volume/type data on two successive end-of-frame samples. Stops after
120 seconds armed or1800 seconds waiting, without generating missing terrain,
input, teleporting, editing voxels, changing gameplay state or calling saves.
Read-only observation adds one transient component and performs allocations/IO.
"""
from pathlib import Path
import os
import subprocess
import sys
import reference_pose

BASE_BUILD = reference_pose.build


def build(game_dir, build_dir=None):
    build_dir = build_dir or reference_pose.TOOLS.parent / 'build' / 'terrain-probe'
    compiler_tmp = reference_pose.TOOLS.parent / 'build' / 'compiler-tmp'
    compiler_tmp.mkdir(parents=True, exist_ok=True)
    os.environ['TMPDIR'] = str(compiler_tmp.resolve())
    mono, shim, _ = BASE_BUILD(game_dir, build_dir)
    managed = game_dir / 'PE_Client_Data/Managed'
    assembly = (build_dir / 'ReferenceTerrainProbe.dll').resolve()
    subprocess.run([str(build_dir / 'compiler'), str(mono), str(managed), '-noconfig', '-nostdlib',
                    '-target:library', f'-r:{managed / "mscorlib.dll"}',
                    f'-r:{managed / "UnityEngine.dll"}', f'-r:{managed / "Assembly-CSharp.dll"}',
                    f'-out:{assembly}', str(reference_pose.TOOLS / 'ReferenceTerrainProbe.cs')], check=True)
    return mono, shim, assembly


def main(argv=None):
    reference_pose.build = build
    reference_pose.__doc__ = __doc__
    return reference_pose.main(argv)


if __name__ == '__main__':
    sys.exit(main())
