#!/usr/bin/env python3
"""Prepare an opt-in original-runtime humanoid pose capture (Linux x86_64).

Default action builds only: no game launch, attachment, input or setting changes.
Requires cc and the legally installed original's libmono, Mono.CSharp and Unity
assemblies. The compiler runs separately; no downloads or SDK installation.

--launch explicitly launches the installed original with an isolated game/XDG
profile and a launch-only LD_PRELOAD shim. No original files are patched. The
shim installs an observation-only MonoBehaviour from PeSingletonRunner.Update
on the Unity main thread. The operator navigates the original, then creates the
printed .arm file; end-of-frame captures stop at frame/time/byte limits or .stop.
No input or attacks are generated. Closing the original unloads the shim; the
capture component destroys itself when done. Files remain for review/deletion.

Output must be inside --user-data; new files are never overwritten. Hooks and
Unity coroutine scheduling require a supervised runtime check before treating
captured poses as validated. Normalized Animator state is recorded, not inferred
from wall time. Read-only means gameplay is unmodified: loading a DLL and adding
one transient observation component do change runtime process state/performance.
"""
import argparse
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import platform
import subprocess
import sys
import uuid
import launch_reference

TOOLS = Path(__file__).resolve().parent
BUILD = TOOLS.parent / 'build' / 'pose-probe'


def build(game_dir, build_dir=BUILD):
    if platform.machine() != 'x86_64' or sys.platform != 'linux':
        raise ValueError('Pose shim supports Linux x86_64 only')
    managed = game_dir / 'PE_Client_Data/Managed'
    mono = (game_dir / 'PE_Client_Data/Mono/x86_64/libmono.so').resolve()
    for path in (mono, managed / 'Mono.CSharp.dll', managed / 'mscorlib.dll', managed / 'UnityEngine.dll'):
        if not path.is_file():
            raise ValueError(f'Missing installed dependency: {path}')
    build_dir.mkdir(parents=True, exist_ok=True)
    compiler = build_dir / 'compiler'
    shim = build_dir / 'pose-shim.so'
    assembly = build_dir / 'ReferencePoseProbe.dll'
    subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror',
                    str(TOOLS / 'reference_pose_compile.c'), '-ldl', '-o', str(compiler)], check=True)
    subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-shared', '-fPIC',
                    str(TOOLS / 'reference_pose_shim.c'), '-ldl', '-o', str(shim)], check=True)
    subprocess.run([str(compiler), str(mono), str(managed), '-noconfig', '-nostdlib',
                    '-target:library', f'-r:{managed / "mscorlib.dll"}',
                    f'-r:{managed / "UnityEngine.dll"}', f'-out:{assembly}',
                    str(TOOLS / 'ReferencePoseProbe.cs')], check=True)
    return mono, shim.resolve(), assembly.resolve()


def capture_paths(user_data, output):
    if any(c.isspace() for c in str(user_data)):
        raise ValueError('Original command line requires a profile path without whitespace')
    if output is None:
        run = datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%SZ') + '-' + uuid.uuid4().hex[:8]
        output = user_data / 'pose-captures' / (run + '.jsonl')
    output = output.resolve()
    if not output.is_relative_to(user_data):
        raise ValueError('--output must be within the isolated --user-data profile')
    paths = (output, output.with_suffix('.arm'), output.with_suffix('.stop'), output.with_suffix('.launch.json'))
    if any(p.exists() for p in paths):
        raise ValueError('Capture/arm/stop/manifest path already exists; choose a fresh output')
    return paths


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--game-dir', type=Path, default=Path.home() / '.local/share/Steam/steamapps/common/Planet Explorers')
    parser.add_argument('--user-data', type=Path, default=Path.home() / '.local/share/planet-explorers-pose-reference')
    parser.add_argument('--output', type=Path, help='fresh JSONL path inside user-data')
    parser.add_argument('--frames', type=int, default=600, choices=range(1, 3601), metavar='1..3600')
    parser.add_argument('--seconds', type=int, default=20, choices=range(1, 121), metavar='1..120')
    parser.add_argument('--wait-seconds', type=int, default=900, choices=range(1, 1801), metavar='1..1800')
    parser.add_argument('--launch', action='store_true', help='explicitly start the original; default is offline build only')
    parser.add_argument('--single-threaded-rendering', action='store_true',
                        help='opt-in Unity -force-gfx-direct diagnostic; not a guaranteed crash fix')
    args = parser.parse_args(argv)
    try:
        game_dir, user_data = args.game_dir.expanduser().resolve(), args.user_data.expanduser().resolve()
        command, environment = launch_reference.launch_command(game_dir, user_data,args.single_threaded_rendering)
        output, arm, stop, manifest = capture_paths(user_data, args.output.expanduser() if args.output else None)
        mono, shim, assembly = build(game_dir)
        if not args.launch:
            print(f'Offline build complete: {assembly}\nShim: {shim}\nNo game launched or profile settings changed.')
            return 0
        if environment.get('LD_PRELOAD'):
            raise ValueError('Existing LD_PRELOAD is set; use a clean explicit launch environment')
        if any(c.isspace() or c == ':' for c in str(shim)):
            raise ValueError('LD_PRELOAD shim path cannot contain whitespace or colon')
        environment.update(LD_PRELOAD=str(shim), PE_POSE_EXECUTABLE=str(Path(command[0]).resolve()),
                           PE_POSE_MONO=str(mono), PE_POSE_ASSEMBLY=str(assembly),
                           PE_POSE_OUTPUT=str(output), PE_POSE_ARM=str(arm), PE_POSE_STOP=str(stop),
                           PE_POSE_FRAMES=str(args.frames), PE_POSE_SECONDS=str(args.seconds),
                           PE_POSE_WAIT=str(args.wait_seconds))
        launch_reference.prepare_settings(user_data)
        launch_reference.prepare_unity_preferences(user_data / 'engine-config')
        output.parent.mkdir(parents=True, exist_ok=True)
        with manifest.open('x', encoding='utf-8') as stream:
            json.dump({'schema': 1, 'command': command, 'profile': str(user_data),
                       'capture': str(output), 'arm': str(arm), 'stop': str(stop),
                       'frames': args.frames, 'seconds': args.seconds, 'wait_seconds': args.wait_seconds,
                       'assembly': str(assembly), 'shim': str(shim), 'mono': str(mono)}, stream, indent=2)
        print(f'Isolated profile: {user_data}\nCapture: {output}\nCreate to arm: {arm}\nCreate to stop: {stop}', flush=True)
        result = subprocess.run(command, cwd=game_dir, env=environment).returncode
        return result if result >= 0 else 128 - result
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        print(f'Pose probe: {error}', file=sys.stderr)
        return 1
    except KeyboardInterrupt:
        return 130


if __name__ == '__main__':
    sys.exit(main())
