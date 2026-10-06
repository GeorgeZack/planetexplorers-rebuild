#!/usr/bin/env python3
"""Bounded observation of the existing Town Defense9077 beacon; offline compile by default.

--launch starts a separate original with a launch-only shim. Never attaches to
an existing process. The operator arms after loading and supplies all inputs.
The probe reads gameplay; loading a component does affect process/performance.
"""
import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import launch_reference
import reference_pose

TOOLS = Path(__file__).resolve().parent
BUILD = TOOLS.parent / 'build/town-defense-probe'
SOURCE = TOOLS / 'ReferenceTownDefenseProbe.cs'


def digest(path):
    with Path(path).open('rb') as f:
        return hashlib.file_digest(f, 'sha256').hexdigest()


def build(game, directory=BUILD):
    temporary = TOOLS.parent / 'build/compiler-tmp'
    temporary.mkdir(parents=True, exist_ok=True)
    os.environ['TMPDIR'] = str(temporary.resolve())
    mono, shim, _ = reference_pose.build(game, directory)
    managed = game / 'PE_Client_Data/Managed'
    dll = (directory / 'ReferenceTownDefenseProbe.dll').resolve()
    subprocess.run([str(directory/'compiler'), str(mono), str(managed), '-noconfig', '-nostdlib', '-target:library',
                    f'-r:{managed/"mscorlib.dll"}', f'-r:{managed/"UnityEngine.dll"}',
                    f'-r:{managed/"Assembly-CSharp.dll"}', f'-out:{dll}', str(SOURCE)], check=True)
    return mono, shim, dll


def paths(profile, output):
    capture, arm, stop, manifest = reference_pose.capture_paths(profile, output)
    exit_path = capture.with_suffix('.exit.json')
    if any(p.exists() or p.is_symlink() for p in (capture, arm, stop, manifest, exit_path)):
        raise ValueError('Capture/sidecar paths must be fresh')
    return capture, arm, stop, manifest, exit_path


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--game-dir', type=Path, default=Path.home()/'.local/share/Steam/steamapps/common/Planet Explorers')
    ap.add_argument('--user-data', type=Path, default=Path.home()/'.local/share/planet-explorers-hunt-reference')
    ap.add_argument('--launch', action='store_true')
    ap.add_argument('--single-threaded-rendering', action='store_true')
    ap.add_argument('--seconds', type=int, choices=range(1,121), default=120, metavar='1..120')
    ap.add_argument('--frames', type=int, choices=range(1,3601), default=3600, metavar='1..3600')
    ap.add_argument('--wait-seconds', type=int, choices=range(1,1801), default=900, metavar='1..1800')
    ap.add_argument('--output', type=Path)
    a = ap.parse_args(argv)
    try:
        game = a.game_dir.expanduser().resolve(); profile = a.user_data.expanduser().resolve()
        output, arm, stop, manifest, exit_path = paths(profile, a.output.expanduser() if a.output else None)
        command, env = launch_reference.launch_command(game, profile, a.single_threaded_rendering)
        if a.launch and env.get('LD_PRELOAD'):
            raise ValueError('Existing preload refused')
        mono, shim, dll = build(game)
        if not a.launch:
            print(f'Offline observer built: {dll}\nNo game launched or profile settings changed.'); return 0
        if any(c.isspace() or c == ':' for c in str(shim)):
            raise ValueError('Invalid shim path')
        source_hash = digest(SOURCE)
        env.update(LD_PRELOAD=str(shim), PE_POSE_EXECUTABLE=str(Path(command[0]).resolve()), PE_POSE_MONO=str(mono),
                   PE_POSE_ASSEMBLY=str(dll), PE_POSE_OUTPUT=str(output), PE_POSE_ARM=str(arm), PE_POSE_STOP=str(stop),
                   PE_POSE_WAIT=str(a.wait_seconds), PE_POSE_FRAMES=str(a.frames), PE_POSE_SECONDS=str(a.seconds),
                   PE_TOWN_DEFENSE_PROBE_SHA256=source_hash)
        launch_reference.prepare_settings(profile); launch_reference.prepare_unity_preferences(profile/'engine-config')
        output.parent.mkdir(parents=True, exist_ok=True)
        with manifest.open('x') as f:
            json.dump({'schema': 'pe-town-defense-launch-1', 'command': command, 'profile': str(profile), 'capture': str(output),
                       'arm': str(arm), 'stop': str(stop), 'frames': a.frames, 'seconds': a.seconds, 'wait_seconds': a.wait_seconds,
                       'assembly_sha256': digest(dll), 'probe_source_sha256': source_hash, 'shim_sha256': digest(shim),
                       'engine_sha256': digest(game/'PE_Client_Data/Managed/UnityEngine.dll'),
                       'game_assembly_sha256': digest(game/'PE_Client_Data/Managed/Assembly-CSharp.dll'),
                       'render_environment': {k: env[k] for k in ('__GLX_VENDOR_LIBRARY_NAME','LIBGL_ALWAYS_SOFTWARE') if k in env}}, f, indent=2)
        print(f'Load preserved manual8 preparation checkpoint, then arm: {arm}\nStop: {stop}\nCapture: {output}', flush=True)
        code = subprocess.run(command, cwd=game, env=env).returncode
        with exit_path.open('x') as f:
            json.dump({'schema': 'pe-town-defense-exit-1', 'returncode': code, 'signal': -code if code < 0 else None,
                       'observed_utc': datetime.now(timezone.utc).isoformat()}, f, indent=2)
        return code if code >= 0 else 128-code
    except (OSError, ValueError, KeyError, TypeError, subprocess.CalledProcessError) as e:
        print(f'Town-defense observer: {e}', file=sys.stderr); return 1


if __name__ == '__main__':
    sys.exit(main())
