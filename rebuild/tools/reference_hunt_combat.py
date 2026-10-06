#!/usr/bin/env python3
"""Observe existing mission9126 combat state through the opt-in main-thread probe.

Default is OFFLINE compilation only. --launch starts a separate original client
using the existing isolated-profile launcher; never attach to a running PID.
Arm after loading the desired checkpoint. No input, spawning, RNG, gameplay
mutation or save calls are made. Agent positions are observed, not generated.
"""
import hashlib
import os
from pathlib import Path
import subprocess
import sys
import reference_pose

BASE_BUILD = reference_pose.build
SOURCE = reference_pose.TOOLS / 'ReferenceHuntCombatProbe.cs'


def build(game_dir, build_dir=None):
    build_dir = build_dir or reference_pose.TOOLS.parent / 'build' / 'hunt-combat-probe'
    temporary = reference_pose.TOOLS.parent / 'build' / 'compiler-tmp'
    temporary.mkdir(parents=True, exist_ok=True)
    os.environ['TMPDIR'] = str(temporary.resolve())
    mono, shim, _ = BASE_BUILD(game_dir, build_dir)
    managed = game_dir / 'PE_Client_Data/Managed'
    assembly = (build_dir / 'ReferenceHuntCombatProbe.dll').resolve()
    subprocess.run([str(build_dir / 'compiler'), str(mono), str(managed), '-noconfig', '-nostdlib',
                    '-target:library', f'-r:{managed / "mscorlib.dll"}',
                    f'-r:{managed / "UnityEngine.dll"}', f'-r:{managed / "Assembly-CSharp.dll"}',
                    f'-out:{assembly}', str(SOURCE)], check=True)
    return mono, shim, assembly


def main(argv=None):
    os.environ['PE_HUNT_COMBAT_PROBE_SHA256'] = hashlib.sha256(SOURCE.read_bytes()).hexdigest()
    reference_pose.build = build
    reference_pose.__doc__ = __doc__
    return reference_pose.main(argv)


if __name__ == '__main__':
    sys.exit(main())
